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
#include <zenbedded_rcl/zenbedded_client.hpp>

LOG_MODULE_REGISTER(inverted_pendulum_demo, LOG_LEVEL_INF);

#define STATE_TOPIC "zenbedded/inverted_pendulum/state"
#define CMD_TOPIC "zenbedded/inverted_pendulum/cmd"

constexpr uint32_t motor_steps_per_rev = 200;
constexpr uint32_t micro_step_res = DT_PROP_OR(DT_ALIAS(stepper_driver), micro_step_res, 1);
constexpr uint32_t micro_steps_per_rev = motor_steps_per_rev * micro_step_res;
uint32_t kWifiConnectTimeout = 15000;  // ms

const device * stepper_driver = DEVICE_DT_GET(DT_ALIAS(stepper_driver));
const device * stepper_ctrl = DEVICE_DT_GET(DT_ALIAS(stepper_ctrl));
const device * encoder_driver = DEVICE_DT_GET(DT_ALIAS(encoder_driver));

void set_stepper_angular_vel(double deg_per_sec)
{
  if (deg_per_sec == 0.0)
  {
    stepper_ctrl_stop(stepper_ctrl);
    return;
  }

  double microsteps_per_sec = fabs(deg_per_sec) * micro_steps_per_rev / 360.0;

  if (microsteps_per_sec <= 0.0)
  {
    stepper_ctrl_stop(stepper_ctrl);
    return;
  }

  auto interval_ns = static_cast<uint64_t>(1e9 / microsteps_per_sec);

  stepper_ctrl_set_microstep_interval(stepper_ctrl, interval_ns);

  stepper_ctrl_run(
    stepper_ctrl,
    (deg_per_sec > 0.0) ? STEPPER_CTRL_DIRECTION_POSITIVE : STEPPER_CTRL_DIRECTION_NEGATIVE);
}

bool get_encoder_angle_deg(double * angle_deg)
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

  *angle_deg = sensor_value_to_double(&val);
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

  LOG_INF("Starting Inverted Pendulum App");
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

  static ZenbeddedClient client;

  int ret = client.init(STATE_TOPIC, CMD_TOPIC, 100);
  if (ret != 0)
  {
    LOG_ERR("ZenbeddedClient.init failed with %d, aborting", ret);
    return ret;
  }

  stepper_enable(stepper_driver);

  while (true)
  {
    const zenbedded_command_t & cmd = client.command();
    zenbedded_state_t & state = client.state();

    set_stepper_angular_vel(cmd.stepper_motor_velocity);

    int32_t stepper_position;
    stepper_ctrl_get_actual_position(stepper_ctrl, &stepper_position);
    state.stepper_motor_position = static_cast<float>(stepper_position);

    double angle_deg;
    if (get_encoder_angle_deg(&angle_deg))
    {
      state.magnetic_encoder_position = static_cast<float>(angle_deg);
    }

    client.sync();

    k_sleep(K_MSEC(20));
  }

  return 0;
}
