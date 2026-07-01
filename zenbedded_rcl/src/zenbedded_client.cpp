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
#include <zenoh-pico.h>
#include <zephyr/kernel.h>

LOG_MODULE_REGISTER(zenbedded_client, LOG_LEVEL_INF);

// Static pointer to the client instance for the callback
static ZenbeddedClient * zrcl_instance = nullptr;

void ZenbeddedClient::reset_buffers()
{
  // reset atomic variables
  atomic_set(&state_buffer_active_idx_, 0);
  atomic_set(&cmd_buffer_active_idx_, 0);

  atomic_set(&state_buffer_version_[0], 0);
  atomic_set(&state_buffer_version_[1], 0);
  atomic_set(&cmd_buffer_version_[0], 0);
  atomic_set(&cmd_buffer_version_[1], 0);

  // Clear buffers
  memset(state_buffer_, 0, sizeof(state_buffer_));
  memset(cmd_buffer_, 0, sizeof(cmd_buffer_));
  memset(&user_state_buffer_, 0, sizeof(user_state_buffer_));
  memset(&user_command_buffer_, 0, sizeof(user_command_buffer_));
}

int ZenbeddedClient::init(const char * state_topic, const char * cmd_topic, uint32_t control_freq)
{
  if (initialized_)
  {
    LOG_WRN("Zenbedded Client already initialized");
    return 0;
  }

  if (!state_topic || !cmd_topic)
  {
    LOG_ERR("State and command topics cannot be NULL");
    return -EINVAL;
  }

  state_topic_ = state_topic;
  cmd_topic_ = cmd_topic;

  reset_buffers();

  // Open Zenoh session
  z_owned_config_t config;
  z_config_default(&config);

  if (z_open(&z_session_, z_move(config), nullptr) < 0)
  {
    LOG_ERR("Failed to open Zenoh session");
    z_config_drop(z_move(config));
    return -EIO;
  }

  // Declare publisher for state (using configured topic)
  z_view_keyexpr_t ke_state;
  if (z_view_keyexpr_from_str(&ke_state, state_topic) < 0)
  {
    LOG_ERR("Invalid state topic: %s", state_topic);
    z_close(z_session_loan_mut(&z_session_), nullptr);
    return -EINVAL;
  }
  if (
    z_declare_publisher(z_session_loan(&z_session_), &z_state_pub_, z_loan(ke_state), nullptr) < 0)
  {
    LOG_ERR("Failed to declare state publisher on topic: %s", state_topic);
    z_close(z_session_loan_mut(&z_session_), nullptr);
    return -EIO;
  }

  // subscriber for commands (using configured topic)
  z_owned_closure_sample_t callback;
  z_closure(&callback, on_zenoh_command_cb, nullptr, this);
  z_view_keyexpr_t ke_cmd;
  if (z_view_keyexpr_from_str(&ke_cmd, cmd_topic) < 0)
  {
    LOG_ERR("Invalid command topic: %s", cmd_topic);
    z_undeclare_publisher(z_publisher_move(&z_state_pub_));
    z_close(z_session_loan_mut(&z_session_), nullptr);
    return -EINVAL;
  }
  if (
    z_declare_subscriber(
      z_session_loan(&z_session_), &z_cmd_sub_, z_loan(ke_cmd), z_move(callback), nullptr) < 0)
  {
    LOG_ERR("Failed to declare command subscriber on topic: %s", cmd_topic);
    z_undeclare_publisher(z_publisher_move(&z_state_pub_));
    z_close(z_session_loan_mut(&z_session_), nullptr);
    return -EIO;
  }

  initialized_ = true;
  control_freq_ = control_freq;
  zrcl_instance = this;

  LOG_INF("ZenbeddedClient initialized: state='%s', cmd='%s'", state_topic, cmd_topic);
  start_thread(control_freq);
  return 0;
}

void ZenbeddedClient::destroy()
{
  if (!initialized_)
  {
    return;
  }
  stop_thread();
  z_undeclare_subscriber(z_subscriber_move(&z_cmd_sub_));
  z_undeclare_publisher(z_publisher_move(&z_state_pub_));
  z_close(z_session_loan_mut(&z_session_), nullptr);

  z_session_drop(z_session_move(&z_session_));

  initialized_ = false;
  zrcl_instance = nullptr;

  LOG_INF("ZenbeddedClient deinitialized");
}

void ZenbeddedClient::on_zenoh_command_cb(z_loaned_sample_t * sample, void * arg)
{
  zenbedded_command_t cmd;
  z_bytes_reader_t reader = z_bytes_get_reader(z_sample_payload(sample));
  size_t bytes_copied =
    z_bytes_reader_read(&reader, reinterpret_cast<uint8_t *>(&cmd), sizeof(zenbedded_command_t));

  if (bytes_copied != sizeof(zenbedded_command_t))
  {
    LOG_WRN("Invalid command size: %zu, expected %zu", bytes_copied, sizeof(zenbedded_command_t));
    return;
  }

  auto * self = static_cast<ZenbeddedClient *>(arg);
  self->write_command_to_buffer(cmd);
}

int ZenbeddedClient::zenoh_publish_state()
{
  if (!initialized_)
  {
    LOG_ERR("Zenbedded Client not initialized");
    return -ENODEV;
  }

  // used slices to ensure zero dynamic allocations
  zenbedded_state_t state;
  z_owned_bytes_t payload;
  z_owned_slice_t slice;

  while (!read_state_from_buffer(state))
  {
  }

  z_slice_from_buf(
    &slice, reinterpret_cast<uint8_t *>(&state), sizeof(zenbedded_state_t), nullptr, nullptr);
  z_bytes_from_slice(&payload, z_move(slice));

  z_result_t ret = z_publisher_put(z_loan(z_state_pub_), z_move(payload), nullptr);
  if (ret < 0)
  {
    LOG_ERR("Failed to publish state: %d", ret);
  }
  return ret;
}

void ZenbeddedClient::sync()
{
  if (!initialized_)
  {
    LOG_ERR("Zenbedded Client not initialized");
    return;
  }

  write_state_to_buffer(user_state_buffer_);
  while (!read_command_from_buffer(user_command_buffer_))
  {
  }
}

bool ZenbeddedClient::read_state_from_buffer(zenbedded_state_t & state)
{
  const int read_idx = atomic_get(&state_buffer_active_idx_);
  const atomic_val_t ver_before = atomic_get(&state_buffer_version_[read_idx]);
  state = state_buffer_[read_idx];

  // Ensure read completes before consistency check
  __atomic_thread_fence(__ATOMIC_ACQUIRE);

  // Verify consistency
  if (
    atomic_get(&state_buffer_active_idx_) != read_idx ||
    atomic_get(&state_buffer_version_[read_idx]) != ver_before)
  {
    return false;
  }
  return true;
}

bool ZenbeddedClient::read_command_from_buffer(zenbedded_command_t & cmd)
{
  const int read_idx = atomic_get(&cmd_buffer_active_idx_);
  const atomic_val_t ver_before = atomic_get(&cmd_buffer_version_[read_idx]);
  cmd = cmd_buffer_[read_idx];

  // Ensure read completes before consistency check
  __atomic_thread_fence(__ATOMIC_ACQUIRE);

  if (
    atomic_get(&cmd_buffer_active_idx_) != read_idx ||
    atomic_get(&cmd_buffer_version_[read_idx]) != ver_before)
  {
    return false;
  }
  return true;
}

void ZenbeddedClient::write_state_to_buffer(const zenbedded_state_t & state)
{
  const int write_idx = atomic_get(&state_buffer_active_idx_) ^ 1;
  state_buffer_[write_idx] = state;

  // Ensure write completes before version increment
  __atomic_thread_fence(__ATOMIC_RELEASE);

  atomic_inc(&state_buffer_version_[write_idx]);
  atomic_set(&state_buffer_active_idx_, write_idx);  // Flip active index
}

void ZenbeddedClient::write_command_to_buffer(const zenbedded_command_t & cmd)
{
  const int write_idx = atomic_get(&cmd_buffer_active_idx_) ^ 1;
  cmd_buffer_[write_idx] = cmd;

  // Ensure write completes before version increment
  __atomic_thread_fence(__ATOMIC_RELEASE);

  atomic_inc(&cmd_buffer_version_[write_idx]);
  atomic_set(&cmd_buffer_active_idx_, write_idx);  // Flip active index
}

int ZenbeddedClient::start_thread(uint32_t control_freq)
{
  if (!initialized_)
  {
    LOG_ERR("Zenbedded Client not initialized");
    return -ENODEV;
  }

  if (control_freq == 0)
  {
    LOG_WRN("Control Thread frequency is zero, thread unable to start");
    return -EINVAL;
  }

  if (control_thread_started_)
  {
    stop_thread();
    k_sleep(K_MSEC(10));
  }

  control_freq_ = control_freq;
  atomic_set(&control_thread_running_, 1);

  k_thread_create(
    &control_thread_, control_thread_stack_, K_KERNEL_STACK_SIZEOF(control_thread_stack_),
    control_thread_fn,
    this,     // arg1 - pass client instance
    nullptr,  // arg2
    nullptr,  // arg3
    CONFIG_ZENBEDDED_RCL_THREAD_PRIORITY,
    0,         // options
    K_NO_WAIT  // start immediately
  );

  control_thread_started_ = true;

  LOG_INF("Started thread at %i Hz", control_freq);
  return 0;
}

void ZenbeddedClient::stop_thread()
{
  if (!control_thread_started_)
  {
    return;
  }

  LOG_INF("Stopping control thread");
  atomic_set(&control_thread_running_, 0);  // cleanly exit thread loop

  // Wait for thread to exit (with timeout)
  int timeout_ms = 200;
  while (atomic_get(&control_thread_running_) != 0 && timeout_ms > 0)
  {
    k_sleep(K_MSEC(10));
    timeout_ms -= 10;
  }

  if (timeout_ms <= 0)
  {
    LOG_WRN("Control thread did not stop gracefully");
    k_thread_abort(&control_thread_);
  }

  control_thread_started_ = false;
  control_freq_ = 0;

  LOG_INF("Publish thread stopped");
}

void ZenbeddedClient::control_thread_fn(void * arg1, void * arg2, void * arg3)
{
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  auto * self = static_cast<ZenbeddedClient *>(arg1);

  if (!self)
  {
    LOG_ERR("Invalid client instance in publish thread");
    return;
  }

  LOG_INF("Control thread started");

  const uint32_t period_ms = MAX(1000 / self->control_freq_, 1);
  uint32_t prev_time = k_uptime_get_32();

  while (atomic_get(&self->control_thread_running_))
  {
    int ret = self->zenoh_publish_state();
    if (ret < 0)
    {
      LOG_DBG("Zenoh Publish failed: %d", ret);
    }
    // wait for extra time to maintain loop freq
    const uint32_t diff = k_uptime_get_32() - prev_time;
    k_sleep(K_MSEC(MAX(period_ms - diff, 1)));
    prev_time = k_uptime_get_32();
  }

  // Clear running flag to signal exit
  atomic_set(&self->control_thread_running_, 0);
  LOG_INF("Publish thread stopped");
}
