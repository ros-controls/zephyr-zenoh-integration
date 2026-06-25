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
#include <string>

#include "zenbedded_hardware_interface/interface_schema.hpp"

class TestInterfaceSchema : public ::testing::Test
{
protected:
  static constexpr const char * simple_yaml = R"(
state_interfaces:
  motor_arm:
    position: float32
  pendulum_axis:
    position: float32
command_interfaces:
  motor_arm:
    position: float32
)";

  static constexpr const char * multi_field_yaml = R"(
state_interfaces:
  left_wheel:
    position: float32
    velocity: float32
    temperature: float32
  right_wheel:
    position: float32
    velocity: float32
command_interfaces:
  left_wheel:
    velocity: float32
  right_wheel:
    velocity: float32
)";
};

TEST_F(TestInterfaceSchema, simple_schema)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(simple_yaml);
  EXPECT_TRUE(schema.valid());
  EXPECT_TRUE(schema.error().empty());
  EXPECT_EQ(schema.total_state_interfaces(), 2);
  EXPECT_EQ(schema.total_command_interfaces(), 1);
}

TEST_F(TestInterfaceSchema, multi_field_components)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(multi_field_yaml);
  EXPECT_TRUE(schema.valid());
  EXPECT_TRUE(schema.error().empty());
  EXPECT_EQ(schema.total_state_interfaces(), 5);
  EXPECT_EQ(schema.total_command_interfaces(), 2);
}

TEST_F(TestInterfaceSchema, missing_state_interfaces_fails)
{
  auto schema =
    zenbedded::InterfaceSchema::from_yaml("command_interfaces:\n  joint:\n    pos: float32\n");
  EXPECT_FALSE(schema.valid());
  EXPECT_FALSE(schema.error().empty());
  EXPECT_THAT(schema.error(), ::testing::HasSubstr("state_interfaces"));
}

TEST_F(TestInterfaceSchema, missing_command_interfaces_fails)
{
  auto schema =
    zenbedded::InterfaceSchema::from_yaml("state_interfaces:\n  joint:\n    pos: float32\n");
  EXPECT_FALSE(schema.valid());
  EXPECT_FALSE(schema.error().empty());
  EXPECT_THAT(schema.error(), ::testing::HasSubstr("command_interfaces"));
}

TEST_F(TestInterfaceSchema, empty_yaml_fails)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml("");
  EXPECT_FALSE(schema.valid());
  EXPECT_FALSE(schema.error().empty());
}

TEST_F(TestInterfaceSchema, invalid_yaml_fails)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml("{invalid: [yaml");
  EXPECT_FALSE(schema.valid());
  EXPECT_FALSE(schema.error().empty());
}
