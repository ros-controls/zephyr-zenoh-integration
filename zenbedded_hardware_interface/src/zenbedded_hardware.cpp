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

#include "zenbedded_hardware_interface/zenbedded_hardware.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "zenbedded_schema/interface_schema.hpp"

namespace zenbedded
{

CallbackReturn ZenbeddedHardware::on_init(
  const hardware_interface::HardwareComponentInterfaceParams & params)
{
  if (hardware_interface::SystemInterface::on_init(params) != CallbackReturn::SUCCESS)
  {
    return CallbackReturn::ERROR;
  }

  try
  {
    zenoh_endpoint_ = info_.hardware_parameters.at("zenoh_endpoint");
    state_topic_ = info_.hardware_parameters.at("state_topic");
    command_topic_ = info_.hardware_parameters.at("command_topic");
    zenoh_mode_ = info_.hardware_parameters.count("zenoh_mode")
                    ? info_.hardware_parameters.at("zenoh_mode")
                    : "client";
  }
  catch (const std::out_of_range & e)
  {
    RCLCPP_FATAL(rclcpp::get_logger("ZenbeddedHardware"), "Missing required URDF parameter");
    return CallbackReturn::ERROR;
  }

  // Load required schema
  std::string schema_path;
  try
  {
    schema_path = info_.hardware_parameters.at("schema_path");
  }
  catch (const std::out_of_range &)
  {
    RCLCPP_FATAL(
      rclcpp::get_logger("ZenbeddedHardware"), "Missing required URDF parameter: schema_path");
    return CallbackReturn::ERROR;
  }

  schema_ = InterfaceSchema::from_yaml_file(schema_path);
  if (!schema_.valid())
  {
    RCLCPP_FATAL(
      rclcpp::get_logger("ZenbeddedHardware"), "Schema error: %s", schema_.error().c_str());
    return CallbackReturn::ERROR;
  }

  hw_states_.resize(schema_.total_state_interfaces(), std::numeric_limits<double>::quiet_NaN());
  hw_commands_.resize(schema_.total_command_interfaces(), std::numeric_limits<double>::quiet_NaN());
  state_buffer_.writeFromNonRT(std::vector<uint8_t>(schema_.state_buffer_size(), 0));
  command_buffer_.resize(schema_.command_buffer_size(), 0);

  RCLCPP_INFO(rclcpp::get_logger("ZenbeddedHardware"), "Hardware Interface Initialized!");
  return CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> ZenbeddedHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (size_t i = 0; i < schema_.state_fields().size(); i++)
  {
    const auto & f = schema_.state_fields()[i];
    state_interfaces.emplace_back(f.component, f.field, &hw_states_[i]);
  }
  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> ZenbeddedHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0; i < schema_.command_fields().size(); i++)
  {
    const auto & f = schema_.command_fields()[i];
    command_interfaces.emplace_back(f.component, f.field, &hw_commands_[i]);
  }
  return command_interfaces;
}

CallbackReturn ZenbeddedHardware::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  try
  {
    auto config = zenoh::Config::create_default();
    config.insert_json5("connect/endpoints", "[\"" + zenoh_endpoint_ + "\"]");
    config.insert_json5("mode", "\"" + zenoh_mode_ + "\"");
    session_ = std::make_unique<zenoh::Session>(std::move(config));

    state_sub_ = std::make_unique<zenoh::Subscriber<void>>(session_->declare_subscriber(
      zenoh::KeyExpr(state_topic_),
      [this](zenoh::Sample & sample)
      {
        const auto & payload = sample.get_payload();
        state_buffer_.writeFromNonRT(payload.as_vector());
      },
      []() {}, zenoh::Session::SubscriberOptions::create_default(), nullptr));

    command_pub_ = std::make_unique<zenoh::Publisher>(session_->declare_publisher(
      zenoh::KeyExpr(command_topic_), zenoh::Session::PublisherOptions::create_default(), nullptr));

    for (size_t i = 0; i < hw_states_.size(); i++)
    {
      if (std::isnan(hw_states_[i]))
      {
        hw_states_[i] = 0;
        hw_commands_[i] = 0;
      }
    }
  }
  catch (const std::exception & e)
  {
    RCLCPP_FATAL(rclcpp::get_logger("ZenbeddedHardware"), "Zenoh failed: %s", e.what());
    return CallbackReturn::ERROR;
  }
  RCLCPP_INFO(rclcpp::get_logger("ZenbeddedHardware"), "Hardware activated!");
  return CallbackReturn::SUCCESS;
}

CallbackReturn ZenbeddedHardware::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  state_sub_.reset();
  command_pub_.reset();
  session_.reset();
  RCLCPP_INFO(rclcpp::get_logger("ZenbeddedHardware"), "Hardware deactivated.");
  return CallbackReturn::SUCCESS;
}

hardware_interface::return_type ZenbeddedHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  auto buf = state_buffer_.readFromRT();
  if (buf)
  {
    for (size_t i = 0; i < schema_.total_state_interfaces(); i++)
    {
      hw_states_[i] = schema_.read_state_field(buf->data(), i);
    }
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type ZenbeddedHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  for (size_t i = 0; i < schema_.total_command_interfaces(); i++)
  {
    schema_.write_command_field(command_buffer_.data(), i, hw_commands_[i]);
  }
  command_pub_->put(command_buffer_);
  return hardware_interface::return_type::OK;
}

}  // namespace zenbedded

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(zenbedded::ZenbeddedHardware, hardware_interface::SystemInterface)
