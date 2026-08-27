cmake_minimum_required(VERSION 3.20.0)

# Force Zephyr to load our transport module so it finds Kconfigs
get_filename_component(WORKSPACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../.. ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${WORKSPACE_DIR}/zenbedded_transport)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zenbedded_test_node)

target_sources(app PRIVATE src/main.cpp)

# NEW: Link the transport library to the application
# target_link_libraries(app PUBLIC zenbedded_transport)
