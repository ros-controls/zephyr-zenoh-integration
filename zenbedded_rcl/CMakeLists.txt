if(CONFIG_ZENBEDDED_RCL)

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
set(ZENBEDDED_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated_include/zenbedded_rcl/generated)
set(ZENBEDDED_GEN_HEADER ${ZENBEDDED_GEN_DIR}/interface_data.h)
zenbedded_generate_header(
    YAML   ${ZENBEDDED_SCHEMA_FILE_ABS}
    OUTPUT ${ZENBEDDED_GEN_HEADER}
    TARGET generate_interface_data
)


zephyr_library_named(zenbedded_rcl)
zephyr_library_sources(
        src/hardware_component.cpp
        src/zenbedded_client.cpp
)

target_include_directories(zenbedded_rcl PUBLIC include)
target_include_directories(zenbedded_rcl PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/generated_include)
target_link_libraries(zenbedded_rcl PUBLIC zenbedded_transport)

# Ensure the header exists before library compiles.
add_dependencies(zenbedde_rcl ${generate_interface_data})
if(TARGET app)
    add_dependencies(app ${generate_interface_data})
endif()

endif() # CONFIG_ZENBEDDED_RCL
