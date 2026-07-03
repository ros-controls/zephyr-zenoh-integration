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
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rclcpp_lifecycle/state.hpp>
#include <zenoh.hxx>
#include "zenbedded_hardware_interface/generated/interface_data.h"
#include "zenbedded_hardware_interface/zenbedded_hardware.hpp"

using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;

class TestZenbeddedHardware : public ::testing::Test
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
    hardware_interface::HardwareInfo info;
    info.name = "test_hw";
    info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:21555";
    info.hardware_parameters["state_topic"] = "joint_states";
    info.hardware_parameters["command_topic"] = "joint_commands";
    info.hardware_parameters["zenoh_mode"] = "client";
    info.hardware_parameters["schema_path"] =
      std::string(TEST_CONFIG_DIR) + "/interface_schema.yaml";

    hardware_interface::HardwareComponentInterfaceParams params;
    params.hardware_info = info;

    hw_ = std::make_unique<zenbedded::ZenbeddedHardware>();
    ASSERT_EQ(hw_->on_init(params), CallbackReturn::SUCCESS);
  }

  static std::unique_ptr<zenoh::Session> test_router_;
  std::unique_ptr<zenbedded::ZenbeddedHardware> hw_;
};

std::unique_ptr<zenoh::Session> TestZenbeddedHardware::test_router_ = nullptr;

TEST_F(TestZenbeddedHardware, missing_schema_path_fails)
{
  hardware_interface::HardwareInfo info;
  info.name = "test_hw";
  info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:21555";
  info.hardware_parameters["state_topic"] = "joint_states";
  info.hardware_parameters["command_topic"] = "joint_commands";
  // Intentionally NOT setting schema_path

  hardware_interface::HardwareComponentInterfaceParams params;
  params.hardware_info = info;

  auto hw = std::make_unique<zenbedded::ZenbeddedHardware>();
  EXPECT_EQ(hw->on_init(params), CallbackReturn::ERROR);
}

TEST_F(TestZenbeddedHardware, missing_zenoh_params_fails)
{
  hardware_interface::HardwareInfo info;
  info.name = "test_hw";
  // Intentionally NOT setting zenoh_endpoint, state_topic, command_topic
  info.hardware_parameters["schema_path"] = std::string(TEST_CONFIG_DIR) + "/interface_schema.yaml";

  hardware_interface::HardwareComponentInterfaceParams params;
  params.hardware_info = info;

  auto hw = std::make_unique<zenbedded::ZenbeddedHardware>();
  EXPECT_EQ(hw->on_init(params), CallbackReturn::ERROR);
}

TEST_F(TestZenbeddedHardware, state_interfaces_exported)
{
  auto si = hw_->export_state_interfaces();
  EXPECT_EQ(si.size(), 2);
  EXPECT_EQ(si[0].get_name(), "motor_arm/position");
  EXPECT_EQ(si[0].get_interface_name(), "position");
  EXPECT_EQ(si[1].get_name(), "pendulum_axis/position");
}

TEST_F(TestZenbeddedHardware, command_interfaces_exported)
{
  auto ci = hw_->export_command_interfaces();
  EXPECT_EQ(ci.size(), 1);
  EXPECT_EQ(ci[0].get_name(), "motor_arm/position");
  EXPECT_EQ(ci[0].get_interface_name(), "position");
}

TEST_F(TestZenbeddedHardware, activate_succeeds_with_router)
{
  rclcpp_lifecycle::State state(1, "unconfigured");
  EXPECT_EQ(hw_->on_activate(state), CallbackReturn::SUCCESS);
  EXPECT_EQ(hw_->on_deactivate(state), CallbackReturn::SUCCESS);
}

TEST_F(TestZenbeddedHardware, activate_fails_with_bad_endpoint)
{
  // Use a port unlikely to be running a Zenoh router
  hardware_interface::HardwareInfo info;
  info.name = "test_hw";
  info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:1";
  info.hardware_parameters["state_topic"] = "joint_states";
  info.hardware_parameters["command_topic"] = "joint_commands";
  info.hardware_parameters["zenoh_mode"] = "client";

  info.hardware_parameters["schema_path"] = std::string(TEST_CONFIG_DIR) + "/interface_schema.yaml";

  hardware_interface::HardwareComponentInterfaceParams params;
  params.hardware_info = info;

  auto hw = std::make_unique<zenbedded::ZenbeddedHardware>();
  ASSERT_EQ(hw->on_init(params), CallbackReturn::SUCCESS);

  rclcpp_lifecycle::State state(1, "unconfigured");
  EXPECT_EQ(hw->on_activate(state), CallbackReturn::ERROR);
}

TEST_F(TestZenbeddedHardware, read_state_updates_hw_states)
{
  rclcpp_lifecycle::State state(1, "unconfigured");
  ASSERT_EQ(hw_->on_activate(state), CallbackReturn::SUCCESS);

  zenbedded_state_t state_data;
  state_data.motor_arm_position = 3.14f;
  state_data.pendulum_axis_position = 2.71f;

  auto buf = std::vector<uint8_t>(
    reinterpret_cast<const uint8_t *>(&state_data),
    reinterpret_cast<const uint8_t *>(&state_data) + sizeof(zenbedded_state_t));

  // Retry loop: publish and read until data arrives or timeout
  bool received = false;
  for (int attempt = 0; attempt < 5; attempt++)
  {
    test_router_->put(zenoh::KeyExpr("joint_states"), buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_EQ(
      hw_->read(rclcpp::Time(0), rclcpp::Duration(0, 0)), hardware_interface::return_type::OK);

    auto si = hw_->export_state_interfaces();
    if (
      si.size() >= 2 &&
      std::abs(static_cast<float>(si[0].get_optional().value_or(0)) - 3.14f) < 0.001f)
    {
      received = true;
      break;
    }
  }

  ASSERT_TRUE(received);

  auto si = hw_->export_state_interfaces();
  EXPECT_FLOAT_EQ(static_cast<float>(si[0].get_optional().value()), 3.14f);
  EXPECT_FLOAT_EQ(static_cast<float>(si[1].get_optional().value()), 2.71f);

  EXPECT_EQ(hw_->on_deactivate(state), CallbackReturn::SUCCESS);
}

TEST_F(TestZenbeddedHardware, write_publishes_command_via_zenoh)
{
  rclcpp_lifecycle::State state(1, "unconfigured");
  ASSERT_EQ(hw_->on_activate(state), CallbackReturn::SUCCESS);

  // Subscribe on command topic via test_router_ to capture published data
  std::vector<uint8_t> received;
  auto sub = test_router_->declare_subscriber(
    zenoh::KeyExpr("joint_commands"),
    [&](zenoh::Sample & sample) { received = sample.get_payload().as_vector(); }, []() {},
    zenoh::Session::SubscriberOptions::create_default(), nullptr);

  auto ci = hw_->export_command_interfaces();
  ASSERT_GE(ci.size(), 1);
  ASSERT_TRUE(ci[0].set_value(1.5));

  // Retry loop: write and check until data arrives or timeout
  bool published = false;
  for (int attempt = 0; attempt < 5; attempt++)
  {
    EXPECT_EQ(
      hw_->write(rclcpp::Time(0), rclcpp::Duration(0, 0)), hardware_interface::return_type::OK);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (received.size() == 4)
    {
      published = true;
      break;
    }
  }

  ASSERT_TRUE(published);
  ASSERT_EQ(received.size(), sizeof(zenbedded_command_t));
  zenbedded_command_t cmd;
  std::memcpy(&cmd, received.data(), sizeof(zenbedded_command_t));
  EXPECT_FLOAT_EQ(cmd.motor_arm_position, 1.5f);

  EXPECT_EQ(hw_->on_deactivate(state), CallbackReturn::SUCCESS);
}

TEST_F(TestZenbeddedHardware, multi_instance_independent_schemas)
{
  auto make_hw = [](
                   const std::string & schema_file, const std::string & state_topic,
                   const std::string & cmd_topic)
  {
    hardware_interface::HardwareInfo info;
    info.name = schema_file;
    info.hardware_parameters["zenoh_endpoint"] = "tcp/127.0.0.1:21555";
    info.hardware_parameters["state_topic"] = state_topic;
    info.hardware_parameters["command_topic"] = cmd_topic;
    info.hardware_parameters["zenoh_mode"] = "client";
    info.hardware_parameters["schema_path"] = std::string(TEST_CONFIG_DIR) + "/" + schema_file;

    hardware_interface::HardwareComponentInterfaceParams params;
    params.hardware_info = info;

    auto hw = std::make_unique<zenbedded::ZenbeddedHardware>();
    EXPECT_EQ(hw->on_init(params), CallbackReturn::SUCCESS);
    return hw;
  };

  auto hw_arm = make_hw("instance_arm.yaml", "arm/state", "arm/cmd");
  ASSERT_TRUE(hw_arm);
  auto hw_gripper = make_hw("instance_gripper.yaml", "gripper/state", "gripper/cmd");
  ASSERT_TRUE(hw_gripper);

  // Verify each instance exports correct interfaces from its own schema
  {
    auto si = hw_arm->export_state_interfaces();
    ASSERT_EQ(si.size(), 3);
    EXPECT_EQ(si[0].get_name(), "shoulder/position");
    EXPECT_EQ(si[1].get_name(), "shoulder/velocity");
    EXPECT_EQ(si[2].get_name(), "elbow/position");

    auto ci = hw_arm->export_command_interfaces();
    ASSERT_EQ(ci.size(), 2);
    EXPECT_EQ(ci[0].get_name(), "shoulder/position");
    EXPECT_EQ(ci[1].get_name(), "elbow/position");
  }
  {
    auto si = hw_gripper->export_state_interfaces();
    ASSERT_EQ(si.size(), 1);
    EXPECT_EQ(si[0].get_name(), "gripper/position");

    auto ci = hw_gripper->export_command_interfaces();
    ASSERT_EQ(ci.size(), 1);
    EXPECT_EQ(ci[0].get_name(), "gripper/position");
  }

  // Activate both — each creates its own Zenoh session, sub, pub
  rclcpp_lifecycle::State state(1, "unconfigured");
  ASSERT_EQ(hw_arm->on_activate(state), CallbackReturn::SUCCESS);
  ASSERT_EQ(hw_gripper->on_activate(state), CallbackReturn::SUCCESS);

  // Build state buffers: arm = 3×float32, gripper = 1×float32
  std::vector<uint8_t> arm_state_buf(12);
  float shoulder_pos = 1.0f, shoulder_vel = 2.0f, elbow_pos = 3.0f;
  std::memcpy(arm_state_buf.data(), &shoulder_pos, 4);
  std::memcpy(arm_state_buf.data() + 4, &shoulder_vel, 4);
  std::memcpy(arm_state_buf.data() + 8, &elbow_pos, 4);

  std::vector<uint8_t> gripper_state_buf(4);
  float gripper_pos = 99.0f;
  std::memcpy(gripper_state_buf.data(), &gripper_pos, 4);

  // Publish on both topics, verify each reads only its own data
  bool state_received = false;
  for (int attempt = 0; attempt < 10; attempt++)
  {
    test_router_->put(zenoh::KeyExpr("arm/state"), arm_state_buf);
    test_router_->put(zenoh::KeyExpr("gripper/state"), gripper_state_buf);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    hw_arm->read(rclcpp::Time(0), rclcpp::Duration(0, 0));
    hw_gripper->read(rclcpp::Time(0), rclcpp::Duration(0, 0));

    auto arm_si = hw_arm->export_state_interfaces();
    auto gripper_si = hw_gripper->export_state_interfaces();
    if (
      arm_si.size() >= 3 && gripper_si.size() >= 1 &&
      std::abs(static_cast<float>(arm_si[0].get_optional().value_or(0)) - 1.0f) < 0.001f &&
      std::abs(static_cast<float>(gripper_si[0].get_optional().value_or(0)) - 99.0f) < 0.001f)
    {
      state_received = true;
      break;
    }
  }
  ASSERT_TRUE(state_received);

  auto arm_si = hw_arm->export_state_interfaces();
  EXPECT_FLOAT_EQ(static_cast<float>(arm_si[0].get_optional().value()), 1.0f);
  EXPECT_FLOAT_EQ(static_cast<float>(arm_si[1].get_optional().value()), 2.0f);
  EXPECT_FLOAT_EQ(static_cast<float>(arm_si[2].get_optional().value()), 3.0f);

  auto gripper_si = hw_gripper->export_state_interfaces();
  EXPECT_FLOAT_EQ(static_cast<float>(gripper_si[0].get_optional().value()), 99.0f);

  // Verify command isolation — subscribe on both topics
  std::vector<uint8_t> arm_cmd_received, gripper_cmd_received;
  auto arm_sub = test_router_->declare_subscriber(
    zenoh::KeyExpr("arm/cmd"),
    [&](zenoh::Sample & sample) { arm_cmd_received = sample.get_payload().as_vector(); }, []() {},
    zenoh::Session::SubscriberOptions::create_default(), nullptr);
  auto gripper_sub = test_router_->declare_subscriber(
    zenoh::KeyExpr("gripper/cmd"),
    [&](zenoh::Sample & sample) { gripper_cmd_received = sample.get_payload().as_vector(); },
    []() {}, zenoh::Session::SubscriberOptions::create_default(), nullptr);

  // Write on arm only — gripper subscriber must stay empty
  auto arm_ci = hw_arm->export_command_interfaces();
  ASSERT_GE(arm_ci.size(), 2);
  ASSERT_TRUE(arm_ci[0].set_value(10.0));
  ASSERT_TRUE(arm_ci[1].set_value(20.0));

  bool cmd_published = false;
  for (int attempt = 0; attempt < 5; attempt++)
  {
    hw_arm->write(rclcpp::Time(0), rclcpp::Duration(0, 0));
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!arm_cmd_received.empty())
    {
      cmd_published = true;
      break;
    }
  }
  ASSERT_TRUE(cmd_published);
  ASSERT_EQ(arm_cmd_received.size(), 8);  // 2 × float32
  EXPECT_TRUE(gripper_cmd_received.empty())
    << "Gripper received arm's command — topic isolation broken";

  EXPECT_EQ(hw_arm->on_deactivate(state), CallbackReturn::SUCCESS);
  EXPECT_EQ(hw_gripper->on_deactivate(state), CallbackReturn::SUCCESS);
}
