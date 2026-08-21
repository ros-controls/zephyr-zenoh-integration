// Copyright 2026 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_wifi.h>
#include <zenbedded_rcl/generated/interface_data.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/stepper/stepper.h>
#include <zephyr/drivers/stepper/stepper_ctrl.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <cmath>
#include <zenbedded_rcl/codecs.hpp>
#include <zenbedded_rcl/zenbedded_client.hpp>

LOG_MODULE_REGISTER(inverted_pendulum_tier2, LOG_LEVEL_INF);

constexpr uint32_t kWifiConnectTimeout = 15000;  // ms
constexpr uint32_t motor_steps_per_rev = 200;
constexpr uint32_t micro_step_res = DT_PROP_OR(DT_ALIAS(stepper_driver), micro_step_res, 1);
constexpr uint32_t micro_steps_per_rev = motor_steps_per_rev * micro_step_res;

const device * stepper_driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver));
const device * stepper_ctrl = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));
const device * encoder_driver = DEVICE_DT_GET(DT_ALIAS(encoder_driver));

void set_stepper_angular_vel(double rads_per_sec)
{
  if (rads_per_sec == 0.0)
  {
    stepper_ctrl_stop(stepper_ctrl);
    return;
  }

  double microsteps_per_sec = fabs(rads_per_sec) * micro_steps_per_rev / (2 * M_PI);

  if (microsteps_per_sec <= 0.0)
  {
    stepper_ctrl_stop(stepper_ctrl);
    return;
  }

  auto interval_ns = static_cast<uint64_t>(1e9 / microsteps_per_sec);

  stepper_ctrl_set_microstep_interval(stepper_ctrl, interval_ns);
  stepper_ctrl_run(
    stepper_ctrl,
    (rads_per_sec > 0.0) ? STEPPER_CTRL_DIRECTION_POSITIVE : STEPPER_CTRL_DIRECTION_NEGATIVE);
}

bool get_encoder_angle_deg(double * angle_rad)
{
  int ret = sensor_sample_fetch(encoder_driver);
  if (ret != 0)
  {
    return false;
  }

  sensor_value val{};
  ret = sensor_channel_get(encoder_driver, SENSOR_CHAN_ROTATION, &val);
  if (ret != 0)
  {
    return false;
  }

  *angle_rad = sensor_value_to_double(&val) * M_PI / 180;
  return true;
}

int main()
{
  if (!device_is_ready(stepper_driver) || !device_is_ready(stepper_ctrl))
  {
    LOG_ERR("Stepper devices not ready\n");
    return 0;
  }

  if (!device_is_ready(encoder_driver))
  {
    LOG_ERR("Encoder device not ready\n");
    return 0;
  }

  LOG_INF("Starting Inverted Pendulum Tier1");
  net_if * iface = net_if_get_default();

  LOG_INF("connecting to WiFi using stored credentials...");

  uint32_t timer = k_uptime_get_32();
  while (net_mgmt(NET_REQUEST_WIFI_CONNECT_STORED, iface, nullptr, 0) != 0)
  {
    if (k_uptime_get_32() - timer > kWifiConnectTimeout)
    {
      LOG_ERR("Wifi Connection Timedout ...");
      return -1;
    }
    LOG_ERR("WiFi connect-stored request failed... retrying...");
    k_sleep(K_MSEC(200));
  }

  LOG_INF("Connected... Waiting for IPV4 address");
  while (net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) == nullptr)
  {
    k_sleep(K_MSEC(200));
  }
  LOG_INF("Got IPV4 address");

  // Disable Wi-Fi power saving
  esp_wifi_set_ps(WIFI_PS_NONE);
  k_sleep(K_MSEC(200));

  static ZenbeddedClient<RawCodec<zenbedded_state_t>, RawCodec<zenbedded_command_t>> client;
  int ret = client.init(50);
  if (ret != 0)
  {
    LOG_ERR("Failed to initialize ZenbeddedClient: %d", ret);
    return ret;
  }

  stepper_enable(stepper_driver);

  int32_t stepper_position;
  double stepper_angle = 0, encoder_angle = 0;
  double stepper_angle_offset = 0, encoder_angle_offset = 0;
  double stepper_angular_velocity = 0;

  stepper_ctrl_get_actual_position(stepper_ctrl, &stepper_position);
  stepper_angle_offset =
    2.0 * M_PI * (stepper_position % micro_steps_per_rev) / micro_steps_per_rev;
  get_encoder_angle_deg(&encoder_angle_offset);

  while (true)
  {
    stepper_ctrl_get_actual_position(stepper_ctrl, &stepper_position);
    stepper_angle = 2.0 * M_PI * (stepper_position % micro_steps_per_rev) / micro_steps_per_rev -
                    stepper_angle_offset;
    get_encoder_angle_deg(&encoder_angle);
    encoder_angle -= encoder_angle_offset;

    zenbedded_state_t state_val{
      .stepper_motor_position = stepper_angle, .magnetic_encoder_position = encoder_angle};
    zenbedded_command_t cmd_val;

    client.write_state(state_val);
    if (client.read_command(cmd_val))
    {
      stepper_angular_velocity = cmd_val.stepper_motor_velocity;
    }

    set_stepper_angular_vel(stepper_angular_velocity);

    k_sleep(K_MSEC(10));
  }

  client.destroy();
  return 0;
}
