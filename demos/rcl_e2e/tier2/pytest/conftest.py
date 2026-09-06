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
import os
import signal
import subprocess
import time

import pytest
import zenoh


def _router_is_ready(endpoint="tcp/127.0.0.1:7447"):
    conf = zenoh.Config()
    conf.insert_json5("mode", '"client"')
    conf.insert_json5("connect/endpoints", json.dumps([endpoint]))
    try:
        session = zenoh.open(conf)
    except Exception:
        return False
    try:
        return len(session.info.routers_zid()) > 0
    finally:
        session.close()


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

                if _router_is_ready():
                    print("\n[CI] Zenoh router handshake confirmed ready.")
                    time.sleep(2)
                    yield process
                    return

                time.sleep(0.1)

            log.flush()
            log.seek(0)
            pytest.fail(f"rmw_zenohd did not become ready within 10 seconds:\n{log.read()}")
        finally:
            _stop_process_group(process)
