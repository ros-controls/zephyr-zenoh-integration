cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rcl_cpp_test)
target_sources(app PRIVATE src/main.cpp)
