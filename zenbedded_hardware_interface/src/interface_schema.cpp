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
#include "zenbedded_hardware_interface/interface_schema.hpp"

#include <yaml-cpp/yaml.h>

#include <fstream>
#include <iterator>

namespace zenbedded
{

FieldType parse_field_type(const std::string & type_str, std::string & error)
{
  if (type_str == "float32")
  {
    return FieldType::FLOAT32;
  }
  if (type_str == "float64" || type_str == "double")
  {
    return FieldType::FLOAT64;
  }
  if (type_str == "int32")
  {
    return FieldType::INT32;
  }
  if (type_str == "uint64")
  {
    return FieldType::UINT64;
  }
  if (type_str == "int16")
  {
    return FieldType::INT16;
  }
  if (type_str == "uint8")
  {
    return FieldType::UINT8;
  }
  error = "Unknown type: " + type_str;
  return static_cast<FieldType>(0);
}

bool parse_component_fields(
  const YAML::Node & component_node, const std::string & component_name,
  std::vector<FlatField> & fields, size_t & buffer_size, std::string & error)
{
  for (const auto & field : component_node)
  {
    const std::string & field_name = field.first.as<std::string>();
    const std::string & type_str = field.second.as<std::string>();
    std::string type_error;
    FieldType type = parse_field_type(type_str, type_error);
    if (!type_error.empty())
    {
      error = "Field '" + field_name + "' in component '" + component_name + "': " + type_error;
      return false;
    }

    size_t byte_size = static_cast<size_t>(type);
    fields.push_back({component_name, field_name, type, buffer_size});
    buffer_size += byte_size;
  }
  return true;
}

InterfaceSchema InterfaceSchema::from_yaml(const std::string & yaml_text)
{
  InterfaceSchema schema;

  try
  {
    YAML::Node root = YAML::Load(yaml_text);

    if (!root["state_interfaces"])
    {
      schema.error_ = "Missing 'state_interfaces' in schema";
      return schema;
    }
    if (!root["command_interfaces"])
    {
      schema.error_ = "Missing 'command_interfaces' in schema";
      return schema;
    }

    // Parse state_interfaces
    for (const auto & component : root["state_interfaces"])
    {
      std::string name = component.first.as<std::string>();

      bool ok = parse_component_fields(
        component.second, name, schema.state_flat_fields_, schema.state_buffer_size_,
        schema.error_);
      if (!ok)
      {
        return schema;
      }
    }

    // Parse command_interfaces
    for (const auto & component : root["command_interfaces"])
    {
      std::string name = component.first.as<std::string>();

      bool ok = parse_component_fields(
        component.second, name, schema.command_flat_fields_, schema.command_buffer_size_,
        schema.error_);
      if (!ok)
      {
        return schema;
      }
    }

    schema.valid_ = true;
  }
  catch (const YAML::Exception & e)
  {
    schema.error_ = "YAML parse error: ";
    schema.error_ += e.what();
  }

  return schema;
}

InterfaceSchema InterfaceSchema::from_yaml_file(const std::string & file_path)
{
  std::ifstream file(file_path);
  if (!file)
  {
    InterfaceSchema schema;
    schema.error_ = "Cannot open file: " + file_path;
    return schema;
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return from_yaml(content);
}

namespace
{

const char * field_type_to_c_type(FieldType type)
{
  if (type == FieldType::FLOAT32)
  {
    return "float";
  }
  if (type == FieldType::FLOAT64)
  {
    return "double";
  }
  if (type == FieldType::INT32)
  {
    return "int32_t";
  }
  if (type == FieldType::UINT64)
  {
    return "uint64_t";
  }
  if (type == FieldType::INT16)
  {
    return "int16_t";
  }
  if (type == FieldType::UINT8)
  {
    return "uint8_t";
  }
  return "unknown";
}

}  // namespace

bool InterfaceSchema::write_c_header(const std::string & output_path) const
{
  if (!valid_)
  {
    return false;
  }

  std::ofstream out(output_path);
  if (!out)
  {
    return false;
  }

  const char * guard = "ZENBEDDED_HARDWARE_INTERFACE__GENERATED__INTERFACE_DATA_H_";

  out << "#ifndef " << guard << "\n";
  out << "#define " << guard << "\n\n";
  out << "#include <stdint.h>\n\n";
  out << "#define ZENBEDDED_STATE_BYTE_SIZE " << state_buffer_size_ << "\n";
  out << "#define ZENBEDDED_COMMAND_BYTE_SIZE " << command_buffer_size_ << "\n\n";
  out << "#pragma pack(push, 1)\n";
  out << "typedef struct\n{\n";

  for (const auto & f : state_flat_fields_)
  {
    out << "  " << field_type_to_c_type(f.type) << " " << f.component << "_" << f.field << ";\n";
  }

  out << "} zenbedded_state_t;\n\n";
  out << "typedef struct\n{\n";

  for (const auto & f : command_flat_fields_)
  {
    out << "  " << field_type_to_c_type(f.type) << " " << f.component << "_" << f.field << ";\n";
  }

  out << "} zenbedded_command_t;\n";
  out << "#pragma pack(pop)\n\n";
  out << "#endif  // " << guard << "\n";

  return true;
}

}  // namespace zenbedded
