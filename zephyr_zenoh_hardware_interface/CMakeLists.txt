cmake_minimum_required(VERSION 3.8)
project(zephyr_zenoh_hardware_interface)

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

add_library(zephyr_zenoh_hardware_interface SHARED
  src/zenoh_zephyr_hardware.cpp
  src/interface_schema.cpp
)

target_include_directories(zephyr_zenoh_hardware_interface PRIVATE
  include
)

target_link_libraries(zephyr_zenoh_hardware_interface
  PRIVATE
    hardware_interface::hardware_interface
    pluginlib::pluginlib
    rclcpp::rclcpp
    rclcpp_lifecycle::rclcpp_lifecycle
    zenohcxx::zenohc
    yaml-cpp::yaml-cpp
)

pluginlib_export_plugin_description_file(hardware_interface zenoh_zephyr_hardware_interface.xml)

install(TARGETS zephyr_zenoh_hardware_interface DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_cmake_gmock REQUIRED)

  ament_add_gmock(test_zenoh_zephyr_hardware
    test/test_zenoh_zephyr_hardware.cpp
  )
  target_link_libraries(test_zenoh_zephyr_hardware
    zephyr_zenoh_hardware_interface
    hardware_interface::hardware_interface
    zenohcxx::zenohc
  )
  target_include_directories(test_zenoh_zephyr_hardware PRIVATE
    include
  )
  ament_add_gmock(test_interface_schema
    test/test_interface_schema.cpp
  )
  target_link_libraries(test_interface_schema
    zephyr_zenoh_hardware_interface
    yaml-cpp::yaml-cpp
  )
  target_include_directories(test_interface_schema PRIVATE
    include
  )
endif()

ament_package()
