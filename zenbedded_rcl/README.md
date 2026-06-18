# Zenbedded Firmware

The embedded client library for Zephyr RTOS that provides a high-level API for ROS 2 communication.

## Features
- **Agent-less**: Communicates directly with ROS 2 via `zenoh`.
- **ROS-like API**: Familiar `read` and `write` paradigms for embedded developers.
- **Optimized**: Designed for low-memory footprint and real-time execution within the Zephyr thread model.

## Setup instructions
- Setup west

```bash
west init -l .
west update
```

- build & flash

```bash
cd zephyr_firmware
west build -p always -b esp32s3_devkitc/esp32s3/procpu #your appropriate board
west flash --esp-device /dev/ttyUSB0
```
