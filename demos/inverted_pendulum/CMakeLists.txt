cmake_minimum_required(VERSION 3.20.0)

# Force Zephyr to load BOTH of our custom libraries
set(ZEPHYR_EXTRA_MODULES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../../zenbedded_rcl
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(pendulum_demo)

target_sources(app PRIVATE src/main.c)
