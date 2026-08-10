

if(NOT DEFINED ZENBEDDED_SCHEMA_FILE)
    set(ZENBEDDED_SCHEMA_FILE interface_schema.yaml)
endif()


# Resolve relative paths against the zephyr application dir.
get_filename_component(ZENBEDDED_SCHEMA_FILE_ABS
        ${ZENBEDDED_SCHEMA_FILE}
        ABSOLUTE
        BASE_DIR ${APPLICATION_SOURCE_DIR}
)

if(NOT EXISTS ${ZENBEDDED_SCHEMA_FILE_ABS})
    message(FATAL_ERROR
            "ZENBEDDED_SCHEMA_FILE does not exist: ${ZENBEDDED_SCHEMA_FILE_ABS}")
endif()

find_package(zenbedded_schema REQUIRED NO_CMAKE_FIND_ROOT_PATH)


# Generate the header at build time
set(ZENBEDDED_GEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/zenbedded_rcl/generated)
set(ZENBEDDED_GEN_HEADER ${ZENBEDDED_GEN_DIR}/interface_data.h)
zenbedded_generate_header(
    YAML   ${ZENBEDDED_SCHEMA_FILE_ABS}
    OUTPUT ${ZENBEDDED_GEN_HEADER}
    TARGET generate_interface_data
)


# ==========================================
# Zenbedded RCL Lib
# ==========================================
zephyr_library_named(zenbedded_rcl)
zephyr_library_sources(
        src/hardware_component.cpp
        src/zenbedded_client.cpp
)
zephyr_library_include_directories(include)
zephyr_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)


# Ensure the header exists before library compiles.
add_dependencies(zenbedded_rcl ${generate_interface_data})
