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
#ifndef ZEPHYR_ZENOH_HARDWARE_INTERFACE__INTERFACE_SCHEMA_HPP_
#define ZEPHYR_ZENOH_HARDWARE_INTERFACE__INTERFACE_SCHEMA_HPP_

#include <cstdint>
#include <string>
#include <vector>

namespace zephyr_zenoh
{

enum class FieldType : uint8_t
{
  FLOAT32 = 4,
  FLOAT64 = 8,
  INT32 = 4,
  UINT64 = 8,
  INT16 = 2,
  UINT8 = 1,
};

struct FieldDescriptor
{
  std::string name;
  FieldType type;
  size_t byte_size;
  size_t byte_offset;
};

struct ComponentDescriptor
{
  std::string name;
  std::vector<FieldDescriptor> state_fields;
  std::vector<FieldDescriptor> command_fields;
  size_t state_size;
  size_t command_size;
};

struct FlatField
{
  std::string component;
  std::string field;
  FieldType type;
  size_t byte_offset;
};

class InterfaceSchema
{
public:
  static InterfaceSchema from_yaml(const std::string & yaml_text);

  bool valid() const { return valid_; }
  std::string error() const { return error_; }

  size_t state_buffer_size() const { return state_buffer_size_; }
  size_t command_buffer_size() const { return command_buffer_size_; }
  size_t total_state_interfaces() const { return state_flat_fields_.size(); }
  size_t total_command_interfaces() const { return command_flat_fields_.size(); }

  const auto & state_fields() const { return state_flat_fields_; }
  const auto & command_fields() const { return command_flat_fields_; }

private:
  InterfaceSchema() = default;

  bool valid_ = false;
  std::string error_;
  size_t state_buffer_size_ = 0;
  size_t command_buffer_size_ = 0;
  std::vector<FlatField> state_flat_fields_;
  std::vector<FlatField> command_flat_fields_;
};

}  // namespace zephyr_zenoh

#endif  // ZEPHYR_ZENOH_HARDWARE_INTERFACE__INTERFACE_SCHEMA_HPP_
