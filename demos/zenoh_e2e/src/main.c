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
// limitations under the License.#include <stdio.h>

#include <stdio.h>

#include <zenoh-pico.h>
#include <zephyr/kernel.h>

#define ENDPOINT "tcp/127.0.0.1:7447"

int main(void)
{
  z_owned_config_t config;
  z_owned_session_t session;

  z_config_default(&config);

  zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, "peer");
  zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, ENDPOINT);

  printf("Connecting peer to %s\n", ENDPOINT);

  if (z_open(&session, z_move(config), NULL) < 0)
  {
    printf("Zenoh connection failed\n");
    return 1;
  }

  printf("Zenoh peer connected\n");

  for (;;)
  {
    k_sleep(K_FOREVER);
  }

  return 0;
}
