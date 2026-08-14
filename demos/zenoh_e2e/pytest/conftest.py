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
import socket
import subprocess
import time

import pytest


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


@pytest.fixture(scope="session")
def zenoh_router(tmp_path_factory):
    log_path = tmp_path_factory.mktemp("zenoh") / "rmw_zenohd.log"

    with log_path.open("w+") as log:
        process = subprocess.Popen(
            ["ros2", "run", "rmw_zenoh_cpp", "rmw_zenohd"],
            stdout=log,
            stderr=subprocess.STDOUT,
            start_new_session=True,
        )

        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline:
                if process.poll() is not None:
                    log.flush()
                    log.seek(0)
                    pytest.fail(f"rmw_zenohd exited with code {process.returncode}:\n{log.read()}")

                with socket.socket() as sock:
                    sock.settimeout(0.2)
                    if sock.connect_ex(("127.0.0.1", 7447)) == 0:
                        yield process
                        return

                time.sleep(0.1)

            pytest.fail("rmw_zenohd did not listen on 127.0.0.1:7447 within 10 seconds")
        finally:
            _stop_process_group(process)
