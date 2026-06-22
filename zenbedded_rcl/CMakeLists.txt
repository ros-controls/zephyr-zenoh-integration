
### As a standalone application
cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(zenbedded_rcl)

target_sources(app PRIVATE
    src/main.cpp
    src/component_manager.cpp
    src/hardware_component.cpp
)

target_include_directories(app PRIVATE include)




### As a library

# zephyr_library()
# zephyr_library_sources(
#     src/component_manager.cpp
#     src/hardware_component.cpp
# )
# zephyr_library_link_libraries(zenoh-pico)
# zephyr_library_include_directories(include)
