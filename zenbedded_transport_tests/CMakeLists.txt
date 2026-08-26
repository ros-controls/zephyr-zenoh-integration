cmake_minimum_required(VERSION 3.8)
project(zenbedded_transport_tests)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(sensor_msgs REQUIRED)

  # The transport module is a Zephyr module (COLCON_IGNOREd), but its CDR
  # serialization is portable C++ — compile it here against the real ROS 2
  # serializer to prove wire-format compatibility.
  add_library(zcdr_under_test STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../zenbedded_transport/src/serialization.cpp
  )
  target_include_directories(zcdr_under_test PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../zenbedded_transport/include
  )
  target_compile_definitions(zcdr_under_test PUBLIC CONFIG_ZENBEDDED_TRANSPORT_TIER_1=1)

  ament_add_gtest(test_cdr_conformance
    test/test_cdr_conformance.cpp
  )
  target_link_libraries(test_cdr_conformance
    zcdr_under_test
    rclcpp::rclcpp
    ${sensor_msgs_TARGETS}
  )
endif()

ament_package()
