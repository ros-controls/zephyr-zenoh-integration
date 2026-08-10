cmake_minimum_required(VERSION 3.20.0)
#set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
#set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
#set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES 1)
#set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 1)
#set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_rcl)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(rcl_cpp_test)
target_sources(app PRIVATE src/main.cpp)

target_include_directories(app PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_rcl/include
)

add_custom_target(lib_dev_sync SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_rcl/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_rcl/include
)
