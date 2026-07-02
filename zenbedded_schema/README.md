# zenbedded_schema

YAML-driven schema parser and C header generator. No ROS dependencies. Builds with colcon or standalone CMake.

## Schema YAML

```yaml
state_interfaces:
  motor_arm:
    position: float32
  pendulum_axis:
    position: float32
command_interfaces:
  motor_arm:
    position: float32
```

Supported types: `float32`, `float64` / `double`, `int32`, `uint64`, `int16`, `uint8`.

## CLI

```bash
# colcon
colcon build --packages-select zenbedded_schema
source install/setup.bash
generate_header --yaml interface_schema.yaml --output interface_data.h

# standalone
cmake -S . -B build && cmake --build build
./build/generate_header --yaml interface_schema.yaml --output interface_data.h
```

## CMake

```cmake
find_package(zenbedded_schema REQUIRED)
target_link_libraries(my_lib PUBLIC zenbedded_schema::zenbedded_schema)
```

### Generate headers from CMake

```cmake
find_package(zenbedded_schema REQUIRED)

zenbedded_generate_header(
  YAML  interface_schema.yaml
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/interface_data.h
  TARGET generate_interface_data
)

add_dependencies(my_test ${generate_interface_data})
```

Paths can be absolute or source-relative. The `TARGET` argument is optional; it sets a variable in the caller's scope with the generated custom target name.
