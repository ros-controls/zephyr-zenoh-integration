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

add_library(zephyr_zenoh_hardware_interface SHARED
  src/zenoh_zephyr_hardware.cpp
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
)

pluginlib_export_plugin_description_file(hardware_interface zenoh_zephyr_hardware_interface.xml)

install(TARGETS zephyr_zenoh_hardware_interface DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  # formatting is handled by clang-format via pre-commit, so skip uncrustify
  set(ament_cmake_uncrustify_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
