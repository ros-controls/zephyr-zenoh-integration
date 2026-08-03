#!/usr/bin/env python3

# Copyright 2026 Open Source Robotics Foundation, Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Host-side companion for the demos/rcl_cpp_test firmware.

Subscribes to the firmware's state topic, and for every state sample it
receives, publishes a command sample back on the command topic. This closes
the loop so the test node's `cmd.motor_arm_position` field is expected to
change over time instead of sitting at the initial value.

Requires: pip install eclipse-zenoh==1.9.0

Usage:
    python3 zenoh_echo_node.py \
        --state-topic zenbedded/test/state \
        --cmd-topic zenbedded/test/cmd

Without --connect the script relies on multicast scouting to find peers. Pass
--connect to reach a router explicitly, matching whatever the firmware was
built with (-DZENOH_LOCATOR):
    python3 zenoh_echo_node.py --connect tcp/192.0.2.1:7447

Payload layout must match interface_data.h exactly (packed, little-endian,
IEEE-754 floats):
    zenbedded_state_t   = struct.pack("<ff", motor_arm_position, pendulum_axis_position)
    zenbedded_command_t = struct.pack("<f",  motor_arm_position)
"""

import argparse
import json
import struct
import time

import zenoh

STATE_FMT = "<ff"  # motor_arm_position, pendulum_axis_position
CMD_FMT = "<f"  # motor_arm_position


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state-topic", default="zenbedded/test/state")
    parser.add_argument("--cmd-topic", default="zenbedded/test/cmd")
    parser.add_argument(
        "--connect",
        action="append",
        metavar="ENDPOINT",
        help="Zenoh endpoint to connect to, e.g. tcp/192.0.2.1:7447. Repeat "
        "for several. Omit to let Zenoh discover peers over multicast "
        "scouting, which is what you want when there is no router.",
    )
    parser.add_argument(
        "--transform",
        type=float,
        default=-1.0,
        help="Multiplier applied to the received motor_arm_position before "
        "echoing it back as a command. Defaults to -1.0 so the echoed "
        "value is visibly different from the state, confirming on the "
        "firmware side that it came from this script and not some "
        "stale buffer.",
    )
    args = parser.parse_args()

    config = zenoh.Config()
    if args.connect:
        config.insert_json5("connect/endpoints", json.dumps(args.connect))
    session = zenoh.open(config)
    publisher = session.declare_publisher(args.cmd_topic)
    print("Ready")

    count = 0

    def on_state(sample: zenoh.Sample) -> None:
        nonlocal count
        payload = bytes(sample.payload)
        if len(payload) != struct.calcsize(STATE_FMT):
            print(f"[warn] unexpected state payload size {len(payload)}, skipping")
            return

        motor_arm_position, pendulum_axis_position = struct.unpack(STATE_FMT, payload)
        count += 1

        cmd_value = motor_arm_position * args.transform
        publisher.put(struct.pack(CMD_FMT, cmd_value))

        if count % 20 == 0:
            print(
                f"[{count}] state: motor_arm={motor_arm_position:.3f} "
                f"pendulum={pendulum_axis_position:.3f}  -> "
                f"echoing cmd: motor_arm={cmd_value:.3f}"
            )

    subscriber = session.declare_subscriber(args.state_topic, on_state)

    print(
        f"Subscribed to '{args.state_topic}', echoing to '{args.cmd_topic}'. " f"Ctrl+C to stop."
    )
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        subscriber.undeclare()
        publisher.undeclare()
        session.close()
        print(f"Stopped. Received {count} state samples total.")


if __name__ == "__main__":
    main()
