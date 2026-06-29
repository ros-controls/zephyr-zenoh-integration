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
find_package(realtime_tools REQUIRED)

# Import schema library from sibling package
add_subdirectory(../zenbedded_schema ${CMAKE_BINARY_DIR}/zenbedded_schema)

add_library(zenbedded_hardware_interface SHARED
  src/zenbedded_hardware.cpp
)

target_include_directories(zenbedded_hardware_interface PRIVATE
  include
)

target_link_libraries(zenbedded_hardware_interface
  PUBLIC
    zenbedded_schema
    hardware_interface::hardware_interface
    pluginlib::pluginlib
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
    zenohcxx::zenohc
    realtime_tools::realtime_tools
)

# Schema-driven header generator (uses zenbedded_schema's generate_header tool)
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
  target_compile_definitions(test_zenbedded_hardware PRIVATE
    TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/config"
  )
  add_dependencies(test_zenbedded_hardware generate_interface_data)

  ament_add_gmock(test_interface_schema
    test/test_interface_schema.cpp
  )
  target_link_libraries(test_interface_schema
    zenbedded_schema
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
