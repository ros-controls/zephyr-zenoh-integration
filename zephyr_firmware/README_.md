# hello_zenoh – Zephyr Hello World + zenoh-pico Publisher

Prints **Hello, World!** over the serial console and publishes a sequenced
`Hello, World! [seq=N]` message to a Zenoh key expression at a configurable
rate.  Targets both **native_sim** (Linux/macOS host simulation) and
**ESP32-S3**.

---

## Repository layout

```
.
├── CMakeLists.txt
├── Kconfig                          # app-level Kconfig symbols
├── prj.conf                         # common Kconfig defaults
├── west.yml                         # workspace manifest (Zephyr + zenoh-pico)
├── src/
│   └── main.c                       # application
└── boards/
    ├── native_sim.conf              # native_sim extra Kconfig
    ├── native_sim.overlay           # native_sim DTS overlay
    ├── esp32s3_devkitm_esp32s3_procpu.conf     # ESP32-S3 extra Kconfig
    └── esp32s3_devkitm_esp32s3_procpu.overlay  # ESP32-S3 DTS overlay
```

---

## Prerequisites

| Tool | Version |
|------|---------|
| Python | ≥ 3.10 |
| west | ≥ 1.2 |
| CMake | ≥ 3.20 |
| Ninja | any recent |
| Zephyr SDK | ≥ 0.16 (includes ESP32 toolchain) |
| ESP-IDF **Zephyr HAL** | pulled automatically by `west update` |

Install west:
```bash
pip install west
```

Install the Zephyr SDK (Linux example):
```bash
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.8/zephyr-sdk-0.16.8_linux-x86_64.tar.xz
tar xf zephyr-sdk-0.16.8_linux-x86_64.tar.xz
cd zephyr-sdk-0.16.8 && ./setup.sh
```

---

## Workspace initialisation

```bash
# 1. Create a workspace directory
mkdir zenoh_ws && cd zenoh_ws

# 2. Initialise west from the app manifest
west init -l app          # 'app' is the folder containing this project
west update               # clones zephyr, zenoh-pico, HALs, …

# 3. Export CMake packages
west zephyr-export

# 4. Install Python deps
pip install -r zephyr/scripts/requirements.txt
```

---

## Building

### native_sim (host simulation)

```bash
west build -b native_sim app \
  -- -DCONF_FILE="prj.conf;boards/native_sim.conf"
```

Run it (creates a virtual TAP interface; requires root or `CAP_NET_ADMIN`):
```bash
sudo west build -t run
# or directly:
sudo ./build/zephyr/zephyr.exe
```

> **Tip:** Start a local Zenoh router first so the publisher can connect:
> ```bash
> docker run --net=host eclipse/zenoh:latest
> ```
> Then subscribe:
> ```bash
> z_sub -k "hello/world"   # from zenoh-pico examples
> ```

---

### ESP32-S3 DevKitM

```bash
west build -b esp32s3_devkitm/esp32s3/procpu app \
  -- -DCONF_FILE="prj.conf;boards/esp32s3_devkitm_esp32s3_procpu.conf"
```

Flash:
```bash
west flash --esp-device /dev/ttyUSB0
```

Monitor serial output:
```bash
west espressif monitor
# or
screen /dev/ttyUSB0 115200
```

---

## Configuration

All symbols can be overridden on the command line with `-D<KEY>=<VALUE>` or
via `menuconfig`:

```bash
west build -t menuconfig
```

| Symbol | Default | Description |
|--------|---------|-------------|
| `CONFIG_ZENOH_LOCATOR` | `udp/192.168.1.100:7447` | Zenoh router locator |
| `CONFIG_ZENOH_KEY_EXPR` | `hello/world` | Key expression to publish on |
| `CONFIG_ZENOH_PUBLISH_PERIOD_MS` | `1000` | Publish interval (ms) |
| `CONFIG_ESP32_WIFI_SSID` | `YOUR_SSID` | Wi-Fi SSID (ESP32-S3 only) |
| `CONFIG_ESP32_WIFI_PASSWORD` | `YOUR_PASSWORD` | Wi-Fi password (ESP32-S3 only) |

Example – publish every 500 ms to a different key:
```bash
west build -b native_sim app -- \
  -DCONFIG_ZENOH_KEY_EXPR=\"sensors/node0\" \
  -DCONFIG_ZENOH_PUBLISH_PERIOD_MS=500
```

---

## Expected serial output

```
=============================
  Hello, World! (Zephyr)
  Board : native_sim
  Zenoh key: hello/world
=============================

[zenoh] Opening session ...
[zenoh] Session opened.
[zenoh] Publisher declared on 'hello/world'.
[zenoh] Publishing: Hello, World! [seq=0]
[zenoh] Publishing: Hello, World! [seq=1]
...
```

---

## Zenoh subscriber (quick test)

Using the zenoh-pico C example subscriber:
```bash
z_sub -k "hello/world" -l "udp/127.0.0.1:7447"
```

Or with the Python SDK:
```python
import zenoh, time
session = zenoh.open(zenoh.Config())
sub = session.declare_subscriber("hello/world", lambda s: print(s.payload.deserialize(str)))
time.sleep(60)
```

---

## License

Apache-2.0 – same as Zephyr and zenoh-pico.
