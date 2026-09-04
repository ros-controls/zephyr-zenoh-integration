// Copyright 2026 Open Source Robotics Foundation, Inc.
// Licensed under the Apache License, Version 2.0

#ifndef ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_H_
#define ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Initializes the Zenbedded transport engine.
   * @param domain_id The ROS 2 Domain ID.
   * @param node_name The ROS 2 Node Name (appears in 'ros2 node list').
   * @param mode "client" or "peer".
   * @param locator The endpoint (e.g., "tcp/192.168.1.100:7447").
   * @return 0 on success, -1 on failure.
   */
  int zenbedded_transport_init(
    uint32_t domain_id, const char * node_name, const char * mode, const char * locator);

  /**
   * @brief Safely tears down the Zenoh session and un-declares Liveliness.
   */
  void zenbedded_transport_destroy(void);

  /**
   * @brief Deterministic network polling executor.
   *
   * Must be called continuously by the application's event loop to drain
   * inbound sockets and maintain keep-alive heartbeats without OS thread blocking.
   */
  void zenbedded_transport_spin(void);

#ifdef CONFIG_ZENBEDDED_TIER_1

  // Opaque handles. The user/RCL cannot see inside these structs.
  typedef struct zenbedded_pub_s * zenbedded_pub_t;
  typedef struct zenbedded_sub_s * zenbedded_sub_t;

  /**
   * @brief Declares a native ROS 2 Publisher on the graph.
   *
   * @param topic_name The ROS 2 topic name (e.g., "joint_states").
   * @param type_name The DDS type namespace (e.g., "sensor_msgs::msg::dds_::JointState_").
   * @param type_hash The 64-character cryptographic type hash.
   * @return Opaque handle to the publisher pool entry, or NULL on failure.
   */
  zenbedded_pub_t zenbedded_transport_declare_publisher(
    const char * topic_name, const char * type_name);

  /**
   * @brief Pushes a serialized CDR payload over the Zenoh transport.
   *
   * @param pub The publisher handle.
   * @param payload Pointer to the CDR serialized byte buffer.
   * @param payload_size Length of the payload in bytes.
   * @return 0 on success, negative error code on failure.
   */
  int zenbedded_transport_publish(
    zenbedded_pub_t pub, const uint8_t * payload, size_t payload_size);

  /**
   * @brief Callback signature for inbound subscriber messages.
   */
  typedef void (*zenbedded_recv_cb_t)(const uint8_t * payload, size_t size, void * user_data);

  /**
   * @brief Declares a native ROS 2 Subscriber on the graph.
   *
   * @param topic_name The ROS 2 topic name.
   * @param type_name The DDS type namespace.
   * @param type_hash The 64-character cryptographic type hash.
   * @param callback Function invoked when a message is received.
   * @param user_data Arbitrary data passed back to the callback context.
   * @return Opaque handle to the subscriber pool entry, or NULL on failure.
   */
  zenbedded_sub_t zenbedded_transport_declare_subscriber(
    const char * topic_name, const char * type_name, zenbedded_recv_cb_t callback,
    void * user_data);

#endif  // CONFIG_ZENBEDDED_TIER_1

#ifdef __cplusplus
}
#endif

#endif  // ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_H_
