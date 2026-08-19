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

import json
import time

import zenoh
from twister_harness import DeviceAdapter


def test_zenoh_client_connects(zenoh_router, dut: DeviceAdapter):
    dut.readlines_until(regex=r"Zenoh client connected", timeout=10)


def test_host_message_is_printed(zenoh_router, dut: DeviceAdapter):
    message = "hello from the host"
    config = zenoh.Config()
    config.insert_json5("connect/endpoints", json.dumps(["tcp/127.0.0.1:7447"]))

    session = zenoh.open(config)
    try:
        dut.readlines_until(regex=r"Zenoh client connected", timeout=10)

        publisher = session.declare_publisher("zenbedded/e2e/test")

        # declare_publisher returns before the router has propagated the firmware's
        # subscription, so publishing immediately can drop the sample. MatchingStatus
        # defines no __bool__, so .matching must be read explicitly to get a real answer.
        deadline = time.monotonic() + 10
        while not publisher.matching_status.matching:
            assert time.monotonic() < deadline, "no matching subscriber within 10s"
            time.sleep(0.05)

        publisher.put(message)

        lines = dut.readlines_until(regex=r"Received test message:", timeout=10)
        assert any(f"Received test message: {message}" in line for line in lines)
    finally:
        session.close()
