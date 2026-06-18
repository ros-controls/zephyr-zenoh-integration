# Zephyr ROS Transport

The shared bridge logic that ensures consistency between the MCU and the ROS 2 host.

## Contents
- **IDL Definitions**: Shared `.msg` files defining the control interface.
- **Serialization**: Common MicroCDR headers used by both C (Zephyr) and C++ (ROS 2).
- **Type Mapping**: Logic to map ROS 2 interface names to Zenoh resource keys.

## Usage
Changes made here propagate to both the firmware and the hardware interface builds to ensure protocol alignment.
