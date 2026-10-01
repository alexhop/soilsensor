# Soil Sensor

A battery and solar powered soil moisture node you can build yourself: a Seeed
XIAO ESP32-C6, a Catnip Electronics I2C soil probe, a LiPo cell and a small
solar panel. Each node wakes up, reads the probe, posts the readings to a
server on your local network over HTTP, checks for a firmware update, and goes
back to deep sleep.

- **Build the hardware:** [docs/assembly-guide.pdf](docs/assembly-guide.pdf)
  (source: [docs/assembly-guide.md](docs/assembly-guide.md))
- **Firmware:** PlatformIO + Arduino framework, version 0.10.0 (build 19)

Status: Catnip readings were validated on a USB-powered bench rig. The MOSFET
power switching used by the battery build follows the guide's wiring and
compiles, but has not been field-tested on that hardware yet. See
[TODO.md](TODO.md).

## Hardware

Follow the assembly guide. This is the pin map it produces, and the one the
firmware expects by default:

| XIAO pin | Connects to |
|----------|-------------|
| D0 | Battery divider midpoint (1 MOhm to BAT+, 1 MOhm to GND) |
| D1 | 2N7000 gate, with 100k pull-down to GND |
| D4 | Catnip SDA |
| D5 | Catnip SCL |
| 3V3 | Catnip VCC |
| GND | 2N7000 source, divider low side |
| BAT+ / BAT- (pads on the back) | CN3065 BAT+ / BAT-, battery |

The probe's ground returns through the MOSFET (Catnip GND to 2N7000 drain), so
with the gate low the probe draws nothing during deep sleep.

## Quick Start

```bash
pip install platformio

# Site-specific settings (WiFi credentials, server address). Gitignored.
cp src/secrets.example.h src/secrets.h
$EDITOR src/secrets.h

# Optional: verify wiring first. Expect the Catnip at 0x20.
pio run -e scanner -t upload
pio device monitor

# Build and flash the firmware
pio run -t upload
pio device monitor

# Host-side unit tests (no hardware needed)
pio test -e native
```

| Env | Purpose |
|-----|---------|
| `soil-sensor` (default) | The firmware for the assembly guide hardware |
| `scanner` | I2C bus scanner for checking wiring |
| `native` | Host-side unit tests |

The ESP32 environments use the
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform
because the stock `espressif32` platform does not support the Arduino framework
on the ESP32-C6. `python3` must be on the PATH; `get_network_flags.py` and
`fix_wifi_deps.py` work around an undeclared `WiFi` to `Network` library
dependency in Arduino-ESP32 3.x.

If the board will not flash, hold BOOT while plugging in USB-C, release, then
upload. A sleeping battery-powered node only enumerates on USB briefly after
each wake; `./flash-on-wake.sh` (macOS) waits for the port and flashes the
already-built firmware the moment it appears.

## Configuration

Site-specific values live in `src/secrets.h` (copied from
`src/secrets.example.h`, never committed):

| Setting | Description |
|---------|-------------|
| `WIFI_SSID` / `WIFI_PASSWORD` | WiFi network to join |
| `SERVER_HOST` / `SERVER_PORT` | Address of the receiving server |

Everything else is in `src/config.h`. The settings below are wrapped in
`#ifndef`, so they can be overridden with `build_flags` in `platformio.ini`.
Give every node its own `DEVICE_ID`.

| Setting | Default | Description |
|---------|---------|-------------|
| `DEVICE_ID` | `"soil-1"` | Unique node ID, prefix for all sensor IDs |
| `DEVICE_HOSTNAME` | `"SoilSensor-001"` | DHCP hostname |
| `DEVICE_LOCATION` | `"Garden"` | Human-readable location sent with each reading |
| `SENSOR_POWER_PIN` | `D1` | MOSFET gate for sensor power |
| `FIXED_SLEEP_MIN` | `0` | Fixed sleep interval in minutes; 0 uses the adaptive tiers |
| `FORCE_USB_MODE` | `false` | Never deep sleep, loop every 30 s |
| `BATTERY_R1` / `BATTERY_R2` | `220000.0f` | Battery divider resistors; only the ratio matters, so 1 MOhm pairs need no change |
| `BATTERY_FULL_V` / `BATTERY_EMPTY_V` | `4.2f` / `3.3f` | Battery voltage range |
| `NOISE_SAMPLES` | `5` | Soil reads per cycle; the median is reported |
| `VERBOSE_LOG` | `true` | Print payloads and ADC details on serial |

## How It Works

Each wake cycle:

1. Switch the probe on through the MOSFET and wait for it to boot.
2. Take five readings and keep the median, then read the battery voltage.
3. Switch the probe off.
4. Join WiFi (three attempts, 15 s each).
5. POST a heartbeat, then the batch of readings.
6. Ask the server whether a newer firmware build exists; if so, download and
   flash it, then reboot.
7. Sleep.

Sleep depends on the power source. If a USB host is detected the node stays
awake and repeats the cycle every 30 seconds, which keeps the serial console
and flashing usable during development. On battery it deep sleeps:

| Battery | Sleep |
|---------|-------|
| 90% or more | 5 min |
| 60% or more | 15 min |
| 30% or more | 30 min |
| below 30% | 60 min |

## Server API

The firmware talks plain HTTP to `SERVER_HOST:SERVER_PORT`. No server is
included in this repository; anything that implements these three endpoints
will work.

### `POST /api/network/heartbeat`

Sent every cycle, even when the probe was not found. The firmware expects
`200`.

```json
{
  "deviceId": "soil-1",
  "firmwareVersion": "0.10.0",
  "buildNumber": 19,
  "location": "Garden",
  "ip": "192.168.1.42",
  "rssi": -54,
  "uptimeSeconds": 9,
  "sensorsOnline": 1,
  "batteryVoltage": 3.95,
  "batteryPercent": 75
}
```

The battery fields are omitted when no battery reading is available.

### `POST /api/sensors/batch`

A JSON array with one object per reading. The firmware expects `201`. The
`battery` field (percent) is omitted when no battery reading is available.

```json
[
  {"sensorId": "soil-1-moisture", "type": "soil_moisture", "value": 412, "unit": "chirp", "location": "Garden", "battery": 75},
  {"sensorId": "soil-1-soil-temp", "type": "temperature", "value": 64.9, "unit": "°F", "location": "Garden", "battery": 75}
]
```

One physical node produces several logical sensors:

| Sensor ID suffix | `type` | `unit` | Meaning |
|------------------|--------|--------|---------|
| `-moisture` | `soil_moisture` | `chirp` | Median raw capacitance reading |
| `-moisture-raw` | `adc_raw` | `mV` | Same median, kept unconverted for range analysis |
| `-noise` | `adc_raw` | `mV` | Max minus min of the samples in this cycle |
| `-soil-temp` | `temperature` | `°F` | Probe's on-board temperature |
| `-battery` | `battery` | `V` | Battery voltage |
| `-battery-pct` | `battery` | `%` | Estimated state of charge |

Soil moisture is sent as a raw value. Converting it to a percentage is the
server's job; see [Calibration](#calibration).

### `GET /api/firmware/check?deviceId=<id>&currentBuild=<n>`

The firmware expects `200` and a compact JSON body. If `updateAvailable` is
`true` it downloads `url` and flashes it to the inactive OTA partition.

```json
{"updateAvailable":true,"buildNumber":20,"url":"http://192.168.1.100:3001/firmware/soil-sensor.bin"}
```

The response parser is deliberately minimal: flat keys only, and no whitespace
between a key and its colon.

### Security

This design assumes a trusted home network. Requests are unauthenticated plain
HTTP, and OTA images are neither signed nor fetched over TLS, so anyone on the
same network who can impersonate the server can replace the firmware. Do not
expose the server endpoints to the internet.

## Calibration

The Catnip probe reports raw capacitance, and the useful range depends on your
soil. As a starting point it reads about 250 in open air and about 650
submerged in water at 3.3 V. For real numbers, take a reading in dry soil,
water thoroughly, wait 30 minutes and take another; use those two values as
the 0% and 100% points on the server.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Scanner finds no devices | Wiring error or MOSFET not switching | Check D1 goes high, the MOSFET orientation, and that SDA/SCL are not swapped. |
| Probe returns 65535 | SDA and SCL swapped | Swap them and re-run the scanner; expect `0x20`. |
| WiFi will not connect | Out of range or wrong credentials | Test closer to the access point. Check `src/secrets.h`. |
| Battery drains fast | Probe staying powered in sleep | Probe VCC to probe GND should read 0 V in deep sleep. Check the 100k gate pull-down. |
| Battery voltage reads 0 or pegged | Divider wired wrong | 1 MOhm from BAT+ to D0, 1 MOhm from D0 to GND. |
| HTTP POST fails, no response | Wrong server address | Check `SERVER_HOST` and `SERVER_PORT` in `src/secrets.h`. |
| HTTP 400 response | Server rejected the payload | Read the payload on the serial console (`VERBOSE_LOG`). |
| Serial monitor shows nothing | USB CDC not enabled | Keep the `ARDUINO_USB_CDC_ON_BOOT=1` build flag. |
| Build fails with "Missing src/secrets.h" | No site config | `cp src/secrets.example.h src/secrets.h` and edit it. |

The assembly guide has its own troubleshooting section for the hardware.

## Other Probes

The firmware also contains code paths for two other probes that were tried
before settling on the Catnip. They compile but have no predefined
environment; enable them with `build_flags`:

| Hardware | Flags | Notes |
|----------|-------|-------|
| Adafruit STEMMA Soil Sensor (I2C `0x36`), optional SHT41 and SCD-41 | `-DCATNIP_SOIL=false`, plus `-DSCD41_ENABLED=true` for CO2 | Moisture unit is `raw`; adds `-air-temp`, `-humidity`, `-co2` readings |
| Seeed XIAO soil moisture board (analog) | `-DCATNIP_SOIL=false -DANALOG_SOIL=true` | Moisture unit is `mV`, inverted: lower is wetter |
| Solar panel voltage sense | `-DSOLAR_ENABLED=true -DSENSOR_POWER_PIN=D9` | Uses D1 for the panel divider, so the MOSFET gate has to move; adds a `-solar` reading |

## Directory Structure

```
.
|-- README.md
|-- LICENSE
|-- TODO.md
|-- platformio.ini            PlatformIO environments
|-- get_network_flags.py      Build helper: include paths for Arduino-ESP32 3.x
|-- fix_wifi_deps.py          Build helper: WiFi -> Network dependency fix
|-- flash-on-wake.sh          Flash a sleeping node the moment it wakes (macOS)
|-- docs/
|   |-- assembly-guide.pdf    Hardware build instructions
|   +-- assembly-guide.md     Source of the PDF
|-- src/
|   |-- main.cpp              Entry point: wake, read, post, OTA check, sleep
|   |-- config.h              Pins, timing, feature flags
|   |-- secrets.example.h     Template for secrets.h (WiFi, server address)
|   |-- sensors.cpp/.h        Sensor init and reads, battery ADC
|   |-- network.cpp/.h        WiFi, JSON payloads, HTTP POST
|   |-- ota.cpp/.h            Pull-based firmware update
|   +-- status_led.cpp/.h     Built-in LED signalling
|-- test/
|   +-- test_logic.cpp        Native unit tests for the pure logic
+-- tools/
    +-- i2c_scanner.cpp       Standalone I2C bus scanner
```

## License

[MIT](LICENSE)
