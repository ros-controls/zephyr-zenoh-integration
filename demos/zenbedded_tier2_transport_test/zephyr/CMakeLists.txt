cmake_minimum_required(VERSION 3.20.0)

# 1. Load the Transport Module
get_filename_component(WORKSPACE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../.. ABSOLUTE)
list(APPEND ZEPHYR_EXTRA_MODULES ${WORKSPACE_DIR}/zenbedded_transport)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zenbedded_tier2_test)

# 2. Run the Native Python Generator (The Clean Fix)
set(SCHEMA_FILE "${CMAKE_CURRENT_SOURCE_DIR}/config/interface_schema.yaml")
set(GEN_HEADER "${CMAKE_CURRENT_BINARY_DIR}/interface_data.h")
set(GEN_SCRIPT "${WORKSPACE_DIR}/zenbedded_transport/tools/generate_tier2_payload.py")

add_custom_command(
  OUTPUT ${GEN_HEADER}
  COMMAND python3 ${GEN_SCRIPT} --yaml ${SCHEMA_FILE} --output ${GEN_HEADER}
  DEPENDS ${SCHEMA_FILE} ${GEN_SCRIPT}
  COMMENT "Generating Tier 2 C-Header natively via Python"
)
add_custom_target(generate_tier2_header ALL DEPENDS ${GEN_HEADER})

# 3. Build the Application
zephyr_compile_definitions(Z_CONFIG_SOCKET_TIMEOUT=1)
zephyr_include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_sources(app PRIVATE src/main.cpp)
add_dependencies(app generate_tier2_header)
