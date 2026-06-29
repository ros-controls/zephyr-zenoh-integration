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

#include "zenbedded_rcl/zenbedded_client.hpp"
#include <zephyr/logging/log.h>
#include <cstring>

LOG_MODULE_REGISTER(zenbedded_client, LOG_LEVEL_INF);

// Internal message queues (shared with hardware thread)
K_MSGQ_DEFINE(g_cmd_queue, sizeof(zenbedded_command_t), 1, 4);
K_MSGQ_DEFINE(g_state_queue, sizeof(zenbedded_state_t), 1, 4);

// Zenoh topic names (could be made configurable)
static const char * STATE_TOPIC = "zenbedded/state";
static const char * CMD_TOPIC = "zenbedded/command";

// -------------------------------------------------------------------
// Static callback for incoming command samples
void ZenbeddedClient::on_command_sample(const z_sample_t * sample, void * arg)
{
  if (sample->payload.len != sizeof(zenbedded_command_t))
  {
    LOG_WRN(
      "Received command with wrong size: %zu, expected %zu", sample->payload.len,
      sizeof(zenbedded_command_t));
    return;
  }
  zenbedded_command_t cmd;
  memcpy(&cmd, sample->payload.start, sizeof(cmd));
  // Put into the command queue – non‑blocking, so we may drop if full.
  int ret = k_msgq_put(&g_cmd_queue, &cmd, K_NO_WAIT);
  if (ret != 0)
  {
    LOG_WRN("Command queue full, dropping incoming command");
  }
}

// -------------------------------------------------------------------
int ZenbeddedClient::init(const char * config_path)
{
  if (initialized)
  {
    return 0;
  }

  // 1. Purge queues
  k_msgq_purge(&g_cmd_queue);
  k_msgq_purge(&g_state_queue);

  // 2. Open Zenoh session
  z_owned_config_t config;
  z_config_default(&config);
  if (config_path)
  {
    z_config_from_file(&config, config_path);
  }
  z_owned_session_t session;
  if (z_open(&session, z_move(config), NULL) < 0)
  {
    LOG_ERR("Failed to open Zenoh session");
    return -EIO;
  }
  this->session = z_loan(session);  // store loaned session

  // 3. Declare publisher for state
  z_owned_publisher_t pub;
  z_view_keyexpr_t ke_state;
  z_view_keyexpr_from_str(&ke_state, STATE_TOPIC);
  if (z_declare_publisher(z_loan(session), &pub, z_loan(ke_state), NULL) < 0)
  {
    LOG_ERR("Failed to declare state publisher");
    z_close(z_loan(session), NULL);
    return -EIO;
  }
  this->state_pub = z_loan(pub);

  // 4. Declare subscriber for commands
  z_owned_closure_sample_t callback;
  z_closure(&callback, on_command_sample, NULL, NULL);
  z_view_keyexpr_t ke_cmd;
  z_view_keyexpr_from_str(&ke_cmd, CMD_TOPIC);
  z_owned_subscriber_t sub;
  if (z_declare_subscriber(z_loan(session), &sub, z_loan(ke_cmd), z_move(callback), NULL) < 0)
  {
    LOG_ERR("Failed to declare command subscriber");
    z_undeclare_publisher(z_loan(pub), NULL);
    z_close(z_loan(session), NULL);
    return -EIO;
  }
  // The subscriber lives for the lifetime of the client – we can drop the owned handle
  // or keep it in a member if we need to undeclare later.
  // For simplicity, we'll keep it in a static/global but we'll just let it go out of scope;
  // however, we need to keep it alive. We can store it in a member or a static variable.
  // We'll store it in a static variable for brevity.
  static z_owned_subscriber_t cmd_sub = z_move(sub);  // keep alive

  initialized = true;
  LOG_INF("ZenbeddedClient initialized with Zenoh");
  return 0;
}

int ZenbeddedClient::send_command(const zenbedded_command_t & cmd, k_timeout_t timeout)
{
  return k_msgq_put(&g_cmd_queue, &cmd, timeout);
}

int ZenbeddedClient::receive_state(zenbedded_state_t & state, k_timeout_t timeout)
{
  return k_msgq_get(&g_state_queue, &state, timeout);
}

int ZenbeddedClient::publish_state(const zenbedded_state_t & state)
{
  if (!initialized)
  {
    return -ENODEV;
  }
  z_owned_bytes_t payload;
  z_bytes_copy_from(&payload, (const uint8_t *)&state, sizeof(state));
  int ret = z_publisher_put(z_loan(state_pub), z_move(payload), NULL);
  if (ret < 0)
  {
    LOG_ERR("Failed to publish state: %d", ret);
  }
  return ret;
}

bool ZenbeddedClient::is_command_available() const
{
  zenbedded_command_t dummy;
  return (k_msgq_get(&g_cmd_queue, &dummy, K_NO_WAIT) == 0);
}
