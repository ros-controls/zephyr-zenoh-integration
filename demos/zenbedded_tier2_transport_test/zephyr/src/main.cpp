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

// This header MUST be generated at build-time
#include "zenbedded_transport/generated/interface_data.h"  // NOLINT
#include "zenbedded_transport/zenoh_transport.h"

LOG_MODULE_REGISTER(tier2_test, LOG_LEVEL_INF);

int main(void)
{
  LOG_INF("Starting Zenbedded Tier 2 Transport Test");

  zenbedded_state_t my_state;
  my_state.test_motor_position = 1.23;
  my_state.test_motor_velocity = 0.55;

  // Simulate passing the raw bytes to the dumb transport pipe
  const uint8_t * raw_bytes = reinterpret_cast<const uint8_t *>(&my_state);
  size_t size = sizeof(zenbedded_state_t);

  LOG_INF("Packed Tier 2 Struct. Total bytes to send: %zu", size);
  LOG_HEXDUMP_INF(raw_bytes, size, "Packed Tier 2 struct");

  while (1)
  {
    k_sleep(K_MSEC(1000));
  }
  return 0;
}
