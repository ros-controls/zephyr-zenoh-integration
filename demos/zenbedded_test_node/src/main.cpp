// Copyright 2026 Zenbedded
// Licensed under the Apache License, Version 2.0

#include <stdint.h>
#include <stdio.h>
#include <zephyr/kernel.h>

#include "zenbedded_transport/serialization.h"
#include "zenbedded_transport/zenoh_transport.h"

void diagnostic_rx_callback(const uint8_t * payload, size_t size, void * user_data) {}

int main(void)
{
  printf("\n=======================================\n");
  printf("  Zenbedded Transport: RCL Reference   \n");
  printf("  HIGH-SPEED MULTI-TOPIC EXECUTOR      \n");
  printf("=======================================\n");

#ifdef CONFIG_TIER_1

  if (zenbedded_transport_init(0, "Zenbedded_mcu", "client", "tcp/127.0.0.1:7447") != 0)
  {
    printf("[FATAL] Transport init failed!\n");
    return -1;
  }

  // --- PUBLISHER DECLARATIONS ---
  const char * js_hash = "RIHS01_a13ee3a330e346c9d87b5aa18d24e11690752bd33a0350f11c5882bc9179260e";
  const char * cmd_hash = "RIHS01_6080a1df9d28b6badffa5efb27d4ba4ae657c4f6dd2b519b178a32db12405985";

  // Topic 1: JointState (Will run at 50Hz)
  zenbedded_pub_t pub_joints = zenbedded_transport_declare_publisher(
    "joint_states", "sensor_msgs::msg::dds_::JointState_", js_hash);

  // Topic 2: JointState (Will run at 10Hz)
  zenbedded_pub_t pub_arm = zenbedded_transport_declare_publisher(
    "arm_states", "sensor_msgs::msg::dds_::JointState_", js_hash);

  // Topic 3: JointCommand (Will run at MAX SPEED)
  zenbedded_pub_t pub_cmd = zenbedded_transport_declare_publisher(
    "joint_commands", "control_msgs::msg::dds_::JointCommand_", cmd_hash);

  // --- SUBSCRIBER DECLARATION ---
  zenbedded_transport_declare_subscriber(
    "diagnostic_cmd", "geometry_msgs::msg::dds_::Twist_", js_hash, diagnostic_rx_callback, NULL);

  // --- MEMORY & SERIALIZATION SETUP ---
  // Fix: Explicitly declare string arrays to satisfy C++17 memory rules
  const char * chassis_joints[] = {"stepper", "pendulum"};
  const char * arm_joints[] = {"shoulder", "elbow", "wrist"};
  const char * cmd_joints[] = {"stepper", "pendulum"};

  zcdr_joint_state_ctx_t ctx_joints, ctx_arm;
  zcdr_joint_command_ctx_t ctx_cmd;

  uint8_t buf_joints[512] = {0};
  uint8_t buf_arm[512] = {0};
  uint8_t buf_cmd[512] = {0};

  zcdr_init_joint_state(&ctx_joints, "base_link", chassis_joints, 2, buf_joints);
  zcdr_init_joint_state(&ctx_arm, "arm_base", arm_joints, 3, buf_arm);
  zcdr_init_joint_command(&ctx_cmd, "base_link", cmd_joints, 2, "position", buf_cmd);

  // --- DATA PAYLOADS ---
  double pos_2[] = {0.0, 0.0};
  double vel_2[] = {0.0, 0.0};
  double eff_2[] = {0.0, 0.0};

  double pos_3[] = {1.1, 2.2, 3.3};
  double vel_3[] = {0.0, 0.0, 0.0};
  double eff_3[] = {0.0, 0.0, 0.0};

  double cmd_vals[] = {100.0, 200.0};

  // --- TIMING SETUP ---
  uint32_t last_50hz_time = k_uptime_get_32();
  uint32_t last_10hz_time = k_uptime_get_32();
  uint32_t last_max_time = k_uptime_get_32();

  printf("\n[SYS] Entering High-Frequency Event Loop...\n\n");

  while (1)
  {
    // 1. DRAIN NETWORK & KEEPALIVE
    zenbedded_transport_spin();

    uint32_t current_time = k_uptime_get_32();

    // 2. PUB 3: MAX SPEED)
    if ((current_time - last_max_time) >= 1)
    {
      zcdr_serialize_joint_command(&ctx_cmd, 0, 0, cmd_vals, buf_cmd);
      zenbedded_transport_publish(pub_cmd, buf_cmd, ctx_cmd.payload_size);
      cmd_vals[0] += 0.1;
      last_max_time = current_time;
    }

    // 3. PUB 1: 50Hz LOOP (Every 20 ms)
    if ((current_time - last_50hz_time) >= 20)
    {
      zcdr_serialize_joint_state(&ctx_joints, 0, 0, pos_2, vel_2, eff_2, buf_joints);
      zenbedded_transport_publish(pub_joints, buf_joints, ctx_joints.payload_size);
      pos_2[0] += 0.01;
      last_50hz_time = current_time;
    }

    // 4. PUB 2: 10Hz LOOP (Every 100 ms)
    if ((current_time - last_10hz_time) >= 100)
    {
      zcdr_serialize_joint_state(&ctx_arm, 0, 0, pos_3, vel_3, eff_3, buf_arm);
      zenbedded_transport_publish(pub_arm, buf_arm, ctx_arm.payload_size);
      pos_3[1] -= 0.05;
      last_10hz_time = current_time;
    }

    // Yield CPU for 1ms to prevent starvation on native_sim
    k_sleep(K_MSEC(1));

#ifdef ZTEST_STATE_OK
    if (iteration_count > 50)
    {
      printf("[CI] Test sequence completed cleanly. Exiting.\n");
      return 0;
    }
#endif
  }
#else
  printf("\nCONFIG_TIER_1 is disabled. Skipping execution.\n");
#endif

  return 0;
}
