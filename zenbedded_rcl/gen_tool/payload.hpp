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

/**
 * ZENBEDDED Tier 2 PAYLOAD HEADER
 * payload.hpp
 *
 * AUTO-GENERATED — do not edit by hand.
 * Source: sample_robot.urdf
 * Generated: 2026-06-19 09:37 UTC
 *
 * Packed wire-format structs for Zephyr <-> ROS2 hardware interface over Zenoh (Tier 2).
 * Include on both sides (Zephyr module and ROS2 hardware interface).
 *
 * Guarantees:
 *   - #pragma pack(push, 1) — zero hidden padding bytes.
 *   - All multi-byte fields are little-endian (ARM LE, x86, RISC-V LE).
 *   - Fixed-width types only.
 *   - sizeof() stable across compilers.
 *
 * Total payload: 173 bytes
 */

#ifndef ZENBEDDED_RCL__GEN_TOOL__PAYLOAD_HPP_
#define ZENBEDDED_RCL__GEN_TOOL__PAYLOAD_HPP_

#include <stdint.h>
#ifdef __cplusplus
#include <cstdint>
#endif

// Enforcing byte alignment — no hidden padding bytes inserted by compiler
#pragma pack(push, 1)

// ── sub-structs for individual hardware components ───────────────────────────

// joint: shoulder_pan_joint  (8 bytes total)
struct shoulder_pan_joint_cmd_t
{
  double position;  // 8 B
};

// joint: shoulder_pan_joint  (24 bytes total)
struct shoulder_pan_joint_state_t
{
  double position;  // 8 B
  double velocity;  // 8 B
  double effort;    // 8 B
};

// joint: elbow_joint  (8 bytes total)
struct elbow_joint_cmd_t
{
  double velocity;  // 8 B
};

// joint: elbow_joint  (16 bytes total)
struct elbow_joint_state_t
{
  double velocity;  // 8 B
  double effort;    // 8 B
};

// joint: wrist_joint  (16 bytes total)
struct wrist_joint_cmd_t
{
  double position;  // 8 B
  double effort;    // 8 B
};

// joint: wrist_joint  (24 bytes total)
struct wrist_joint_state_t
{
  double position;  // 8 B
  double velocity;  // 8 B
  double effort;    // 8 B
};

// sensor: base_imu  (0 bytes total)
struct base_imu_cmd_t
{
};

// sensor: base_imu  (48 bytes total)
struct base_imu_state_t
{
  double linear_acceleration_x;  // 8 B
  double linear_acceleration_y;  // 8 B
  double linear_acceleration_z;  // 8 B
  double angular_velocity_x;     // 8 B
  double angular_velocity_y;     // 8 B
  double angular_velocity_z;     // 8 B
};

// gpio: tool_gpio  (1 bytes total)
struct tool_gpio_cmd_t
{
  uint8_t gpio_vacuum_enable;  // 1 B
};

// gpio: tool_gpio  (16 bytes total)
struct tool_gpio_state_t
{
  double gpio_vacuum_pressure;  // 8 B
  double gpio_error_status;     // 8 B
};

// ── master bundled payload ───────────────────────────────────────────────────
// Total size: 173 bytes
struct zenbedded_payload_t
{
  uint64_t timestamp_ns;  // 8 B
  uint32_t sequence_id;   // 4 B

  // Embedded components
  shoulder_pan_joint_cmd_t shoulder_pan_joint;    // 8B
  shoulder_pan_joint_state_t shoulder_pan_joint;  // 24B
  elbow_joint_cmd_t elbow_joint;                  // 8B
  elbow_joint_state_t elbow_joint;                // 16B
  wrist_joint_cmd_t wrist_joint;                  // 16B
  wrist_joint_state_t wrist_joint;                // 24B
  base_imu_cmd_t base_imu;                        // 0B
  base_imu_state_t base_imu;                      // 48B
  tool_gpio_cmd_t tool_gpio;                      // 1B
  tool_gpio_state_t tool_gpio;                    // 16B
};

#pragma pack(pop)  // restore default packing

// ── compile-time size assertions ──────────────────────────────────────────────
#ifdef __cplusplus
static_assert(
  sizeof(shoulder_pan_joint_cmd_t) == 8, "shoulder_pan_joint_cmd_t size mismatch — check packing");
static_assert(
  sizeof(shoulder_pan_joint_state_t) == 24,
  "shoulder_pan_joint_state_t size mismatch — check packing");
static_assert(sizeof(elbow_joint_cmd_t) == 8, "elbow_joint_cmd_t size mismatch — check packing");
static_assert(
  sizeof(elbow_joint_state_t) == 16, "elbow_joint_state_t size mismatch — check packing");
static_assert(sizeof(wrist_joint_cmd_t) == 16, "wrist_joint_cmd_t size mismatch — check packing");
static_assert(
  sizeof(wrist_joint_state_t) == 24, "wrist_joint_state_t size mismatch — check packing");
static_assert(sizeof(base_imu_cmd_t) == 0, "base_imu_cmd_t size mismatch — check packing");
static_assert(sizeof(base_imu_state_t) == 48, "base_imu_state_t size mismatch — check packing");
static_assert(sizeof(tool_gpio_cmd_t) == 1, "tool_gpio_cmd_t size mismatch — check packing");
static_assert(sizeof(tool_gpio_state_t) == 16, "tool_gpio_state_t size mismatch — check packing");
static_assert(
  sizeof(zenbedded_payload_t) == 173, "zenbedded_payload_t size mismatch — check packing");
#endif  // __cplusplus

#endif  // ZENBEDDED_RCL__GEN_TOOL__PAYLOAD_HPP_
