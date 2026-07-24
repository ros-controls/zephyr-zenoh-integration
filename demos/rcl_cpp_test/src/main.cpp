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

#include <math.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zenbedded_rcl/zenbedded_client.hpp>

LOG_MODULE_REGISTER(zenbedded_test_node, LOG_LEVEL_INF);

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
#define ZENOH_LOCATOR "tcp/10.79.152.128:7447"
#endif

#define STATE_TOPIC "zenbedded/test/state"
#define CMD_TOPIC "zenbedded/test/cmd"

constexpr uint32_t kControlFreqHz = 50;
constexpr int kIterations = 200;  // ~4s at 50Hz
constexpr int kWifiConnectTimeout = 15;

K_SEM_DEFINE(wifi_connected_sem, 0, 1);

void net_event_handler(net_mgmt_event_callback * cb, uint32_t mgmt_event, net_if * iface)
{
  if (mgmt_event == NET_EVENT_L4_CONNECTED)
  {
    LOG_INF("ZENBEDDED_TEST: got IPv4 address");
    k_sem_give(&wifi_connected_sem);
  }
}

static bool already_has_ipv4(net_if * iface)
{
  return net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED) != nullptr;
}

int connect_wifi_blocking(int timeout)
{
  static net_mgmt_event_callback cb;
  net_mgmt_init_event_callback(&cb, net_event_handler, NET_EVENT_L4_CONNECTED);
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

  // Belt-and-suspenders against the same race happening between the
  // connect request above and k_sem_take below: poll briefly in addition
  // to waiting on the semaphore, instead of trusting the event exclusively.
  int64_t deadline = k_uptime_get() + timeout * 1000;
  while (k_uptime_get() < deadline)
  {
    if (k_sem_take(&wifi_connected_sem, K_MSEC(200)) == 0)
    {
      return 0;
    }
    if (already_has_ipv4(iface))
    {
      LOG_INF(
        "IP appeared without the event firing (race with "
        "callback registration) -- continuing anyway");
      return 0;
    }
  }
  // if (k_sem_take(&wifi_connected_sem, K_SECONDS(timeout))==0) return 0;
  LOG_ERR("WiFi connect timed out after %ds", timeout);
  return -ETIMEDOUT;
}

static void report(bool pass, const char * msg)
{
  if (pass)
  {
    LOG_INF("PASS: %s", msg);
  }
  else
  {
    LOG_ERR("FAIL: %s", msg);
  }
}

int main(void)
{
  LOG_INF("starting test node");
  k_sleep(K_MSEC(500));

  // --- WiFi bring-up (test node's job now, not the module's) ---
  int wifi_ret = connect_wifi_blocking(kWifiConnectTimeout);
  report(wifi_ret == 0, "WiFi connected and got an IP");
  if (wifi_ret != 0)
  {
    LOG_ERR("cannot continue without network, aborting");
    return wifi_ret;
  }

  k_sleep(K_MSEC(500));
  static ZenbeddedClient client;

  int ret = client.init(STATE_TOPIC, CMD_TOPIC, ZENOH_MODE, ZENOH_LOCATOR, kControlFreqHz);
  report(ret == 0, "client.init() returned 0");
  if (ret != 0)
  {
    LOG_ERR("init failed with %d, aborting", ret);
    return ret;
  }
  report(client.is_initialized(), "client.is_initialized() is true after init()");

  // --- exercise the public API surface ---
  int mismatch_count = 0;
  int sync_calls = 0;

  for (int i = 0; i < kIterations; i++)
  {
    // Write a deterministic, easy-to-eyeball waveform into state()
    zenbedded_state_t & s = client.state();
    s.motor_arm_position = sinf(static_cast<float>(i) * 0.1f) * 90.0f;
    s.pendulum_axis_position = static_cast<float>(i) * 0.01f;

    client.sync();
    sync_calls++;

    const zenbedded_command_t & cmd = client.command();

    if (i % 20 == 0)
    {
      LOG_INF(
        "iter=%d state.motor_arm=%d.%02d "
        "state.pendulum=%d.%02d cmd.motor_arm=%d.%02d",
        i, static_cast<int>(s.motor_arm_position),
        abs(static_cast<int>(s.motor_arm_position * 100) % 100),
        static_cast<int>(s.pendulum_axis_position),
        abs(static_cast<int>(s.pendulum_axis_position * 100) % 100),
        static_cast<int>(cmd.motor_arm_position),
        abs(static_cast<int>(cmd.motor_arm_position * 100) % 100));
    }

    // A stale/never-changing command across many iterations is a real
    // regression signal once tools/zenoh_echo_node.py is running as a
    // peer; without it there's no command traffic and this is expected
    // to stay at 0.0.
    if (cmd.motor_arm_position == 0.0f)
    {
      mismatch_count++;
    }

    k_sleep(K_MSEC(1000 / kControlFreqHz));
  }

  report(sync_calls == kIterations, "sync() called for every iteration without crashing");

  bool loopback_ok = mismatch_count < kIterations;
  report(
    loopback_ok,
    "received at least one non-zero command "
    "(requires tools/zenoh_echo_node.py running as a peer)");

  LOG_INF("is_control_thread_running=%d", client.is_control_thread_running());

  // --- destroy() ---
  client.destroy();
  report(!client.is_initialized(), "client.destroy() left client uninitialized");

  LOG_INF("done");
  return 0;
}
