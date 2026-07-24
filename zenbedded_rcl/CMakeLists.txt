
zephyr_library()

zephyr_library_sources(
        src/component_manager.cpp
        src/hardware_component.cpp
        src/zenbedded_client.cpp
)
zephyr_library_include_directories(include)
