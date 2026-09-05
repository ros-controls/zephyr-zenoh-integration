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

import struct
import threading
import zenoh
from twister_harness import DeviceAdapter

TIMEOUT_SEC = 5.0


def test_tier2_ping_pong(zenoh_router, dut: DeviceAdapter):
    """TEST: Tier 2 Raw Zenoh Ping-Pong E2E Communication."""
    # 1. BOOT MCU FIRST: Let it claim the network ports and enter the loop
    dut.readlines_until(regex=r"\[SYS\] Entering Main Event Loop", timeout=TIMEOUT_SEC)

    # 2. Initialize Zenoh Python session pointing to the local test router
    conf = zenoh.Config()
    conf.insert_json5("mode", '"client"')
    conf.insert_json5("connect/endpoints", '["tcp/127.0.0.1:7447"]')

    z = zenoh.open(conf)

    pong_received = threading.Event()
    latest_state = (0.0, 0.0)

    def state_callback(sample):
        nonlocal latest_state
        try:
            payload_bytes = bytes(sample.payload)
            latest_state = struct.unpack("<dd", payload_bytes)
            pong_received.set()
        except struct.error:
            pass

    try:
        pub = z.declare_publisher("test_motor/cmd")
        z.declare_subscriber("test_motor/state", state_callback)

        # Send initial command frame to trigger telemetry feedback
        effort = 0.0
        pong_received.clear()
        pub.put(struct.pack("<d", effort))

        # Assert round-trip response from the MCU
        assert pong_received.wait(
            timeout=TIMEOUT_SEC
        ), "Tier 2 ping-pong timeout: No state response received from MCU"

        pos, vel = latest_state
        assert isinstance(pos, float)
        assert isinstance(vel, float)

    finally:
        z.close()
