// Copyright 2026 Zenbedded
// Licensed under the Apache License, Version 2.0

#include "zenbedded_transport/zenoh_transport.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <zenoh-pico.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/random.h>
#include "zenbedded_transport/rihs_ledger.h"

#if defined(CONFIG_ZENBEDDED_TRANSPORT_LOG_LEVEL)
LOG_MODULE_REGISTER(zenoh_transport, CONFIG_ZENBEDDED_TRANSPORT_LOG_LEVEL);
#else
LOG_MODULE_REGISTER(zenoh_transport, LOG_LEVEL_INF);
#endif

static z_owned_session_t z_session;
static uint32_t current_domain_id = 0;
static char current_node_name[64] = {0};
static bool is_initialized = false;
static uint32_t last_keepalive_time_ms = 0;

#define KEEPALIVE_INTERVAL_MS 5000

#ifdef CONFIG_ZENBEDDED_TIER_1
#define MAX_PUBLISHERS 10
#define MAX_SUBSCRIBERS 10
#define KEYEXPR_MAX_LEN 512
#define RMW_GID_SIZE 16

typedef struct __attribute__((packed))
{
  int64_t sequence_number;
  int64_t timestamp_ns;
  uint8_t gid_length;
  uint8_t gid[RMW_GID_SIZE];
} rmw_zenoh_attachment_t;

struct zenbedded_pub_s
{
  bool in_use;
  z_owned_publisher_t z_pub;
  rmw_zenoh_attachment_t attachment;
  char topic_keyexpr[KEYEXPR_MAX_LEN];
  z_owned_liveliness_token_t lv_token;
  char lv_keyexpr[KEYEXPR_MAX_LEN];
};

struct zenbedded_sub_s
{
  bool in_use;
  z_owned_subscriber_t z_sub;
  zenbedded_recv_cb_t user_cb;
  void * user_data;
  char topic_keyexpr[KEYEXPR_MAX_LEN];
  z_owned_liveliness_token_t lv_token;
  char lv_keyexpr[KEYEXPR_MAX_LEN];
};

static struct zenbedded_pub_s pub_pool[MAX_PUBLISHERS] = {0};
static struct zenbedded_sub_s sub_pool[MAX_SUBSCRIBERS] = {0};

static z_owned_liveliness_token_t node_lv_token;
static char node_lv_str[KEYEXPR_MAX_LEN] = {0};

static void internal_sub_handler(z_loaned_sample_t * sample, void * arg)
{
  struct zenbedded_sub_s * sub = (struct zenbedded_sub_s *)arg;
  if (!sub || !sub->user_cb)
  {
    return;
  }

  z_bytes_reader_t reader = z_bytes_get_reader(z_sample_payload(sample));
  size_t len = z_bytes_len(z_sample_payload(sample));

  static uint8_t rx_buf[1024];
  if (len > sizeof(rx_buf))
  {
    LOG_WRN("Incoming payload too large (%zu bytes), dropping.", len);
    return;
  }
  z_bytes_reader_read(&reader, rx_buf, len);
  sub->user_cb(rx_buf, len, sub->user_data);
}
#endif  // CONFIG_ZENBEDDED_TIER_1

int zenbedded_transport_init(
  uint32_t domain_id, const char * node_name, const char * mode, const char * locator)
{
  if (is_initialized)
  {
    return 0;
  }

  current_domain_id = domain_id;
  strncpy(current_node_name, node_name, sizeof(current_node_name) - 1);

  z_owned_config_t z_config;
  z_config_default(&z_config);
  zp_config_insert(z_config_loan_mut(&z_config), Z_CONFIG_MODE_KEY, mode);

  if (locator && strlen(locator) > 0)
  {
    uint8_t key = (strcmp(mode, "client") == 0) ? Z_CONFIG_CONNECT_KEY : Z_CONFIG_LISTEN_KEY;
    zp_config_insert(z_config_loan_mut(&z_config), key, locator);
  }

  if (z_open(&z_session, z_config_move(&z_config), NULL) != Z_OK)
  {
    LOG_ERR("Failed to open Zenoh session");
    return -1;
  }

#ifdef CONFIG_ZENBEDDED_TIER_1
  z_id_t zid = z_info_zid(z_session_loan(&z_session));

  snprintf(
    node_lv_str, KEYEXPR_MAX_LEN,
    "@ros2_lv/%u/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/0/0/NN/%%/%%/%s",
    current_domain_id, zid.id[0], zid.id[1], zid.id[2], zid.id[3], zid.id[4], zid.id[5], zid.id[6],
    zid.id[7], zid.id[8], zid.id[9], zid.id[10], zid.id[11], zid.id[12], zid.id[13], zid.id[14],
    zid.id[15], current_node_name);

  LOG_DBG("[WIRE-TRACE] Transmitting Node Liveliness Declaration:");
  LOG_DBG("[WIRE-TRACE] %s", node_lv_str);

  z_view_keyexpr_t ke;
  z_view_keyexpr_from_str(&ke, node_lv_str);
  z_liveliness_declare_token(
    z_session_loan(&z_session), &node_lv_token, z_view_keyexpr_loan(&ke), NULL);
#endif

  last_keepalive_time_ms = k_uptime_get_32();
  is_initialized = true;
  LOG_INF("Transport Initialized on Domain %u (Node: %s)", domain_id, current_node_name);
  return 0;
}

void zenbedded_transport_destroy(void)
{
  if (!is_initialized)
  {
    return;
  }

#ifdef CONFIG_ZENBEDDED_TIER_1
  for (int i = 0; i < MAX_PUBLISHERS; i++)
  {
    if (pub_pool[i].in_use)
    {
      z_liveliness_undeclare_token(z_move(pub_pool[i].lv_token));
      z_undeclare_publisher(z_publisher_move(&pub_pool[i].z_pub));
      pub_pool[i].in_use = false;
    }
  }
  for (int i = 0; i < MAX_SUBSCRIBERS; i++)
  {
    if (sub_pool[i].in_use)
    {
      z_undeclare_subscriber(z_subscriber_move(&sub_pool[i].z_sub));
      sub_pool[i].in_use = false;
    }
  }
  z_liveliness_undeclare_token(z_move(node_lv_token));
#endif

  z_close(z_session_loan_mut(&z_session), NULL);
  is_initialized = false;
  LOG_INF("Transport Destroyed Successfully");
}

void zenbedded_transport_spin(void)
{
  if (!is_initialized)
  {
    return;
  }

  zp_read_options_t read_opts;
  zp_read_options_default(&read_opts);
  zp_read(z_session_loan(&z_session), &read_opts);

  uint32_t now = k_uptime_get_32();
  if ((now - last_keepalive_time_ms) >= KEEPALIVE_INTERVAL_MS)
  {
    LOG_DBG("[HEARTBEAT] Transmitting KeepAlive pulse (Uptime: %u ms)", now);
    zp_send_keep_alive_options_t ka_opts;
    zp_send_keep_alive_options_default(&ka_opts);
    zp_send_keep_alive(z_session_loan(&z_session), &ka_opts);
    last_keepalive_time_ms = now;
  }
}

#ifdef CONFIG_ZENBEDDED_TIER_1
zenbedded_pub_t zenbedded_transport_declare_publisher(
  const char * topic_name, const char * type_name)
{
  if (!is_initialized)
  {
    return NULL;
  }

  const char * type_hash = zenbedded_get_rihs_hash(type_name);

  struct zenbedded_pub_s * pub = NULL;
  for (int i = 0; i < MAX_PUBLISHERS; i++)
  {
    if (!pub_pool[i].in_use)
    {
      pub = &pub_pool[i];
      pub->in_use = true;
      break;
    }
  }
  if (!pub)
  {
    LOG_ERR("Publisher pool exhausted! Max limit: %d", MAX_PUBLISHERS);
    return NULL;
  }

  const char * clean_topic = (topic_name[0] == '/') ? topic_name + 1 : topic_name;

  snprintf(
    pub->topic_keyexpr, KEYEXPR_MAX_LEN, "%u/%s/%s/%s", current_domain_id, clean_topic, type_name,
    type_hash);

  z_view_keyexpr_t ke;
  z_view_keyexpr_from_str_unchecked(&ke, pub->topic_keyexpr);

  if (
    z_declare_publisher(z_session_loan(&z_session), &pub->z_pub, z_view_keyexpr_loan(&ke), NULL) !=
    Z_OK)
  {
    pub->in_use = false;
    LOG_ERR("Failed to declare Zenoh publisher for %s", pub->topic_keyexpr);
    return NULL;
  }

  pub->attachment.sequence_number = 0;
  pub->attachment.gid_length = RMW_GID_SIZE;
  sys_rand_get(pub->attachment.gid, RMW_GID_SIZE);

  z_id_t zid = z_info_zid(z_session_loan(&z_session));
  snprintf(
    pub->lv_keyexpr, KEYEXPR_MAX_LEN,
    "@ros2_lv/%u/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/0/10/MP/%%/%%/%s/"
    "%%%s/%s/%s/::,7:,:,:,,",
    current_domain_id, zid.id[0], zid.id[1], zid.id[2], zid.id[3], zid.id[4], zid.id[5], zid.id[6],
    zid.id[7], zid.id[8], zid.id[9], zid.id[10], zid.id[11], zid.id[12], zid.id[13], zid.id[14],
    zid.id[15], current_node_name, clean_topic, type_name, type_hash);

  LOG_DBG("[WIRE-TRACE] Transmitting Publisher Liveliness Declaration:");
  LOG_DBG("[WIRE-TRACE] %s", pub->lv_keyexpr);

  z_view_keyexpr_t lv_ke;
  z_view_keyexpr_from_str(&lv_ke, pub->lv_keyexpr);
  z_liveliness_declare_token(
    z_session_loan(&z_session), &pub->lv_token, z_view_keyexpr_loan(&lv_ke), NULL);

  LOG_INF("Declared Tier 1 Publisher: %s", pub->topic_keyexpr);
  return pub;
}

int zenbedded_transport_publish(zenbedded_pub_t pub, const uint8_t * payload, size_t payload_size)
{
  if (!pub || !pub->in_use)
  {
    return -1;
  }

  struct timespec tv;
  clock_gettime(CLOCK_REALTIME, &tv);

  pub->attachment.sequence_number++;
  pub->attachment.timestamp_ns = (int64_t)tv.tv_sec * 1000000000LL + tv.tv_nsec;

  z_owned_bytes_t z_attachment;
  z_bytes_from_static_buf(
    &z_attachment, (const uint8_t *)&pub->attachment, sizeof(rmw_zenoh_attachment_t));

  z_publisher_put_options_t options;
  z_publisher_put_options_default(&options);
  options.attachment = z_bytes_move(&z_attachment);

  z_owned_bytes_t z_payload;
  z_bytes_from_static_buf(&z_payload, payload, payload_size);

  if (z_publisher_put(z_publisher_loan(&pub->z_pub), z_bytes_move(&z_payload), &options) != Z_OK)
  {
    return -1;
  }
  return 0;
}

zenbedded_sub_t zenbedded_transport_declare_subscriber(
  const char * topic_name, const char * type_name, zenbedded_recv_cb_t callback, void * user_data)
{
  if (!is_initialized)
  {
    return NULL;
  }

  const char * type_hash = zenbedded_get_rihs_hash(type_name);

  struct zenbedded_sub_s * sub = NULL;
  for (int i = 0; i < MAX_SUBSCRIBERS; i++)
  {
    if (!sub_pool[i].in_use)
    {
      sub = &sub_pool[i];
      sub->in_use = true;
      break;
    }
  }
  if (!sub)
  {
    LOG_ERR("Subscriber pool exhausted! Max limit: %d", MAX_SUBSCRIBERS);
    return NULL;
  }

  sub->user_cb = callback;
  sub->user_data = user_data;

  const char * clean_topic = (topic_name[0] == '/') ? topic_name + 1 : topic_name;

  snprintf(
    sub->topic_keyexpr, KEYEXPR_MAX_LEN, "%u/%s/%s/%s", current_domain_id, clean_topic, type_name,
    type_hash);

  z_view_keyexpr_t ke;
  z_view_keyexpr_from_str_unchecked(&ke, sub->topic_keyexpr);

  z_owned_closure_sample_t sub_cb;
  z_closure_sample(&sub_cb, internal_sub_handler, NULL, sub);

  if (
    z_declare_subscriber(
      z_session_loan(&z_session), &sub->z_sub, z_view_keyexpr_loan(&ke),
      z_closure_sample_move(&sub_cb), NULL) != Z_OK)
  {
    sub->in_use = false;
    LOG_ERR("Failed to declare Zenoh subscriber for %s", sub->topic_keyexpr);
    return NULL;
  }

  z_id_t zid = z_info_zid(z_session_loan(&z_session));
  snprintf(
    sub->lv_keyexpr, KEYEXPR_MAX_LEN,
    "@ros2_lv/%u/%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x/0/10/MS/%%/%%/%s/"
    "%%%s/%s/%s/::,7:,:,:,,",
    current_domain_id, zid.id[0], zid.id[1], zid.id[2], zid.id[3], zid.id[4], zid.id[5], zid.id[6],
    zid.id[7], zid.id[8], zid.id[9], zid.id[10], zid.id[11], zid.id[12], zid.id[13], zid.id[14],
    zid.id[15], current_node_name, clean_topic, type_name, type_hash);

  LOG_DBG("[WIRE-TRACE] Transmitting Subscriber Liveliness Declaration:");
  LOG_DBG("[WIRE-TRACE] %s", sub->lv_keyexpr);

  z_view_keyexpr_t lv_ke;
  z_view_keyexpr_from_str(&lv_ke, sub->lv_keyexpr);
  z_liveliness_declare_token(
    z_session_loan(&z_session), &sub->lv_token, z_view_keyexpr_loan(&lv_ke), NULL);

  LOG_INF("Declared Tier 1 Subscriber: %s", sub->topic_keyexpr);
  return sub;
}
#endif  // CONFIG_ZENBEDDED_TIER_1
