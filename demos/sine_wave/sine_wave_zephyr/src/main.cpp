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
#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <cmath>
#include <zenbedded_rcl/zenbedded_client.hpp>

LOG_MODULE_REGISTER(sine_wave_zephyr, LOG_LEVEL_INF);

#ifndef ZENOH_MODE
#define ZENOH_MODE "client"
#endif
#ifndef ZENOH_LOCATOR
#define ZENOH_LOCATOR "tcp/10.75.105.128:7447"
#endif

#define STATE_TOPIC "zenbedded/sine_wave/state"
#define CMD_TOPIC "zenbedded/sine_wave/cmd"

K_SEM_DEFINE(network_connected_sem, 0, 1);

void net_event_handler(net_mgmt_event_callback * cb, uint64_t mgmt_event, net_if * iface)
{
  if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD)
  {
    LOG_INF("got IPv4 address");
    k_sem_give(&network_connected_sem);
  }
}

float wave_amp = 5;
float wave_frequency = 1.4;  // hz
uint32_t loop_freq = 100;    // hz

int main()
{
  static net_mgmt_event_callback cb;
  net_mgmt_init_event_callback(&cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD);
  net_mgmt_add_event_callback(&cb);

  LOG_INF("starting Sine Wave Zephyr App");

  LOG_INF("Waiting for IPV4 address");
  k_sem_take(&network_connected_sem, K_FOREVER);

  // Disable Wi-Fi power saving
  esp_wifi_set_ps(WIFI_PS_NONE);
  k_sleep(K_MSEC(200));

  static ZenbeddedClient client;

  int ret = client.init(STATE_TOPIC, CMD_TOPIC, ZENOH_MODE, ZENOH_LOCATOR, loop_freq);
  if (ret != 0)
  {
    LOG_ERR("ZenbeddedClient.init failed with %d, aborting", ret);
    return ret;
  }

  int64_t start_time = k_uptime_get();  // Get kernel boot time in ms
  int32_t cnt = 0;
  bool cmd_recv = false;
  while (true)
  {
    const zenbedded_command_t & cmd = client.command();
    zenbedded_state_t & state = client.state();

    if (fabsf(cmd.sine_wave_amplitude) >= 1e-6)
    {
      cmd_recv = true;
    }
    if (cmd_recv)
    {
      wave_amp = fabsf(cmd.sine_wave_amplitude);
    }

    auto current_time_ms = static_cast<double>(k_uptime_get() - start_time);
    double x = 2.0 * M_PI * current_time_ms * wave_frequency / 1000.0;
    state.sine_wave_position = wave_amp * sin(x);

    if (++cnt == 500)
    {
      LOG_INF("Running...");
      cnt = 0;
    }

    client.sync();

    k_sleep(K_MSEC(20));
  }

  client.destroy();
  return 0;
}
