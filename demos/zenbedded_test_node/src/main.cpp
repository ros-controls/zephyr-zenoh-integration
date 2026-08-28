// Copyright 2026 Zenbedded
// Licensed under the Apache License, Version 2.0

#include <stdint.h>
#include <stdio.h>
#include <zephyr/kernel.h>

#include "zenbedded_transport/serialization.h"
#include "zenbedded_transport/zenoh_transport.h"

#ifdef CONFIG_ZENBEDDED_TIER_1

// 1. Define global context so the callback knows how to parse the incoming bytes
zcdr_joint_command_ctx_t ctx_sub_1;

// 2. The independent subscriber callback
void cmd_1_rx_callback(const uint8_t * payload, size_t size, void * user_data)
{
  zcdr_joint_command_ctx_t * ctx = reinterpret_cast<zcdr_joint_command_ctx_t *>(user_data);
  double vals[2] = {0};

  if (zcdr_deserialize_joint_command(ctx, payload, size, NULL, NULL, vals))
  {
    printf("[SUB 1] /joint_commands RX -> stepper: %.2f | pendulum: %.2f\n", vals[0], vals[1]);
  }
}
#endif  // CONFIG_ZENBEDDED_TIER_1

int main(void)
{
  printf("\n=======================================\n");
  printf("  Zenbedded Transport: RCL Reference   \n");
  printf("  HIGH-SPEED 1-PUB/1-SUB EXECUTOR      \n");
  printf("=======================================\n");

#ifdef CONFIG_ZENBEDDED_TIER_1

  // Read from the Kconfig environment
  int domain_id = CONFIG_ZENBEDDED_RCL_DOMAIN_ID;
  const char * node_name = CONFIG_ZENBEDDED_RCL_NODE_NAME;
  const char * mode = CONFIG_ZENBEDDED_RCL_ZENOH_MODE;
  const char * locator = CONFIG_ZENBEDDED_RCL_ZENOH_LOCATOR;

  if (zenbedded_transport_init(domain_id, node_name, mode, locator) != 0)
  {
    printf("[FATAL] Transport init failed!\n");
    return -1;
  }

  // --- MEMORY & SERIALIZATION SETUP ---
  const char * chassis_joints[] = {"stepper", "pendulum"};

  zcdr_joint_state_ctx_t ctx_joints;
  uint8_t buf_joints[512] = {0};

  // We need a dummy buffer to initialize the read offset for the subscriber
  uint8_t dummy_1[256] = {0};

  zcdr_init_joint_state(&ctx_joints, "base_link", chassis_joints, 2, buf_joints);
  zcdr_init_joint_command(&ctx_sub_1, "base_link", chassis_joints, 2, "position", dummy_1);

  // --- PUBLISHER DECLARATION ---
  zenbedded_pub_t pub_joints =
    zenbedded_transport_declare_publisher("joint_states", "sensor_msgs::msg::dds_::JointState_");
  k_msleep(10);  // Prevent router UDP saturation

  // --- SUBSCRIBER DECLARATION ---
  zenbedded_transport_declare_subscriber(
    "joint_commands", "control_msgs::msg::dds_::JointCommand_", cmd_1_rx_callback, &ctx_sub_1);
  k_msleep(10);  // Prevent router UDP saturation

  // --- DATA PAYLOADS ---
  double pos_2[] = {0.0, 0.0};
  double vel_2[] = {0.0, 0.0};
  double eff_2[] = {0.0, 0.0};

  // --- TIMING SETUP ---
  uint32_t last_50hz_time = k_uptime_get_32();

  printf("\n[SYS] Entering High-Frequency Event Loop...\n\n");

  while (1)
  {
    // 1. DRAIN NETWORK & KEEPALIVE (This triggers the callbacks!)
    zenbedded_transport_spin();

    uint32_t current_time = k_uptime_get_32();

    // 2. PUB 1: 50Hz LOOP (Every 20 ms)
    if ((current_time - last_50hz_time) >= 20)
    {
      zcdr_serialize_joint_state(&ctx_joints, 0, 0, pos_2, vel_2, eff_2, buf_joints);
      zenbedded_transport_publish(pub_joints, buf_joints, ctx_joints.payload_size);
      pos_2[0] += 0.01;
      last_50hz_time = current_time;
    }

    // Yield CPU for 1ms to prevent starvation on native_sim
    k_sleep(K_MSEC(1));
  }
#else
  printf("\nCONFIG_ZENBEDDED_TIER_1 is disabled. Skipping execution.\n");
#endif

  return 0;
}
