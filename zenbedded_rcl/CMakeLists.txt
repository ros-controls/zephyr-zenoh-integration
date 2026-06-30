cmake_minimum_required(VERSION 3.20.0)

zephyr_library()
zephyr_library_sources(
    src/component_manager.cpp
    src/hardware_component.cpp
)
zephyr_include_directories(include)
