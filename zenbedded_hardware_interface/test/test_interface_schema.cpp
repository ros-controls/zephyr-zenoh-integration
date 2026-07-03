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
#include <cstring>
#include <fstream>
#include <string>

#include "zenbedded_hardware_interface/generated/interface_data.h"
#include "zenbedded_schema/interface_schema.hpp"

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

  static constexpr const char * all_types_yaml = R"(
state_interfaces:
  all_types:
    f32: float32
    f64: float64
    i32: int32
    u64: uint64
    i16: int16
    u8: uint8
command_interfaces:
  all_types:
    f32: float32
    f64: float64
    i32: int32
    u64: uint64
    i16: int16
    u8: uint8
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

TEST_F(TestInterfaceSchema, read_state_field_all_types)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(all_types_yaml);
  ASSERT_TRUE(schema.valid());

  std::vector<uint8_t> buf(schema.state_buffer_size(), 0);

  float f32_val = 3.14f;
  double f64_val = 2.718;
  int32_t i32_val = -42;
  uint64_t u64_val = 123456789;
  int16_t i16_val = -1000;
  uint8_t u8_val = 200;

  std::memcpy(buf.data(), &f32_val, 4);
  std::memcpy(buf.data() + 4, &f64_val, 8);
  std::memcpy(buf.data() + 12, &i32_val, 4);
  std::memcpy(buf.data() + 16, &u64_val, 8);
  std::memcpy(buf.data() + 24, &i16_val, 2);
  std::memcpy(buf.data() + 26, &u8_val, 1);

  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 0), static_cast<double>(f32_val));
  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 1), f64_val);
  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 2), static_cast<double>(i32_val));
  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 3), static_cast<double>(u64_val));
  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 4), static_cast<double>(i16_val));
  EXPECT_DOUBLE_EQ(schema.read_state_field(buf.data(), 5), static_cast<double>(u8_val));
}

TEST_F(TestInterfaceSchema, read_state_field_out_of_range_returns_zero)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(simple_yaml);
  ASSERT_TRUE(schema.valid());
  uint8_t dummy = 0;
  EXPECT_DOUBLE_EQ(schema.read_state_field(&dummy, 999), 0.0);
}

TEST_F(TestInterfaceSchema, read_command_field_all_types)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(all_types_yaml);
  ASSERT_TRUE(schema.valid());
  EXPECT_EQ(schema.total_command_interfaces(), 6);

  std::vector<uint8_t> buf(schema.command_buffer_size(), 0);

  float f32_val = 1.5f;
  double f64_val = 2.71828;
  int32_t i32_val = -99;
  uint64_t u64_val = 999888777;
  int16_t i16_val = -5;
  uint8_t u8_val = 12;

  std::memcpy(buf.data(), &f32_val, 4);
  std::memcpy(buf.data() + 4, &f64_val, 8);
  std::memcpy(buf.data() + 12, &i32_val, 4);
  std::memcpy(buf.data() + 16, &u64_val, 8);
  std::memcpy(buf.data() + 24, &i16_val, 2);
  std::memcpy(buf.data() + 26, &u8_val, 1);

  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 0), static_cast<double>(f32_val));
  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 1), f64_val);
  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 2), static_cast<double>(i32_val));
  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 3), static_cast<double>(u64_val));
  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 4), static_cast<double>(i16_val));
  EXPECT_DOUBLE_EQ(schema.read_command_field(buf.data(), 5), static_cast<double>(u8_val));
}

TEST_F(TestInterfaceSchema, write_command_field_all_types)
{
  auto schema = zenbedded::InterfaceSchema::from_yaml(all_types_yaml);
  ASSERT_TRUE(schema.valid());

  std::vector<uint8_t> buf(schema.command_buffer_size(), 0);

  schema.write_command_field(buf.data(), 0, 1.5);
  schema.write_command_field(buf.data(), 1, 2.5);
  schema.write_command_field(buf.data(), 2, -99.0);
  schema.write_command_field(buf.data(), 3, 999.0);
  schema.write_command_field(buf.data(), 4, -5.0);
  schema.write_command_field(buf.data(), 5, 12.0);

  float f32_out;
  std::memcpy(&f32_out, buf.data(), 4);
  EXPECT_FLOAT_EQ(f32_out, 1.5f);
  double f64_out;
  std::memcpy(&f64_out, buf.data() + 4, 8);
  EXPECT_DOUBLE_EQ(f64_out, 2.5);
  int32_t i32_out;
  std::memcpy(&i32_out, buf.data() + 12, 4);
  EXPECT_EQ(i32_out, -99);
  uint64_t u64_out;
  std::memcpy(&u64_out, buf.data() + 16, 8);
  EXPECT_EQ(u64_out, 999);
  int16_t i16_out;
  std::memcpy(&i16_out, buf.data() + 24, 2);
  EXPECT_EQ(i16_out, -5);
  uint8_t u8_out;
  std::memcpy(&u8_out, buf.data() + 26, 1);
  EXPECT_EQ(u8_out, 12);
}
