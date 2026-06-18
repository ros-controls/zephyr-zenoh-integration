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

#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp_lifecycle/state.hpp>
#include <zenoh.hxx>
#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "zephyr_zenoh_hardware_interface/zenoh_zephyr_hardware.hpp"

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class TestZenohZephyrHardware : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    auto cfg = zenoh::Config::create_default();
    cfg.insert_json5("listen/endpoints", R"(["tcp/127.0.0.1:21555"])");
    cfg.insert_json5("mode", "\"peer\"");
    test_router_ = std::make_unique<zenoh::Session>(std::move(cfg));
    ASSERT_NE(test_router_, nullptr);
  }

  static void TearDownTestSuite() { test_router_.reset(); }

  void SetUp() override
  {
    // Build HardwareComponentInterfaceParams with test data
    hardware_interface::HardwareInfo info;
    info.name = "test_hw";
    info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:21555";
    info.hardware_parameters["state_topic"] = "joint_states";
    info.hardware_parameters["command_topic"] = "joint_commands";
    info.hardware_parameters["zenoh_mode"] = "client";

    hardware_interface::ComponentInfo joint;
    joint.name = "test_joint";
    hardware_interface::InterfaceInfo si;
    si.name = "position";
    joint.state_interfaces.push_back(si);

    hardware_interface::InterfaceInfo ci;
    ci.name = "position";
    joint.command_interfaces.push_back(ci);
    info.joints.push_back(joint);

    hardware_interface::HardwareComponentInterfaceParams params;
    params.hardware_info = info;

    hw_ = std::make_unique<zephyr_zenoh::ZenohZephyrHardware>();
    ASSERT_EQ(hw_->on_init(params), CallbackReturn::SUCCESS);
  }

  static std::unique_ptr<zenoh::Session> test_router_;
  std::unique_ptr<zephyr_zenoh::ZenohZephyrHardware> hw_;
};

std::unique_ptr<zenoh::Session> TestZenohZephyrHardware::test_router_ = nullptr;

TEST_F(TestZenohZephyrHardware, missing_parameter_fails)
{
  hardware_interface::HardwareInfo info;
  info.name = "test_hw";
  // Intentionally NOT setting zenoh_endpoint, state_topic, command_topic
  hardware_interface::ComponentInfo joint;
  joint.name = "test_joint";
  hardware_interface::InterfaceInfo si;
  si.name = "position";
  joint.state_interfaces.push_back(si);

  hardware_interface::InterfaceInfo ci;
  ci.name = "position";
  joint.command_interfaces.push_back(ci);
  info.joints.push_back(joint);

  hardware_interface::HardwareComponentInterfaceParams params;
  params.hardware_info = info;

  auto hw = std::make_unique<zephyr_zenoh::ZenohZephyrHardware>();
  EXPECT_EQ(hw->on_init(params), CallbackReturn::ERROR);
}

TEST_F(TestZenohZephyrHardware, state_interfaces_exported)
{
  auto si = hw_->export_state_interfaces();
  EXPECT_EQ(si.size(), 1);
  EXPECT_EQ(si[0].get_name(), "test_joint/position");
  EXPECT_EQ(si[0].get_interface_name(), "position");
}

TEST_F(TestZenohZephyrHardware, command_interfaces_exported)
{
  auto ci = hw_->export_command_interfaces();
  EXPECT_EQ(ci.size(), 1);
  EXPECT_EQ(ci[0].get_name(), "test_joint/position");
  EXPECT_EQ(ci[0].get_interface_name(), "position");
}

TEST_F(TestZenohZephyrHardware, activate_succeeds_with_router)
{
  rclcpp_lifecycle::State state(1, "unconfigured");
  EXPECT_EQ(hw_->on_activate(state), CallbackReturn::SUCCESS);
  EXPECT_EQ(hw_->on_deactivate(state), CallbackReturn::SUCCESS);
}

TEST_F(TestZenohZephyrHardware, activate_fails_with_bad_endpoint)
{
  // Use a port unlikely to be running a Zenoh router
  hardware_interface::HardwareInfo info;
  info.name = "test_hw";
  info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:1";
  info.hardware_parameters["state_topic"] = "joint_states";
  info.hardware_parameters["command_topic"] = "joint_commands";
  info.hardware_parameters["zenoh_mode"] = "client";

  hardware_interface::ComponentInfo joint;
  joint.name = "test_joint";
  hardware_interface::InterfaceInfo si;
  si.name = "position";
  joint.state_interfaces.push_back(si);
  hardware_interface::InterfaceInfo ci;
  ci.name = "position";
  joint.command_interfaces.push_back(ci);
  info.joints.push_back(joint);

  hardware_interface::HardwareComponentInterfaceParams params;
  params.hardware_info = info;

  auto hw = std::make_unique<zephyr_zenoh::ZenohZephyrHardware>();
  ASSERT_EQ(hw->on_init(params), CallbackReturn::SUCCESS);

  rclcpp_lifecycle::State state(1, "unconfigured");
  EXPECT_EQ(hw->on_activate(state), CallbackReturn::ERROR);
}
