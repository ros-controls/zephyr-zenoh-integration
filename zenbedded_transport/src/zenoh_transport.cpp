// Copyright 2026
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

#include "zenbedded_transport/zenoh_transport.h"
#include <zenoh-pico.h>
#include <zephyr/logging/log.h>
#include <cinttypes>
#include <cstdio>
#include <cstring>

LOG_MODULE_REGISTER(zenbedded_transport, LOG_LEVEL_INF);

#define RMW_GID_SIZE 16
#define KEYEXPR_SIZE 400
#define TOPIC_MAX_NAME 124

// RMW Attachment structure required by rmw_zenoh
typedef struct __attribute__((__packed__))
{
  int64_t sequence_number;
  int64_t time;
  uint8_t rmw_gid_size;
  uint8_t rmw_gid[RMW_GID_SIZE];
} rmw_attachment_t;

static z_owned_session_t s_session;
static z_owned_publisher_t z_pub;
static z_owned_subscriber_t z_sub;

static z_owned_liveliness_token_t node_lv_token;
static z_owned_liveliness_token_t pub_lv_token;
static z_owned_liveliness_token_t sub_lv_token;

static rmw_attachment_t pub_attachment;

static zenbedded_sub_cb_t sub_cb = nullptr;  // callback provided by client for incoming commands
static void * sub_user_data = nullptr;       // optional context info for callback

static zenbedded_topic_info_t pub_topic = {
  .name = CONFIG_ZENBEDDED_PUB_TOPIC,
  .type = "zenbedded_interfaces::msg::dds_::ZenbeddedCommand_",
  .rihs_hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};
static zenbedded_topic_info_t sub_topic = {
  .name = CONFIG_ZENBEDDED_SUB_TOPIC,
  .type = "zenbedded_interfaces::msg::dds_::ZenbeddedState_",
  .rihs_hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"};

static void gen_random_gid(uint8_t * gid)
{
  for (int i = 0; i < RMW_GID_SIZE; i++)
  {
    gid[i] = z_random_u8();
  }
}

static int generate_topic_keyexpr(
  uint32_t domain_id, const zenbedded_topic_info_t * topic, char * keyexpr)
{
  return snprintf(
    keyexpr, KEYEXPR_SIZE, "%" PRIu32 "/%s/%s_/RIHS01_%s", domain_id, topic->name, topic->type,
    topic->rihs_hash);
}

// Generates Node Liveliness: @ros2_lv/<domain_id>/<session_id>/0/0/NN/%/%/<node_name>
static int generate_node_liveliness_keyexpr(
  uint32_t domain_id, const char * node_name, char * keyexpr)
{
  z_id_t id = z_info_zid(z_session_loan(&s_session));
  return snprintf(
    keyexpr, KEYEXPR_SIZE,
    "@ros2_lv/%" PRIu32
    "/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/"
    "0/0/NN/%%/%%/%s",
    domain_id, id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7],
    id.id[8], id.id[9], id.id[10], id.id[11], id.id[12], id.id[13], id.id[14], id.id[15],
    node_name);
}

// Generates Entity Liveliness (MP for Pub, MS for Sub):
// @ros2_lv/<domain_id>/<session_id>/0/10/<entity_kind>/%/%/<node_name>/%<mangled_topic>/<type>_/RIHS01_<hash>/::,7:,:,:,,
static int generate_entity_liveliness_keyexpr(
  uint32_t domain_id, const char * node_name, const zenbedded_topic_info_t * topic,
  const char * entity_str, char * keyexpr)
{
  char mangled_topic[TOPIC_MAX_NAME] = {0};
  strncpy(mangled_topic, topic->name, TOPIC_MAX_NAME - 1);

  // Mangle slashes '/' to '%' for rmw_zenoh liveliness tokens
  for (char * p = mangled_topic; *p; p++)
  {
    if (*p == '/')
    {
      *p = '%';
    }
  }

  z_id_t id = z_info_zid(z_session_loan(&s_session));

  return snprintf(
    keyexpr, KEYEXPR_SIZE,
    "@ros2_lv/%" PRIu32
    "/"
    "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/"
    "0/10/%s/%%/%%/%s/%%%s/%s_/RIHS01_%s/::,7:,:,:,,",
    domain_id, id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7],
    id.id[8], id.id[9], id.id[10], id.id[11], id.id[12], id.id[13], id.id[14], id.id[15],
    entity_str, node_name, mangled_topic, topic->type, topic->rihs_hash);
}

// Internal Subscriber Callback handler
static void zenoh_sub_handler(z_loaned_sample_t * sample, void * ctx)
{
  if (!sub_cb)
  {
    return;
  }

  size_t len = z_bytes_len(z_sample_payload(sample));
  if (len == 0 || len > CONFIG_ZENBEDDED_MAX_CMD_BUFFER_SIZE)
  {
    LOG_WRN(
      "Subscriber payload size %zu exceeds cmd buffer limit (%d)", len,
      CONFIG_ZENBEDDED_MAX_CMD_BUFFER_SIZE);
    return;
  }

  static uint8_t cmd_rx_buf[CONFIG_ZENBEDDED_MAX_CMD_BUFFER_SIZE];
  z_bytes_reader_t reader = z_bytes_get_reader(z_sample_payload(sample));
  z_bytes_reader_read(&reader, cmd_rx_buf, len);

  sub_cb(cmd_rx_buf, len, sub_user_data);
}

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_1
static int configure_zenoh_tier1()
{
  char keyexpr[KEYEXPR_SIZE];
  z_view_keyexpr_t ke;

  // Declare Node Liveliness Token
  generate_node_liveliness_keyexpr(
    CONFIG_ZENBEDDED_ROS2_DOMAIN_ID, CONFIG_ZENBEDDED_ROS2_NODE_NAME, keyexpr);
  z_view_keyexpr_from_str(&ke, keyexpr);
  if (
    z_liveliness_declare_token(
      z_session_loan(&s_session), &node_lv_token, z_view_keyexpr_loan(&ke), nullptr) != Z_OK)
  {
    LOG_ERR("Failed to declare node liveliness token");
    return -1;
  }

  // Declare Publisher with rmw attachment
  generate_topic_keyexpr(CONFIG_ZENBEDDED_ROS2_DOMAIN_ID, &pub_topic, keyexpr);
  z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
  pub_attachment.rmw_gid_size = RMW_GID_SIZE;
  gen_random_gid(pub_attachment.rmw_gid);
  pub_attachment.sequence_number = 0;
  if (
    z_declare_publisher(z_session_loan(&s_session), &z_pub, z_view_keyexpr_loan(&ke), nullptr) !=
    Z_OK)
  {
    LOG_ERR("Failed to declare publisher");
    return -1;
  }

  // Publisher Liveliness Token
  generate_entity_liveliness_keyexpr(
    CONFIG_ZENBEDDED_ROS2_DOMAIN_ID, CONFIG_ZENBEDDED_ROS2_NODE_NAME, &pub_topic, "MP", keyexpr);
  z_view_keyexpr_from_str(&ke, keyexpr);
  z_liveliness_declare_token(
    z_session_loan(&s_session), &pub_lv_token, z_view_keyexpr_loan(&ke), nullptr);

  // Declare Subscriber
  generate_topic_keyexpr(CONFIG_ZENBEDDED_ROS2_DOMAIN_ID, &sub_topic, keyexpr);
  z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
  z_owned_closure_sample_t callback;
  z_closure_sample(&callback, zenoh_sub_handler, nullptr, nullptr);
  if (
    z_declare_subscriber(
      z_session_loan(&s_session), &z_sub, z_view_keyexpr_loan(&ke),
      z_closure_sample_move(&callback), nullptr) != Z_OK)
  {
    LOG_ERR("Failed to declare subscriber");
    return -1;
  }

  // Subscriber Liveliness token
  generate_entity_liveliness_keyexpr(
    CONFIG_ZENBEDDED_ROS2_DOMAIN_ID, CONFIG_ZENBEDDED_ROS2_NODE_NAME, &sub_topic, "MS", keyexpr);
  z_view_keyexpr_from_str(&ke, keyexpr);
  z_liveliness_declare_token(
    z_session_loan(&s_session), &sub_lv_token, z_view_keyexpr_loan(&ke), nullptr);

  return Z_OK;
}
#endif

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_2
static int configure_zenoh_tier2()
{
  z_view_keyexpr_t ke;

  // Declare publisher
  z_view_keyexpr_from_str(&ke, CONFIG_ZENBEDDED_PUB_TOPIC);
  if (
    z_declare_publisher(z_session_loan(&s_session), &z_pub, z_view_keyexpr_loan(&ke), nullptr) !=
    Z_OK)
  {
    LOG_ERR("Failed to declare publisher");
    return -1;
  }

  // Declare Subscriber
  z_owned_closure_sample_t callback;
  z_view_keyexpr_from_str(&ke, CONFIG_ZENBEDDED_SUB_TOPIC);
  z_closure_sample(&callback, zenoh_sub_handler, nullptr, nullptr);
  if (
    z_declare_subscriber(
      z_session_loan(&s_session), &z_sub, z_view_keyexpr_loan(&ke),
      z_closure_sample_move(&callback), nullptr) != Z_OK)
  {
    LOG_ERR("Failed to declare subscriber");
    return -1;
  }

  return Z_OK;
}
#endif

void zenbedded_set_subscriber_cb(zenbedded_sub_cb_t cb, void * user_data)
{
  sub_cb = cb;
  sub_user_data = user_data;
}

int zenbedded_transport_init()
{
  z_owned_config_t z_config;
  z_config_default(&z_config);

  zp_config_insert(z_config_loan_mut(&z_config), Z_CONFIG_MODE_KEY, CONFIG_ZENBEDDED_ZENOH_MODE);
  if (strcmp(CONFIG_ZENBEDDED_ZENOH_LOCATOR, "") != 0)
  {
    zp_config_insert(
      z_config_loan_mut(&z_config),
      (strcmp(CONFIG_ZENBEDDED_ZENOH_MODE, "client") == 0) ? Z_CONFIG_CONNECT_KEY
                                                           : Z_CONFIG_LISTEN_KEY,
      CONFIG_ZENBEDDED_ZENOH_LOCATOR);
  }

  if (z_open(&s_session, z_config_move(&z_config), nullptr) != Z_OK)
  {
    LOG_ERR("Failed to open Zenoh session");
    return -1;
  }

  // Start lease and read tasks
  zp_start_read_task(z_session_loan_mut(&s_session), nullptr);
  zp_start_lease_task(z_session_loan_mut(&s_session), nullptr);

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_1
  if (configure_zenoh_tier1() != Z_OK)
  {
    return -1;
  }
#endif
#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_2
  if (configure_zenoh_tier2() != Z_OK)
  {
    return -1;
  }
#endif

  return Z_OK;
}

int zenbedded_publish(const uint8_t * payload, size_t size)
{
  z_publisher_put_options_t options;
  z_publisher_put_options_default(&options);

#ifdef CONFIG_ZENBEDDED_TRANSPORT_TIER_1
  pub_attachment.sequence_number++;
  z_clock_t now = z_clock_now();
  pub_attachment.time = now.tv_sec * 1000000000LL + now.tv_nsec;

  z_owned_bytes_t z_attachment;
  z_bytes_from_static_buf(
    &z_attachment, reinterpret_cast<const uint8_t *>(&pub_attachment), sizeof(rmw_attachment_t));
  options.attachment = z_bytes_move(&z_attachment);
#endif

  z_owned_bytes_t zbytes;
  z_bytes_from_static_buf(&zbytes, payload, size);

  if (z_publisher_put(z_publisher_loan(&z_pub), z_bytes_move(&zbytes), &options) != Z_OK)
  {
    LOG_ERR("Failed to publish payload");
    return -1;
  }
  return Z_OK;
}

void zenbedded_transport_close()
{
  z_liveliness_token_drop(z_liveliness_token_move(&sub_lv_token));
  z_liveliness_token_drop(z_liveliness_token_move(&pub_lv_token));
  z_liveliness_token_drop(z_liveliness_token_move(&node_lv_token));

  z_undeclare_subscriber(z_subscriber_move(&z_sub));
  z_undeclare_publisher(z_publisher_move(&z_pub));

  zp_stop_lease_task(z_session_loan_mut(&s_session));
  zp_stop_read_task(z_session_loan_mut(&s_session));

  z_close(z_session_loan_mut(&s_session), nullptr);
}
