if(CONFIG_ZENBEDDED_RCL)

zephyr_library()
zephyr_library_sources(
        src/hardware_component.cpp
        src/zenbedded_client.cpp
)
zephyr_library_include_directories(include)

endif() #CONFIG_ZENBEDDED_RCL
