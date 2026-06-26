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

#include <cstdlib>
#include <iostream>
#include <string>

#include "zenbedded_hardware_interface/interface_schema.hpp"

int main(int argc, char * argv[])
{
  std::string yaml_path;
  std::string output_path;

  for (int i = 1; i < argc; ++i)
  {
    std::string arg = argv[i];
    if (arg == "--yaml" && i + 1 < argc)
    {
      yaml_path = argv[++i];
    }
    else if (arg == "--output" && i + 1 < argc)
    {
      output_path = argv[++i];
    }
  }

  if (yaml_path.empty() || output_path.empty())
  {
    std::cerr << "Usage: generate_header --yaml <schema.yaml> --output <output.h>\n";
    return EXIT_FAILURE;
  }

  auto schema = zenbedded::InterfaceSchema::from_yaml_file(yaml_path);
  if (!schema.valid())
  {
    std::cerr << "Schema error: " << schema.error() << "\n";
    return EXIT_FAILURE;
  }

  if (!schema.write_c_header(output_path))
  {
    std::cerr << "Failed to write header: " << output_path << "\n";
    return EXIT_FAILURE;
  }

  std::cout << "Generated " << output_path << "\n";
  return EXIT_SUCCESS;
}
