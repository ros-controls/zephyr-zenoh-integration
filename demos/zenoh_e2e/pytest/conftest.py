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

import os
import signal
import subprocess
import time

import pytest

ROUTER_READY_LINE = "Started Zenoh router with id"
ROUTER_READY_TIMEOUT = 30


def _stop_process_group(process):
    if process.poll() is not None:
        return

    for sig, timeout in ((signal.SIGINT, 5), (signal.SIGTERM, 2)):
        try:
            os.killpg(process.pid, sig)
            process.wait(timeout=timeout)
            return
        except ProcessLookupError:
            return
        except subprocess.TimeoutExpired:
            pass

    os.killpg(process.pid, signal.SIGKILL)
    process.wait()


def _wait_until_router_ready(process, log_path):
    deadline = time.monotonic() + ROUTER_READY_TIMEOUT

    while time.monotonic() < deadline:
        if process.poll() is not None:
            pytest.fail(
                f"rmw_zenohd exited with code {process.returncode}:\n{log_path.read_text()}"
            )

        if ROUTER_READY_LINE in log_path.read_text():
            return

        time.sleep(0.1)

    pytest.fail(
        f"rmw_zenohd did not report {ROUTER_READY_LINE!r} within "
        f"{ROUTER_READY_TIMEOUT} seconds:\n{log_path.read_text()}"
    )


@pytest.fixture(scope="session")
def zenoh_router(tmp_path_factory):
    log_path = tmp_path_factory.mktemp("zenoh") / "rmw_zenohd.log"

    with log_path.open("w") as log:
        process = subprocess.Popen(
            ["ros2", "run", "rmw_zenoh_cpp", "rmw_zenohd"],
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )

        try:
            _wait_until_router_ready(process, log_path)
            yield process
        finally:
            _stop_process_group(process)
