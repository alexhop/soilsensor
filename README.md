# Soil Sensor

Firmware and build instructions for a battery-powered garden sensor node based
on the Seeed XIAO ESP32-C6. Each node wakes up, reads soil moisture (plus soil
temperature, air temperature, humidity and CO2 where fitted), posts the readings
to a server on the local network over HTTP, checks for a firmware update, and
goes back to deep sleep.

- Firmware: PlatformIO + Arduino framework, version 0.9.0 (build 18)
- Hardware guide: [docs/assembly-guide.pdf](docs/assembly-guide.pdf)
  (source: [docs/assembly-guide.md](docs/assembly-guide.md))

## Supported Hardware

Three soil probes are supported, selected at compile time:

| Probe | Interface | Build flag |
|-------|-----------|------------|
| Adafruit STEMMA Soil Sensor (#4026) | I2C `0x36` | default |
| Catnip Electronics / Chirp | I2C `0x20` | `-DCATNIP_SOIL=true` |
| Seeed XIAO soil moisture board | Analog (ADC + PWM excitation) | `-DANALOG_SOIL=true` |

In a two-week side-by-side dry-down test the Catnip probe gave the cleanest,
most monotonic signal of the three, which is why the assembly guide is written
around it.

Optional extras on the I2C bus: SHT41 (air temperature and humidity, `0x44`)
and SCD-41 (CO2, `0x62`). Missing sensors are detected at boot and skipped.

### PlatformIO Environments

| Env | Hardware | Power | Sleep |
|-----|----------|-------|-------|
| `soil-1` | Custom build: STEMMA + SHT41 + SCD-41, MOSFET power gating | LiPo + solar | Adaptive, 5 to 60 min by battery level |
| `soil-2` | Seeed pre-built soil moisture board, analog probe only | AA cells | Fixed 30 min |
| `soil-T` | Bench unit, same sensors as `soil-1` | USB | 30 s loop while USB is connected |
| `soil-test-seeed` | Comparison rig, analog probe only | USB | Never sleeps |
| `soil-test-stemma` | Comparison rig, STEMMA probe only | USB | Never sleeps |
| `soil-test-catnip` | Comparison rig, Catnip probe only | USB | Never sleeps |
| `scanner` | I2C bus scanner (diagnostic, not the main firmware) | USB | n/a |
| `native` | Host-side unit tests | n/a | n/a |

### Assembly Guide vs. Firmware

Read this before building from the guide. The assembly guide describes the
newest hardware revision: Catnip probe, LiPo + solar, sensor power gated by a
MOSFET on **D1**, and a 1 MOhm battery divider on D0. The firmware in this
repository predates that revision and has no environment for it yet:

- `SENSOR_POWER_PIN` defaults to D9. It can be overridden with
  `-DSENSOR_POWER_PIN=D1`.
- D1 is used as the solar-voltage ADC input unless `-DSOLAR_ENABLED=false`.
- The `CATNIP_SOIL` code path does not drive the MOSFET gate at all, so on the
  guide's hardware the probe would never be powered.

The only Catnip environment today is `soil-test-catnip`, a USB-powered bench
rig with the probe wired straight to 3V3 and GND. Closing this gap is the first
item in [TODO.md](TODO.md).

## Quick Start

```bash
pip install platformio

# Site-specific settings (WiFi credentials, server address). Gitignored.
cp src/secrets.example.h src/secrets.h
$EDITOR src/secrets.h

# Optional: verify wiring with the I2C scanner first
pio run -e scanner -t upload
pio device monitor

# Build and flash the firmware for your hardware
pio run -e soil-1 -t upload
pio device monitor

# Host-side unit tests (no hardware needed)
pio test -e native
```

The ESP32 environments use the
[pioarduino](https://github.com/pioarduino/platform-espressif32) platform
because the stock `espressif32` platform does not support the Arduino framework
on the ESP32-C6. `python3` must be on the PATH; `get_network_flags.py` and
`fix_wifi_deps.py` work around an undeclared `WiFi` to `Network` library
dependency in Arduino-ESP32 3.x.

If the board will not flash, hold BOOT while plugging in USB-C, release, then
upload. A sleeping battery-powered node only enumerates on USB briefly after
each wake; `./flash-on-wake.sh [env]` (macOS) waits for the port and flashes
the already-built firmware the moment it appears.

## Configuration

Site-specific values live in `src/secrets.h` (copied from
`src/secrets.example.h`, never committed):

| Setting | Description |
|---------|-------------|
| `WIFI_SSID` / `WIFI_PASSWORD` | WiFi network to join |
| `SERVER_HOST` / `SERVER_PORT` | Address of the receiving server |

Everything else is in `src/config.h`. Most settings are wrapped in `#ifndef`
so a PlatformIO environment can override them with `build_flags`:

| Setting | Default | Description |
|---------|---------|-------------|
| `DEVICE_ID` | `"soil-1"` | Unique node ID, prefix for all sensor IDs |
| `DEVICE_HOSTNAME` | `"SoilSensor-001"` | DHCP hostname |
| `DEVICE_LOCATION` | `"Garden"` | Human-readable location sent with each reading |
| `SENSOR_POWER_PIN` | `D9` | MOSFET gate for sensor power |
| `ANALOG_SOIL` | `false` | Use the analog probe instead of STEMMA |
| `CATNIP_SOIL` | `false` | Use the Catnip/Chirp probe instead of STEMMA |
| `SCD41_ENABLED` | `true` | Probe for the CO2 sensor |
| `SOLAR_ENABLED` | `true` | Read panel voltage on D1 |
| `FIXED_SLEEP_MIN` | `0` | Fixed sleep interval in minutes; 0 uses the adaptive tiers |
| `FORCE_USB_MODE` | `false` | Never deep sleep, loop every 30 s |
| `BATTERY_R1` / `BATTERY_R2` | `220000.0f` | Battery divider resistors (ohms) |
| `BATTERY_FULL_V` / `BATTERY_EMPTY_V` | `4.2f` / `3.3f` | Battery range; a full voltage below 2.0 selects the linear AA mapping |
| `NOISE_SAMPLES` | `5` | Soil reads per cycle; the median is reported |
| `VERBOSE_LOG` | `true` | Print payloads and ADC details on serial |

## How It Works

Each wake cycle:

1. Power the sensors through the MOSFET and probe the I2C bus.
2. Read every sensor that answered, then the battery and solar ADCs.
3. Power the sensors off.
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

Sent every cycle, even when no sensor was found. The firmware expects `200`.

```json
{
  "deviceId": "soil-1",
  "firmwareVersion": "0.9.0",
  "buildNumber": 18,
  "location": "Garden",
  "ip": "192.168.1.42",
  "rssi": -54,
  "uptimeSeconds": 9,
  "sensorsOnline": 2,
  "batteryVoltage": 3.95,
  "batteryPercent": 75,
  "solarVoltage": 5.4
}
```

The battery and solar fields are omitted when those readings are unavailable.

### `POST /api/sensors/batch`

A JSON array with one object per reading. The firmware expects `201`. Only
sensors that responded are included, and `battery` (percent) is omitted when
no battery reading is available.

```json
[
  {"sensorId": "soil-1-moisture", "type": "soil_moisture", "value": 742, "unit": "raw", "location": "Garden", "battery": 85},
  {"sensorId": "soil-1-soil-temp", "type": "temperature", "value": 64.9, "unit": "°F", "location": "Garden", "battery": 85}
]
```

One physical node produces several logical sensors:

| Sensor ID suffix | `type` | `unit` | Source |
|------------------|--------|--------|--------|
| `-moisture` | `soil_moisture` | `raw`, `chirp` or `mV` | Median soil reading (STEMMA, Catnip or analog probe) |
| `-moisture-raw` | `adc_raw` | `mV` | Same median, kept unconverted for range analysis |
| `-noise` | `adc_raw` | `mV` | Max minus min of the samples in this cycle |
| `-soil-temp` | `temperature` | `°F` | STEMMA or Catnip on-board temperature |
| `-air-temp` | `temperature` | `°F` | SHT41 |
| `-humidity` | `humidity` | `%` | SHT41 |
| `-co2` | `co2` | `ppm` | SCD-41 |
| `-battery` | `battery` | `V` | Battery voltage |
| `-battery-pct` | `battery` | `%` | Estimated state of charge |
| `-solar` | `solar` | `V` | Panel voltage |

Soil moisture is sent as a raw value. Converting it to a percentage is the
server's job; see [Calibration](#calibration).

### `GET /api/firmware/check?deviceId=<id>&currentBuild=<n>`

The firmware expects `200` and a compact JSON body. If `updateAvailable` is
`true` it downloads `url` and flashes it to the inactive OTA partition.

```json
{"updateAvailable":true,"buildNumber":19,"url":"http://192.168.1.100:3001/firmware/soil-1.bin"}
```

The response parser is deliberately minimal: flat keys only, and no whitespace
between a key and its colon.

### Security

This design assumes a trusted home network. Requests are unauthenticated plain
HTTP, and OTA images are neither signed nor fetched over TLS, so anyone on the
same network who can impersonate the server can replace the firmware. Do not
expose the server endpoints to the internet.

## Wiring (`soil-1`)

The assembly guide covers the Catnip build step by step. This section is the
pin map the firmware defaults expect, which is the STEMMA-based `soil-1` build.

| XIAO pin | Connects to |
|----------|-------------|
| D0 | Battery divider midpoint (220k to BAT+, 220k to GND) |
| D1 | Solar divider midpoint (220k to panel +, 220k to GND) |
| D4 | I2C SDA, all sensors |
| D5 | I2C SCL, all sensors |
| D9 | 2N7000 gate, with 10k pull-down to GND |
| 3V3 | Sensor VCC |
| GND | 2N7000 source, divider low sides |
| BAT+ / BAT- (pads on the back) | Solar charger output |

Sensor power is switched on the low side: every sensor ground returns through
the MOSFET, so with the gate low the sensors draw nothing during deep sleep.

```
  3V3 ----+---- STEMMA VCC
          +---- SHT41 VIN
          +---- SCD-41 VIN

  STEMMA GND --+
  SHT41 GND ---+
  SCD-41 GND --+
               |
             DRAIN
  D9 ------- GATE     2N7000 (flat side facing you: S, G, D)
          |  SOURCE
         10k   |
          |    |
  GND ----+----+
```

Parts for this build: XIAO ESP32-C6, Adafruit STEMMA Soil Sensor, SHT41 and
optionally SCD-41 (STEMMA QT), 3.7 V LiPo, CN3065 solar charger, 6 V panel,
2N7000, one 10k and four 220k resistors, an IP65 enclosure and PG-7 cable
glands. Weatherproof the top half of the soil probe with adhesive-lined heat
shrink, as described in the assembly guide.

The `soil-2` environment targets Seeed's pre-built soil moisture board and
needs no wiring. The firmware drives its excitation and power pins directly.

## Calibration

### Soil Moisture

All three probes report raw values, and the useful range depends on your soil.
Take a reading in dry soil, water thoroughly, wait 30 minutes and take another;
use those two numbers as the 0% and 100% points on the server.

Rough starting points:

| Probe | Dry | Wet | Notes |
|-------|-----|-----|-------|
| STEMMA (`raw`) | 200 to 350 in air | 1200 to 2000 saturated | 600 to 900 is moist soil |
| Catnip (`chirp`) | about 250 in air | about 650 in water | At 3.3 V supply |
| Analog (`mV`) | about 2750 | about 1200 | Inverted: lower voltage is wetter |

### Deep Sleep Current

With a multimeter in series between battery and XIAO, expect roughly 15 uA
with the sensors gated off. If it is above 100 uA the MOSFET is probably not
switching off; check the gate pull-down.

## Power Budget

An estimate for a 30-minute interval, not a measurement:

| State | Current | Duration | Energy per cycle |
|-------|---------|----------|------------------|
| Deep sleep | 15 uA | 30 min | 7.5 uAh |
| Sensor read | about 25 mA | 1.5 s | 10.4 uAh |
| WiFi TX | about 120 mA | 2 s | 66.7 uAh |
| **Total** | | | **about 85 uAh** |

At 48 cycles a day that is about 4.1 mAh per day. The SCD-41 adds several
seconds of measurement time per cycle and is not included.

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| No I2C devices found | Wiring error or MOSFET not on | Run the scanner. Check `SENSOR_POWER_PIN` goes high. Check SDA/SCL are not swapped. |
| Catnip returns 65535 | SDA and SCL swapped | Swap them and re-run the scanner; expect `0x20`. |
| STEMMA reads about 200 always | Probe not deep enough | Bury the probe 3 to 4 inches. |
| STEMMA reads about 2000 always | Water above the probe line | Check the heat shrink seal; water wicks along the cable. |
| WiFi will not connect | Out of range or wrong credentials | Test closer to the access point. Check `src/secrets.h`. |
| Battery drains fast | MOSFET not gating | Measure sleep current. Check the gate pull-down. |
| HTTP POST fails, no response | Wrong server address | Check `SERVER_HOST` and `SERVER_PORT` in `src/secrets.h`. |
| HTTP 400 response | Server rejected the payload | Read the payload on the serial console (`VERBOSE_LOG`). |
| Serial monitor shows nothing | USB CDC not enabled | Keep the `ARDUINO_USB_CDC_ON_BOOT=1` build flag. |
| Build fails with "Missing src/secrets.h" | No site config | `cp src/secrets.example.h src/secrets.h` and edit it. |

## Directory Structure

```
.
|-- README.md
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
|   |-- sensors.cpp/.h        Sensor init and reads, battery and solar ADC
|   |-- network.cpp/.h        WiFi, JSON payloads, HTTP POST
|   |-- ota.cpp/.h            Pull-based firmware update
|   +-- status_led.cpp/.h     Built-in LED signalling
|-- test/
|   +-- test_logic.cpp        Native unit tests for the pure logic
+-- tools/
    +-- i2c_scanner.cpp       Standalone I2C bus scanner
```
