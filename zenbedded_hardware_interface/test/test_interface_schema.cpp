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
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include "zenbedded_hardware_interface/generated/interface_data.h"
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

TEST_F(TestInterfaceSchema, generated_state_struct_compiles)
{
  EXPECT_GT(sizeof(zenbedded_state_t), 0);
  EXPECT_GT(ZENBEDDED_STATE_BYTE_SIZE, 0);
}

TEST_F(TestInterfaceSchema, generated_command_struct_compiles)
{
  EXPECT_GT(sizeof(zenbedded_command_t), 0);
  EXPECT_GT(ZENBEDDED_COMMAND_BYTE_SIZE, 0);
}

TEST_F(TestInterfaceSchema, generated_struct_sizes_match_config_schema)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml_file(
    std::string(TEST_CONFIG_DIR) + "/interface_schema.yaml");
  ASSERT_TRUE(schema.valid()) << schema.error();
  EXPECT_EQ(sizeof(zenbedded_state_t), ZENBEDDED_STATE_BYTE_SIZE);
  EXPECT_EQ(sizeof(zenbedded_state_t), schema.state_buffer_size());
  EXPECT_EQ(sizeof(zenbedded_command_t), ZENBEDDED_COMMAND_BYTE_SIZE);
  EXPECT_EQ(sizeof(zenbedded_command_t), schema.command_buffer_size());
}

TEST_F(TestInterfaceSchema, write_c_header_produces_valid_output)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(multi_field_yaml);
  ASSERT_TRUE(schema.valid());

  char tmp_path[] = "/tmp/test_header_XXXXXX";
  int fd = mkstemp(tmp_path);
  ASSERT_GE(fd, 0);
  close(fd);

  EXPECT_TRUE(schema.write_c_header(tmp_path));

  std::ifstream file(tmp_path);
  ASSERT_TRUE(file.is_open());
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  file.close();
  std::remove(tmp_path);

  EXPECT_THAT(content, ::testing::HasSubstr("zenbedded_state_t"));
  EXPECT_THAT(content, ::testing::HasSubstr("zenbedded_command_t"));
  EXPECT_THAT(content, ::testing::HasSubstr("ZENBEDDED_STATE_BYTE_SIZE"));
  EXPECT_THAT(content, ::testing::HasSubstr("ZENBEDDED_COMMAND_BYTE_SIZE"));
  EXPECT_THAT(content, ::testing::HasSubstr("left_wheel_position"));
  EXPECT_THAT(content, ::testing::HasSubstr("right_wheel_velocity"));
}

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

TEST_F(TestInterfaceSchema, unknown_type_fails)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(
    "state_interfaces:\n  j:\n    p: float33\n"
    "command_interfaces:\n  j:\n    p: float32\n");
  EXPECT_FALSE(schema.valid());
  EXPECT_THAT(schema.error(), ::testing::HasSubstr("Unknown type"));
}
