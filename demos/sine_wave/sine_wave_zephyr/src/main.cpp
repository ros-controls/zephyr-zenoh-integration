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

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <cmath>
#include <zenbedded_rcl/zenbedded_client.hpp>

LOG_MODULE_REGISTER(sine_wave_zephyr, LOG_LEVEL_INF);

#ifndef WIFI_SSID
#define WIFI_SSID "mechatronics"
#endif
#ifndef WIFI_PSK
#define WIFI_PSK "123454321"
#endif
#ifndef ZENOH_MODE
#define ZENOH_MODE "client"
#endif
#ifndef ZENOH_LOCATOR
#define ZENOH_LOCATOR "tcp/10.244.80.128:7447"
#endif

#define STATE_TOPIC "zenbedded/sine_wave/state"
#define CMD_TOPIC "zenbedded/sine_wave/cmd"

K_SEM_DEFINE(network_connected_sem, 0, 1);

void net_event_handler(net_mgmt_event_callback * cb, uint32_t mgmt_event, net_if * iface)
{
  if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD)
  {
    LOG_INF("got IPv4 address");
    k_sem_give(&network_connected_sem);
  }
}

int connect_wifi_blocking()
{
  static net_mgmt_event_callback cb;
  net_mgmt_init_event_callback(&cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD);
  net_mgmt_add_event_callback(&cb);

  net_if * iface = net_if_get_default();

  wifi_connect_req_params params = {
    .ssid = reinterpret_cast<const uint8_t *>(WIFI_SSID),
    .ssid_length = sizeof(WIFI_SSID) - 1,
    .psk = reinterpret_cast<const uint8_t *>(WIFI_PSK),
    .psk_length = sizeof(WIFI_PSK) - 1,
    .channel = WIFI_CHANNEL_ANY,
    .security = WIFI_SECURITY_TYPE_PSK,
  };

  LOG_INF("connecting to WiFi...");
  int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
  if (ret)
  {
    LOG_ERR("WiFi connect request failed: %d", ret);
    return ret;
  }

  k_sem_take(&network_connected_sem, K_FOREVER);
  return 0;
}

float wave_amp = 5;
float wave_period = 3000;  // MS
uint32_t loop_freq = 100;  // hz

int main()
{
  LOG_INF("starting test node");
  k_sleep(K_MSEC(2000));
  connect_wifi_blocking();
  k_sleep(K_MSEC(100));

  static ZenbeddedClient client;

  int ret = client.init(STATE_TOPIC, CMD_TOPIC, ZENOH_MODE, ZENOH_LOCATOR, loop_freq);
  if (ret != 0)
  {
    LOG_ERR("ZenbeddedClient.init failed with %d, aborting", ret);
    return ret;
  }

  int64_t start_time = k_uptime_get();  // Get kernel boot time in ms
  int32_t cnt = 0;
  while (true)
  {
    zenbedded_state_t & state = client.state();

    auto current_time_ms = static_cast<float>(k_uptime_get() - start_time);
    float x = 2 * M_PI * current_time_ms / wave_period;
    state.sine_wave_position = wave_amp * (sinf(x) + 1) / 2;

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
