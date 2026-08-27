cmake_minimum_required(VERSION 3.20.0)

zephyr_library()

zephyr_library_sources(
  src/zenoh_transport.c
  src/serialization.c
)

zephyr_include_directories(include)

zephyr_library_compile_definitions(Z_CONFIG_SOCKET_TIMEOUT=1)
