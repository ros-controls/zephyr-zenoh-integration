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

#include <stdio.h>

#include <zenoh-pico.h>
#include <zephyr/kernel.h>

#define ENDPOINT "tcp/127.0.0.1:7447"
#define TEST_TOPIC "zenbedded/e2e/test"

static void on_test_message(z_loaned_sample_t * sample, void * arg)
{
  ARG_UNUSED(arg);

  z_owned_string_t payload;
  if (z_bytes_to_string(z_sample_payload(sample), &payload) < 0)
  {
    printf("Received test message: <invalid payload>\n");
    return;
  }

  printf(
    "Received test message: %.*s\n", (int)z_string_len(z_loan(payload)),
    z_string_data(z_loan(payload)));
  z_drop(z_move(payload));
}

int main(void)
{
  z_owned_config_t config;
  z_owned_session_t session;

  z_config_default(&config);

  zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, "client");
  zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, ENDPOINT);

  printf("Connecting client to %s\n", ENDPOINT);

  if (z_open(&session, z_move(config), NULL) < 0)
  {
    printf("Zenoh connection failed\n");
    return 1;
  }

  if (zp_start_read_task(z_loan_mut(session), NULL) < 0)
  {
    printf("Zenoh read task failed to start\n");
    z_close(z_loan_mut(session), NULL);
    return 1;
  }

  if (zp_start_lease_task(z_loan_mut(session), NULL) < 0)
  {
    printf("Zenoh lease task failed to start\n");
    zp_stop_read_task(z_loan_mut(session));
    z_close(z_loan_mut(session), NULL);
    return 1;
  }

  z_view_keyexpr_t keyexpr;
  z_view_keyexpr_from_str_unchecked(&keyexpr, TEST_TOPIC);

  z_owned_closure_sample_t callback;
  z_closure(&callback, on_test_message, NULL, NULL);

  z_owned_subscriber_t subscriber;
  if (
    z_declare_subscriber(z_loan(session), &subscriber, z_loan(keyexpr), z_move(callback), NULL) < 0)
  {
    printf("Zenoh subscriber declaration failed\n");
    z_close(z_loan_mut(session), NULL);
    return 1;
  }

  printf("Zenoh client connected\n");

  for (;;)
  {
    k_sleep(K_FOREVER);
  }

  return 0;
}
