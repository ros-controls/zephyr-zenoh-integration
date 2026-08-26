cmake_minimum_required(VERSION 3.20.0)

zephyr_library_named(zenbedded_transport)

zephyr_library_sources(
  src/zenoh_transport.cpp
  src/serialization.cpp
)

target_include_directories(zenbedded_transport PUBLIC include)
