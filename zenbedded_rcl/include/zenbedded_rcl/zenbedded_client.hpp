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
  /**
   * @brief Initialize the client: Zenoh session, queues, and subscriber.
   * @param config_path Path to Zenoh configuration file (optional).
   * @return 0 on success, negative error code.
   */
  int init(const char * config_path = nullptr);

  /**
   * @brief Send a command to the hardware (internal queue).
   */
  int send_command(const zenbedded_command_t & cmd, k_timeout_t timeout = K_NO_WAIT);

  /**
   * @brief Receive the latest state from the hardware (internal queue).
   */
  int receive_state(zenbedded_state_t & state, k_timeout_t timeout = K_NO_WAIT);

  /**
   * @brief Publish the current state via Zenoh (called by the user thread).
   * @param state The state to publish.
   * @return 0 on success, negative on error.
   */
  int publish_state(const zenbedded_state_t & state);

  /**
   * @brief Non‑blocking check if a new command has arrived via Zenoh.
   *        (The subscriber places it into the internal command queue.)
   */
  bool is_command_available() const;

  // The Zenoh subscriber callback will be implemented as a static function.
  static void on_command_sample(const z_sample_t * sample, void * arg);

private:
  // Internal queues (same as before)
  // They are defined globally (or as static members) to be accessible from the callback.
  // We'll keep them as global objects for simplicity.

  // Zenoh session and publisher.
  z_session_t session;
  z_publisher_t state_pub;
  bool initialized = false;
};

#endif  // ZENBEDDED_RCL__ZENBEDDED_CLIENT_HPP_
