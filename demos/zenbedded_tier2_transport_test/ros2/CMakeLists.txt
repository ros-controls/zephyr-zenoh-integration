cmake_minimum_required(VERSION 3.20.0)
project(zenbedded_tier2_test_ros)

find_package(ament_cmake REQUIRED)
find_package(zenbedded_transport REQUIRED)

# We use Kamal's CMake macro to generate the header for the PC tests
zenbedded_generate_header(
  YAML config/interface_schema.yaml
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/include/interface_data.h
  TARGET generate_test_header
)

ament_package()
