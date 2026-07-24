# zenbedded_rcl test node

A standalone firmware application that links against `zenbedded_rcl` and
drives `ZenbeddedClient` through its full public API — `init()`, `state()`,
`sync()`, `command()`, `destroy()` — the same way any real application would.
Output goes to the serial console; look for lines prefixed
`zenbedded_test_node:`.

This is **not** a ztest/host unit test. It's a small firmware "node" you
flash and watch, plus an optional host-side Zenoh peer for a true
end-to-end check.


So **this test node now owns WiFi bring-up** (`connect_wifi_blocking()` in
`src/main.cpp`) and calls `client.init()` only once it has an IP address.
If your application does WiFi bring-up somewhere else already (e.g. via
Zephyr's connection manager / `net_config` auto-init), you can drop that
helper and just call `client.init()` directly.



## Building & flashing

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu zenbedded_rcl/tests/test_node
west flash --esp-device /dev/ttyUSB0
```

Set real WiFi credentials at build time instead of committing them:

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu zenbedded_rcl/tests/test_node \
    -- -DWIFI_SSID=\"YourSSID\" \
       -DWIFI_PSK=\"YourPassword\"
```

To point at a Zenoh router instead of relying on multicast scouting (most
WiFi APs block multicast, so this is usually what you want on real
hardware):

```bash
west build -p always -b esp32s3_devkitc/esp32s3/procpu zenbedded_rcl/tests/test_node \
    -- -DZENOH_MODE=\"client\" -DZENOH_LOCATOR=\"tcp/192.0.2.1:7447\"
```

WiFi connect in this test node
is bounded by `kWifiConnectTimeoutMs` (15s) rather than blocking forever —
it will report a `FAIL` and exit cleanly on bad credentials.


## What each console check verifies

| Check | What it confirms |
|---|---|
| `WiFi connected and got an IP` | Test node's own network bring-up works |
| `client.init() returned 0` | Init doesn't fail (bad topics, Zenoh session open failure, etc.) |
| `client.is_initialized() is true` | The Zenoh session/pub/sub actually came up |
| `sync() called for every iteration` | The state/command double-buffer plumbing doesn't crash or deadlock across repeated calls |
| non-zero command received | Commands are actually arriving from a peer (requires the echo script below) |
| `is_control_thread_running` | The background publish thread is up |
| `destroy() left client uninitialized` | Clean teardown, safe to re-`init()` |

## Full end-to-end loopback

Run the companion peer on a machine on the same network/Zenoh session:

```bash
pip install eclipse-zenoh
python3 tools/zenoh_echo_node.py \
    --state-topic zenbedded/test/state \
    --cmd-topic zenbedded/test/cmd
```

It subscribes to the firmware's state topic and echoes a transformed value
back on the command topic. On the firmware side you should then see
`cmd.motor_arm=` values tracking `state.motor_arm=` instead of sitting at
`0.00`, confirming publish → Zenoh → subscribe actually works end to end.

If you're running in `"client"` mode against a router, start `zenohd`
first; the echo script and the firmware both need to reach it.
