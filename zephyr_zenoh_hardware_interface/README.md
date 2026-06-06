# Zenoh Hardware Interface (ros2_control)

A `ros2_control` SystemInterface implementation that communicates with remote MCUs via Zenoh.

## Setup

```bash
mkdir -p ~/zephyr_zenoh_ws/src
cd ~/zephyr_zenoh_ws/src
git clone git@github.com:ros-controls/zephyr-zenoh-integration.git
cd ~/zephyr_zenoh_ws
rosdep install --from-paths src --ignore-src -r -y
colcon build
```
