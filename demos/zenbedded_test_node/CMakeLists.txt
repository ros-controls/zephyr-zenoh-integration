cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zenbedded_test_node)

target_sources(app PRIVATE
    src/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../../zenbedded_transport/src/serialization.cpp
)

# This was done to avoid pulling in zenoh_transport.cpp which won't build on native sim
target_sources(app PUBLIC
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../../zenbedded_transport/include
    FILES ${CMAKE_CURRENT_LIST_DIR}/../../zenbedded_transport/include/zenbedded_transport/serialization.hpp
)

target_compile_definitions(app PRIVATE
    CONFIG_ZENBEDDED_TRANSPORT_TIER_1=y
)
