cmake_minimum_required(VERSION 3.8)
project(zenbedded_hardware_interface)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(rclcpp REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(zenoh_cpp_vendor REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(zenbedded_hardware_interface SHARED
  src/zenbedded_hardware.cpp
  src/interface_schema.cpp
)

target_include_directories(zenbedded_hardware_interface PRIVATE
  include
)

target_link_libraries(zenbedded_hardware_interface
  PRIVATE
    hardware_interface::hardware_interface
    pluginlib::pluginlib
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
    zenohcxx::zenohc
    yaml-cpp::yaml-cpp
)

# Schema-driven header generator
add_executable(generate_header
  tools/generate_header.cpp
  src/interface_schema.cpp
)
target_link_libraries(generate_header yaml-cpp::yaml-cpp)
target_include_directories(generate_header PRIVATE include)

set(SCHEMA_YAML "${CMAKE_CURRENT_SOURCE_DIR}/config/interface_schema.yaml")
set(GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/zenbedded_hardware_interface/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
  OUTPUT ${GENERATED_DIR}/interface_data.h
  COMMAND generate_header
    --yaml ${SCHEMA_YAML}
    --output ${GENERATED_DIR}/interface_data.h
  DEPENDS generate_header ${SCHEMA_YAML}
  COMMENT "Generating interface_data.h from schema"
)

add_custom_target(generate_interface_data
  DEPENDS ${GENERATED_DIR}/interface_data.h
)

add_dependencies(zenbedded_hardware_interface generate_interface_data)

pluginlib_export_plugin_description_file(hardware_interface zenbedded_hardware_interface.xml)

install(TARGETS zenbedded_hardware_interface DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gmock REQUIRED)

  ament_add_gmock(test_zenbedded_hardware
    test/test_zenbedded_hardware.cpp
  )
  target_link_libraries(test_zenbedded_hardware
    zenbedded_hardware_interface
    hardware_interface::hardware_interface
    zenohcxx::zenohc
  )
  target_include_directories(test_zenbedded_hardware PRIVATE
    include
  )
  ament_add_gmock(test_interface_schema
    test/test_interface_schema.cpp
  )
  target_link_libraries(test_interface_schema
    zenbedded_hardware_interface
    yaml-cpp::yaml-cpp
  )
  target_include_directories(test_interface_schema PRIVATE
    include
  )
  target_compile_definitions(test_interface_schema PRIVATE
    TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
  )
  add_dependencies(test_interface_schema generate_interface_data)
endif()

ament_package()
