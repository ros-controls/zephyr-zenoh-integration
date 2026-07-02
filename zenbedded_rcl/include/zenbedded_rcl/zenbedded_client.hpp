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

#ifndef ZENBEDDED_RCL__ZENBEDDED_CLIENT_HPP_
#define ZENBEDDED_RCL__ZENBEDDED_CLIENT_HPP_

#include <zenoh-pico.h>
#include <zephyr/kernel.h>
#include "zenbedded_rcl/interface_data.h"

class ZenbeddedClient
{
public:
  ZenbeddedClient() { reset_buffers(); }

  /// @brief initialize the zenbedded client
  /// @param control_freq the loop frequency in Hz (thread only start on positive values)
  /// @return int
  int init(const char * state_topic, const char * cmd_topic, uint32_t control_freq = 100);

  // Deinitialize the client
  void destroy();

  // Synchronizes buffers between user and the zenbedded client.
  void sync();

  // Publish the current state via Zenoh.
  int zenoh_publish_state();

  /// @brief start the zenbedded thread
  /// @param control_freq the loop frequency in Hz (thread only start on positive values)
  /// @return int
  int start_thread(uint32_t control_freq);

  // Stop zenbedded thread
  void stop_thread();

  // Get a reference to the user's state buffer (for writing).
  zenbedded_state_t & state() { return user_state_buffer_; }

  // Get a const reference to the user's command buffer (for reading).
  [[nodiscard]] const zenbedded_command_t & command() const { return user_command_buffer_; }

  // Check if client is initialized.
  [[nodiscard]] bool is_initialized() const { return initialized_; }

  bool is_control_thread_running() { return atomic_get(&control_thread_running_); }

private:
  // Double-buffer for State
  zenbedded_state_t state_buffer_[2];  // Double buffer for states
  atomic_t state_buffer_active_idx_;   // Which buffer is active (0 or 1)
  atomic_t state_buffer_version_[2];   // buffer versioning for ABA prevention

  // Double-buffer for Command
  zenbedded_command_t cmd_buffer_[2];  // Double buffer for commands
  atomic_t cmd_buffer_active_idx_;     // Which buffer is active (0 or 1)
  atomic_t cmd_buffer_version_[2];     // buffer versioning for ABA prevention

  // User-space buffers (accessed by user thread)
  zenbedded_state_t user_state_buffer_{};      // User writes state here
  zenbedded_command_t user_command_buffer_{};  // User reads commands here

  // Zenoh session, pub & sub.
  z_owned_session_t z_session_{};
  z_owned_publisher_t z_state_pub_{};
  z_owned_subscriber_t z_cmd_sub_{};

  // zenoh topics
  const char * state_topic_ = nullptr;
  const char * cmd_topic_ = nullptr;

  // module ready
  bool initialized_ = false;

  // control thread variables
  uint32_t control_freq_ = 0;
  atomic_t control_thread_running_ = 0;
  bool control_thread_started_ = false;
  k_thread control_thread_;
  K_KERNEL_STACK_MEMBER(control_thread_stack_, CONFIG_ZENBEDDED_RCL_THREAD_STACK_SIZE);

  // Control thread function
  static void control_thread_fn(void * arg1, void * arg2, void * arg3);

  // Control flags
  // reset buffers and locks
  void reset_buffers();

  // The Zenoh subscriber callback.
  static void on_zenoh_command_cb(z_loaned_sample_t * sample, void * arg);

  // buffer read/write commands
  void write_state_to_buffer(const zenbedded_state_t & state);

  bool read_state_from_buffer(zenbedded_state_t & state);

  void write_command_to_buffer(const zenbedded_command_t & cmd);

  bool read_command_from_buffer(zenbedded_command_t & cmd);
};

#endif  // ZENBEDDED_RCL__ZENBEDDED_CLIENT_HPP_
