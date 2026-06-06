#include "zephyr_zenoh_hardware_interface/zenoh_zephyr_hardware.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include "hardware_interface/types/hardware_interface_type_values.hpp"
#include "rclcpp/rclcpp.hpp"

namespace zephyr_zenoh
{

CallbackReturn ZenohZephyrHardware::on_init(const hardware_interface::HardwareComponentInterfaceParams & params)
{
  if (hardware_interface::SystemInterface::on_init(params) != CallbackReturn::SUCCESS)
  {
    return CallbackReturn::ERROR;
  }

  hw_states_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  hw_commands_.resize(info_.joints.size(), std::numeric_limits<double>::quiet_NaN());
  hw_state_buffer_.resize(info_.joints.size(), 0.0);

  RCLCPP_INFO(rclcpp::get_logger("ZenohZephyrHardware"), "Hardware Interface Initialized!");
  return CallbackReturn::SUCCESS;
}

std::vector<hardware_interface::StateInterface> ZenohZephyrHardware::export_state_interfaces()
{
  std::vector<hardware_interface::StateInterface> state_interfaces;
  for (size_t i = 0; i < info_.joints.size(); i++)
  {
    state_interfaces.emplace_back(hardware_interface::StateInterface(
      info_.joints[i].name, hardware_interface::HW_IF_POSITION, &hw_states_[i]));
  }
  return state_interfaces;
}

std::vector<hardware_interface::CommandInterface> ZenohZephyrHardware::export_command_interfaces()
{
  std::vector<hardware_interface::CommandInterface> command_interfaces;
  for (size_t i = 0; i < info_.joints.size(); i++)
  {
    command_interfaces.emplace_back(hardware_interface::CommandInterface(
      info_.joints[i].name, hardware_interface::HW_IF_POSITION, &hw_commands_[i]));
  }
  return command_interfaces;
}

CallbackReturn ZenohZephyrHardware::on_activate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  for (size_t i = 0; i < hw_states_.size(); i++)
  {
    if (std::isnan(hw_states_[i]))
    {
      hw_states_[i] = 0;
      hw_commands_[i] = 0;
    }
  }
  RCLCPP_INFO(rclcpp::get_logger("ZenohZephyrHardware"), "Hardware activated!");
  return CallbackReturn::SUCCESS;
}

CallbackReturn ZenohZephyrHardware::on_deactivate(const rclcpp_lifecycle::State & /*previous_state*/)
{
  RCLCPP_INFO(rclcpp::get_logger("ZenohZephyrHardware"), "Hardware deactivated.");
  return CallbackReturn::SUCCESS;
}

hardware_interface::return_type ZenohZephyrHardware::read(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  std::lock_guard<std::mutex> lock(data_mutex_);
  for (size_t i = 0; i < hw_states_.size(); i++)
  {
    hw_states_[i] = hw_state_buffer_[i];
  }
  return hardware_interface::return_type::OK;
}

hardware_interface::return_type ZenohZephyrHardware::write(
  const rclcpp::Time & /*time*/, const rclcpp::Duration & /*period*/)
{
  std::lock_guard<std::mutex> lock(data_mutex_);
  for (size_t i = 0; i < hw_commands_.size(); i++)
  {
    hw_state_buffer_[i] = hw_commands_[i];
  }
  return hardware_interface::return_type::OK;
}

}  // namespace zephyr_zenoh

#include "pluginlib/class_list_macros.hpp"

PLUGINLIB_EXPORT_CLASS(
  zephyr_zenoh::ZenohZephyrHardware, hardware_interface::SystemInterface)