cmake_minimum_required(VERSION 3.16)
project(zenbedded_hardware_interface)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(hardware_interface REQUIRED)
find_package(rclcpp REQUIRED)
find_package(pluginlib REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(zenoh_cpp_vendor REQUIRED)
find_package(realtime_tools REQUIRED)
find_package(yaml-cpp REQUIRED)

add_library(zenbedded_hardware_interface SHARED
  src/zenbedded_hardware.cpp
  src/interface_schema.cpp
)

target_include_directories(zenbedded_hardware_interface PRIVATE
  include
)

target_link_libraries(zenbedded_hardware_interface
  PUBLIC
  yaml-cpp::yaml-cpp
    hardware_interface::hardware_interface
    pluginlib::pluginlib
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
    zenohcxx::zenohc
    realtime_tools::realtime_tools
)

pluginlib_export_plugin_description_file(hardware_interface zenbedded_hardware_interface.xml)

install(TARGETS zenbedded_hardware_interface DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gmock REQUIRED)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)

  set(SCHEMA_YAML "${CMAKE_CURRENT_SOURCE_DIR}/test/config/interface_schema.yaml")
  set(GEN_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include/zenbedded_hardware_interface/generated")
  get_filename_component(WORKSPACE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
  set(PYTHON_GEN "${WORKSPACE_DIR}/zenbedded_transport/tools/generate_tier2_payload.py")

  file(MAKE_DIRECTORY ${GEN_INCLUDE_DIR})

  add_custom_command(
    OUTPUT ${GEN_INCLUDE_DIR}/interface_data.h
    COMMAND Python3::Interpreter ${PYTHON_GEN} --yaml ${SCHEMA_YAML} --output ${GEN_INCLUDE_DIR}/interface_data.h
    DEPENDS ${SCHEMA_YAML} ${PYTHON_GEN}
    COMMENT "Generating test header natively via Python"
  )
  add_custom_target(generate_interface_data DEPENDS ${GEN_INCLUDE_DIR}/interface_data.h)

  ament_add_gmock(test_zenbedded_hardware test/test_zenbedded_hardware.cpp)
  target_link_libraries(test_zenbedded_hardware
    zenbedded_hardware_interface
    hardware_interface::hardware_interface
    zenohcxx::zenohc
  )

  ament_add_gmock(test_interface_schema test/test_interface_schema.cpp)
    target_link_libraries(test_interface_schema zenbedded_hardware_interface)
    target_include_directories(test_interface_schema PRIVATE
      include
      ${CMAKE_CURRENT_BINARY_DIR}/include
    )
    target_compile_definitions(test_interface_schema PRIVATE TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/config")
    add_dependencies(test_interface_schema generate_interface_data)

  target_include_directories(test_zenbedded_hardware PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}/include
  )
  target_compile_definitions(test_zenbedded_hardware PRIVATE TEST_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/config")
  add_dependencies(test_zenbedded_hardware generate_interface_data)
endif()

ament_package()
