cmake_minimum_required(VERSION 3.8)
project(zenbedded_schema)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake QUIET)
find_package(yaml-cpp REQUIRED)

add_library(zenbedded_schema
  src/interface_schema.cpp
)
target_include_directories(zenbedded_schema
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_property(TARGET zenbedded_schema PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zenbedded_schema PUBLIC yaml-cpp::yaml-cpp)

add_executable(generate_header
  tools/generate_header.cpp
)
target_link_libraries(generate_header PRIVATE zenbedded_schema)

install(TARGETS zenbedded_schema
  EXPORT zenbedded_schemaTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(TARGETS generate_header
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

if(ament_cmake_FOUND)
  ament_export_targets(zenbedded_schemaTargets)
  ament_package(
    CONFIG_EXTRAS cmake/zenbedded_schema-extras.cmake
  )
endif()
# Suppress colcon unused-variable warning
set(___ ${CATKIN_INSTALL_INTO_PREFIX_ROOT})
