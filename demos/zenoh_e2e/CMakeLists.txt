cmake_minimum_required(VERSION 3.28.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zenoh_e2e)

zephyr_compile_definitions(ZENOH_DEBUG=3)

target_sources(app PRIVATE src/main.c)
