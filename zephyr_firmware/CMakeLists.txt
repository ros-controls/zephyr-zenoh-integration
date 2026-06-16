# Copyright 2026 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.



### As a standalone application
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zephyr_firmware)

target_sources(app PRIVATE
    src/main.cpp
    lib/component_manager.cpp
    lib/hardware_component.cpp
)

target_include_directories(app PRIVATE include)






### As a library

# zephyr_library()
# zephyr_library_sources(
#     lib/component_manager.cpp
#     lib/hardware_component.cpp
# )
# zephyr_library_link_libraries(zenoh-pico)
# zephyr_library_include_directories(include)
