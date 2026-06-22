cmake_minimum_required(VERSION 3.20.0)

zephyr_library()

# Add our native library source files
zephyr_library_sources(
  src/zenoh_transport.cpp
  src/serialization.cpp
)

# Expose our native headers globally
zephyr_include_directories(include)
