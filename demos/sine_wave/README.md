# Zenbedded Sine Wave Demo

A minimal end-to-end example showing `zenbedded_hardware_interface` and
`zenbedded_rcl` working together: a Zephyr firmware app generates a
sinusoidal signal which is inspected using PlotJuggler on `controller_manager/introspection_data` topics.

This demo is intentionally state-only — no controller drives a command
back down to the firmware. It exists to validate and document the
wiring between the schema, the Zephyr RCL module, and the host-side
hardware interface, not to demonstrate closed-loop control.

Both sides are generated from the same `interface_schema.yaml`

## Prerequisites

- A built and installed `zenbedded_transport` (ament_cmake package)
- A built `zenbedded_hardware_interface`
- A Zephyr workspace with `zenbedded_rcl` set up as a module
- An ESP32-S3 board (or adjust `CONFIG_WIFI_ESP32` / board target for a
  different Zephyr-supported board)
- A reachable Zenoh router, or peer-to-peer connectivity between the
  board and the host

## Building and running (host side)

```bash
cd <ros_ws>
# copy packages or use "ln -s" symlink into the workspace
rosdep update
rosdep install --from-paths src --ignore-src -r -y
colcon build --packages-select zenbedded_hardware_interface zenbedded_transport sine_wave_ros
source install/setup.bash
ros2 launch zenbedded_sine_wave sine_wave.launch.py
```


## Building and flashing (firmware side)

```bash
cd sine_wave_zephyr
west build --board=esp32s3_devkitc/esp32s3/procpu -p always
west flash
```

Set network params in both `sine_wave.urdf` and the
firmware's `client.init()` call to match your actual Zenoh router
address before building.

## Configuration reference

| `<param>` (URDF, host side) | Required | Description                                                                                                      |
|---|---|------------------------------------------------------------------------------------------------------------------|
| `zenoh_endpoint` | yes | Zenoh connect endpoint, e.g. `tcp/192.168.1.50:7447`                                                             |
| `state_topic` | yes | Key expression the firmware publishes state to                                                                   |
| `command_topic` | yes | Key expression for commands (unused, but still required by the plugin)                                           |
| `schema_path` | yes | **Absolute** path to `interface_schema.yaml` — no `$(find ...)` substitution happens in the launch file (with xacro) |
| `zenoh_mode` | no | Defaults to `client`                                                                                             |
