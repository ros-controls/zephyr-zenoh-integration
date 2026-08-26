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

#include <stdint.h>
#include <stdio.h>

#include <zephyr/kernel.h>
#include <zenbedded_transport/serialization.hpp>

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_1
static void print_hex_dump(const char * label, const uint8_t * buffer, size_t size)
{
  printf("\n--- %s HEX DUMP (%zu bytes) ---\n", label, size);
  for (size_t i = 0; i < size; i++)
  {
    printf("%02X ", buffer[i]);
    if ((i + 1) % 16 == 0)
    {
      printf("\n");
    }
  }
  printf("\n---------------------------------------\n");
}
#endif

int main()
{
  printf("\n=======================================\n");
  printf("  Zenbedded CDR: Integration Test Node\n");
  printf("=======================================\n");

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_1
  printf("\n[Tier 1 Native CDR Engine Enabled]\n");

  // --- JointState Loopback Test ---
  zcdr_joint_state_ctx_t state_ctx;
  uint8_t state_buffer[512] = {0};

  const char * frame_id = "base_link";
  const char * joints[] = {"stepper", "pendulum"};
  uint32_t num_joints = 2;

  zcdr_init_joint_state(&state_ctx, frame_id, joints, num_joints, state_buffer);

  double tx_pos[] = {1.57, -3.14};
  double tx_vel[] = {0.5, 0.0};
  double tx_eff[] = {1.2, 0.0};
  zcdr_serialize_joint_state(&state_ctx, 1700000000, 500000, tx_pos, tx_vel, tx_eff, state_buffer);

  printf("\n=== JointState Loopback ===\n");
  print_hex_dump("JointState", state_buffer, state_ctx.payload_size);

  int32_t rx_sec = 0;
  uint32_t rx_nanosec = 0;
  double rx_pos[2] = {0};
  double rx_vel[2] = {0};
  double rx_eff[2] = {0};

  bool state_success = zcdr_deserialize_joint_state(
    &state_ctx, state_buffer, state_ctx.payload_size, &rx_sec, &rx_nanosec, rx_pos, rx_vel, rx_eff);

  if (state_success)
  {
    printf("SUCCESS: JointState payload extracted cleanly.\n");
    printf("  -> Time: %d.%d\n", rx_sec, rx_nanosec);
    printf("  -> Positions: [%f, %f]\n", rx_pos[0], rx_pos[1]);
  }
  else
  {
    printf("FAILED: Corrupted JointState payload.\n");
  }

  // --- JointCommand Loopback Test ---
  zcdr_joint_command_ctx_t cmd_ctx;
  uint8_t cmd_buffer[512] = {0};
  const char * interface_name = "velocity";

  zcdr_init_joint_command(&cmd_ctx, frame_id, joints, num_joints, interface_name, cmd_buffer);

  double cmd_tx_values[] = {10.5, -5.25};
  zcdr_serialize_joint_command(&cmd_ctx, 1700000001, 600000, cmd_tx_values, cmd_buffer);

  printf("\n=== JointCommand Loopback ===\n");
  print_hex_dump("JointCommand", cmd_buffer, cmd_ctx.payload_size);

  int32_t cmd_rx_sec = 0;
  uint32_t cmd_rx_nano = 0;
  double cmd_rx_values[2] = {0};

  bool cmd_success = zcdr_deserialize_joint_command(
    &cmd_ctx, cmd_buffer, cmd_ctx.payload_size, &cmd_rx_sec, &cmd_rx_nano, cmd_rx_values);

  if (cmd_success)
  {
    printf("SUCCESS: JointCommand payload extracted cleanly.\n");
    printf("  -> Time: %d.%d\n", cmd_rx_sec, cmd_rx_nano);
    printf("  -> Values: [%f, %f]\n", cmd_rx_values[0], cmd_rx_values[1]);
  }
  else
  {
    printf("FAILED: Corrupted JointCommand payload.\n");
  }

#else
  printf("\nCONFIG_ZENBEDDED_TRANSPORT_TIER_1 is disabled. Skipping CDR tests.\n");
#endif

  return 0;
}
