cmake_minimum_required(VERSION 3.20.0)
project(zenbedded_transport)

# ==============================================================================
# FACE 1: ZEPHYR / WEST MCU BUILD (Cross-Compiled for Target)
# ==============================================================================
if(ZEPHYR_BASE)
  zephyr_library()
  zephyr_include_directories(include)

  # 1. Base Zenoh transport pipe (Shared by all Tiers)
  zephyr_library_sources(src/zenoh_transport.c)
  zephyr_library_compile_definitions(Z_CONFIG_SOCKET_TIMEOUT=1)

  # 2. Tier 1: Compile ZCDR serialization if enabled
  if(CONFIG_ZENBEDDED_TIER_1)
    zephyr_library_sources(src/serialization.c)
  endif()

  # 3. Tier 2: Automatic "Blackbox" Header Generation
  if(CONFIG_ZENBEDDED_TIER_2)
    set(SCHEMA_FILE "${APPLICATION_SOURCE_DIR}/interface_schema.yaml")
    set(GEN_OUTPUT_DIR "${ZEPHYR_BINARY_DIR}/include/generated")
    set(GEN_HEADER "${GEN_OUTPUT_DIR}/interface_data.h")

    if(EXISTS "${SCHEMA_FILE}")
      # Run Python generator during build
      add_custom_command(
        OUTPUT "${GEN_HEADER}"
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tier2_payload.py"
                --yaml "${SCHEMA_FILE}"
                --output "${GEN_HEADER}"
        DEPENDS "${SCHEMA_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_tier2_payload.py"
        COMMENT "[zenbedded] Generating Tier 2 payload struct from interface_schema.yaml"
      )

      # Ensure header is generated before any Zephyr source compiles
      add_custom_target(zenbedded_tier2_header_gen ALL DEPENDS "${GEN_HEADER}")
      add_dependencies(zephyr_interface zenbedded_tier2_header_gen)

      # Make generated header visible to user code (#include <generated/interface_data.h>)
      zephyr_include_directories("${ZEPHYR_BINARY_DIR}/include")
    else()
      message(WARNING "[zenbedded] CONFIG_ZENBEDDED_TIER_2=y but no interface_schema.yaml found at ${SCHEMA_FILE}")
    endif()
  endif()

  # Stop here when building for Zephyr
  return()
endif()

# ==============================================================================
# FACE 2: ROS 2 / COLCON HOST BUILD (Native PC Toolchain)
# ==============================================================================
find_package(ament_cmake REQUIRED)
find_package(yaml-cpp REQUIRED)

# Compile C++ runtime schema parser for host ROS 2 hardware interface
add_library(zenbedded_transport src/interface_schema.cpp)
target_include_directories(zenbedded_transport PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
set_property(TARGET zenbedded_transport PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zenbedded_transport PUBLIC yaml-cpp::yaml-cpp)

# Install library & headers
install(TARGETS zenbedded_transport
  EXPORT zenbedded_transportTargets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)
install(DIRECTORY include/ DESTINATION include)

# Install Python script for host CLI or tests
install(PROGRAMS tools/generate_tier2_payload.py DESTINATION lib/${PROJECT_NAME})

ament_export_targets(zenbedded_transportTargets)
ament_package(
  CONFIG_EXTRAS cmake/zenbedded_transport-extras.cmake
)
