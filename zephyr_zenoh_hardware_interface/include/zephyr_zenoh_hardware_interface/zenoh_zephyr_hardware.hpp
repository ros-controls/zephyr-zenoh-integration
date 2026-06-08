// Copyright 2026 kamal2730
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

#ifndef ZEPHYR_ZENOH_HARDWARE_INTERFACE__ZENOH_ZEPHYR_HARDWARE_HPP_
#define ZEPHYR_ZENOH_HARDWARE_INTERFACE__ZENOH_ZEPHYR_HARDWARE_HPP_

#include <mutex>
#include <string>
#include <vector>

#include <zenoh.hxx>
#include "hardware_interface/handle.hpp"
#include "hardware_interface/hardware_info.hpp"
#include "hardware_interface/system_interface.hpp"
#include "hardware_interface/types/hardware_interface_return_values.hpp"
#include "rclcpp/macros.hpp"
#include "rclcpp_lifecycle/node_interfaces/lifecycle_node_interface.hpp"
#include "rclcpp_lifecycle/state.hpp"

namespace zephyr_zenoh
{
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class ZenohZephyrHardware : public hardware_interface::SystemInterface
{
public:
  RCLCPP_SHARED_PTR_DEFINITIONS(ZenohZephyrHardware)

  CallbackReturn on_init(
    const hardware_interface::HardwareComponentInterfaceParams & params) override;

  std::vector<hardware_interface::StateInterface> export_state_interfaces() override;

  std::vector<hardware_interface::CommandInterface> export_command_interfaces() override;

  CallbackReturn on_activate(const rclcpp_lifecycle::State & previous_state) override;

  CallbackReturn on_deactivate(const rclcpp_lifecycle::State & previous_state) override;

  hardware_interface::return_type read(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

  hardware_interface::return_type write(
    const rclcpp::Time & time, const rclcpp::Duration & period) override;

private:
  std::vector<double> hw_commands_;
  std::vector<double> hw_states_;
  std::vector<double> hw_state_buffer_;
  mutable std::mutex data_mutex_;
  std::string zenoh_endpoint_;
  std::string zenoh_mode_;
  std::string state_topic_;
  std::string command_topic_;
};

}  // namespace zephyr_zenoh

#endif  // ZEPHYR_ZENOH_HARDWARE_INTERFACE__ZENOH_ZEPHYR_HARDWARE_HPP_
