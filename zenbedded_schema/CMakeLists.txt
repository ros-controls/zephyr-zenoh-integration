cmake_minimum_required(VERSION 3.8)
project(zenbedded_schema)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(yaml-cpp REQUIRED)

add_library(zenbedded_schema
  src/interface_schema.cpp
)
set_property(TARGET zenbedded_schema PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(zenbedded_schema PUBLIC include)
target_link_libraries(zenbedded_schema PUBLIC yaml-cpp::yaml-cpp)

add_executable(generate_header
  tools/generate_header.cpp
)
target_link_libraries(generate_header PRIVATE zenbedded_schema)

install(TARGETS zenbedded_schema generate_header
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/ DESTINATION include)

if(COMMAND ament_package)
  ament_package()
endif()
# Suppress colcon unused-variable warnings
set(_ ${CATKIN_INSTALL_INTO_PREFIX_ROOT})
set(_ ${BUILD_TESTING})
