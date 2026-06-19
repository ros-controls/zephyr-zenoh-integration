For an interface you can specify the value_type. if absent its an 8bytes double.
possible values ar bool, short, ushort, int, uint, long, ulong, float and double

```xml

  <ros2_control name="robot_system" type="system">

    <!-- Position-controlled revolute joint (full state feedback) -->
    <joint name="shoulder_pan_joint">
      <command_interface name="position"/>
      <state_interface   name="position"/>
      <state_interface   name="velocity"/>
      <state_interface   name="effort"/>
    </joint>

    <!-- Velocity-controlled joint (e.g. conveyor / wheel) -->
    <joint name="elbow_joint">
      <command_interface name="velocity"/>
      <state_interface   name="velocity"/>
      <state_interface   name="effort"/>
    </joint>

    <!-- Position + effort (torque-controlled wrist) -->
    <joint name="wrist_joint">
      <command_interface name="position"/>
      <command_interface name="effort"/>
      <state_interface   name="position"/>
      <state_interface   name="velocity"/>
      <state_interface   name="effort"/>
    </joint>

    <!-- IMU mounted on the base -->
    <sensor name="base_imu">
      <state_interface name="linear_acceleration.x"/>
      <state_interface name="linear_acceleration.y"/>
      <state_interface name="linear_acceleration.z"/>
      <state_interface name="angular_velocity.x"/>
      <state_interface name="angular_velocity.y"/>
      <state_interface name="angular_velocity.z"/>
    </sensor>

    <!-- Tool GPIO: vacuum on/off command, pressure + error status readback -->
    <gpio name="tool_gpio">
      <command_interface name="gpio/vacuum_enable" value_type="bool"/>
      <state_interface   name="gpio/vacuum_pressure" value_type="float"/>
      <state_interface   name="gpio/error_status" value_type="ushort"/>
    </gpio>

  </ros2_control>
```
