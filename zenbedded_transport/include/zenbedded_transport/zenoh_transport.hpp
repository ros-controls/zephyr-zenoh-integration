// Copyright 2026 ROS2CONTROL
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

#ifndef ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_HPP_
#define ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_HPP_

#include <cstddef>
#include <cstdint>

// Generic callback operating purely on raw bytes
typedef void (*zenbedded_sub_cb_t)(const uint8_t * payload, size_t size, void * user_data);

// Register generic subscriber callback with user context
void zenbedded_set_subscriber_cb(zenbedded_sub_cb_t cb, void * user_data);

// Initializes Zenoh session, node liveliness, pub/sub entities, and background tasks.
int zenbedded_transport_init();

// Publishes raw payload directly with zenoh publisher
int zenbedded_publish(const uint8_t * payload, size_t size);

// Closes Zenoh session and cleans up resources.
void zenbedded_transport_close();

#endif  // ZENBEDDED_TRANSPORT__ZENOH_TRANSPORT_HPP_
