#ifndef CONFIG_H
#define CONFIG_H

// ─── Build ───────────────────────────────────────────────────
#define BUILD_NUMBER    20
#define FW_VERSION      "0.11.0"

// ─── Device Identity ───────────────────────────────────────
// Each physical sensor node gets a unique prefix.
// Individual sensor IDs are constructed as: DEVICE_ID + "-moisture", "-soil-temp", etc.
// These can be overridden per-device via build_flags in platformio.ini.
#ifndef DEVICE_ID
#define DEVICE_ID       "soil-1"
#endif
#ifndef DEVICE_HOSTNAME
#define DEVICE_HOSTNAME "SoilSensor-001"
#endif
#ifndef DEVICE_LOCATION
#define DEVICE_LOCATION "Garden"
#endif

// ─── WiFi + Server ─────────────────────────────────────────
// WIFI_SSID, WIFI_PASSWORD, SERVER_HOST and SERVER_PORT are site-specific and
// live in secrets.h, which is gitignored so credentials never get committed.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing src/secrets.h - copy src/secrets.example.h to src/secrets.h and fill in your WiFi and server settings"
#endif

#define WIFI_TIMEOUT_MS 15000     // Max wait per attempt
#define WIFI_RETRIES    3         // Number of connection attempts

// The batch endpoint accepts an array of sensor readings.
#define API_PATH        "/api/sensors/batch"

// ─── Pins (XIAO ESP32C6 silk labels) ──────────────────────
#ifndef SENSOR_POWER_PIN
#define SENSOR_POWER_PIN  D1    // MOSFET gate — sensor power control
#endif
#define I2C_SDA           D4    // I2C data
#define I2C_SCL           D5    // I2C clock
#define BATTERY_ADC_PIN   D0    // Battery voltage via divider (GPIO0, ADC capable)
// Shares D1 with the default MOSFET gate: move SENSOR_POWER_PIN before enabling solar sense.
#define SOLAR_ADC_PIN     D1    // Solar panel voltage via divider (GPIO1, ADC capable)

// ─── I2C Addresses ────────────────────────────────────────
#define ADDR_STEMMA   0x36
#define ADDR_SHT41    0x44
#define ADDR_SCD41    0x62
#define ADDR_CATNIP   0x20

// ─── Timing ───────────────────────────────────────────────
#define SLEEP_SECONDS_USB   30        // Loop interval when USB-powered

// Adaptive sleep tiers (battery mode) — longer intervals at lower charge
#define SLEEP_TIER_FULL_MIN    5      // Battery ≥ 90%
#define SLEEP_TIER_HIGH_MIN   15      // Battery ≥ 60%
#define SLEEP_TIER_MED_MIN    30      // Battery ≥ 30%
#define SLEEP_TIER_LOW_MIN    60      // Battery < 30%
#define SENSOR_WARMUP_MS    2000      // Wait after MOSFET on (seesaw needs ~1.5s)
#define SCD41_MEASURE_MS    5500      // SCD-41 single-shot duration
#define HTTP_TIMEOUT_MS     10000     // HTTP request timeout

// ─── Force USB Mode ──────────────────────────────────────
// When true, skip USB SOF detection and always use the USB loop (no deep sleep).
// Useful for test rigs where USB is plugged in but SOF detection is unreliable
// (e.g. after deep sleep wake, the host hasn't re-enumerated yet).
#ifndef FORCE_USB_MODE
#define FORCE_USB_MODE      false
#endif

// ─── Logging ──────────────────────────────────────────────
// Set to false in production battery builds to save flash from format strings.
#ifndef VERBOSE_LOG
#define VERBOSE_LOG         true
#endif

// ─── Features ─────────────────────────────────────────────
#ifndef SCD41_ENABLED
#define SCD41_ENABLED       false     // Set true when an SCD-41 is wired up (STEMMA builds)
#endif

// ─── Analog Soil Sensor (XIAO Soil Sensor board) ─────────
// When ANALOG_SOIL is true, the firmware reads soil moisture via ADC
// instead of I2C STEMMA. The board uses PWM excitation on one pin
// and reads the capacitive response on another.
#ifndef ANALOG_SOIL
#define ANALOG_SOIL         false
#endif

#if ANALOG_SOIL && SOLAR_ENABLED
#error "SOIL_ADC_PIN and SOLAR_ADC_PIN both use D1 — cannot enable both"
#endif

#if ANALOG_SOIL
#define SOIL_ADC_PIN        D1        // GPIO1 — capacitive sensor analog input
#define SOIL_PWM_PIN        D3        // GPIO21 — 200kHz excitation signal
#define SOIL_PWM_FREQ       200000    // Excitation frequency in Hz
#define SOIL_PWM_DUTY       174       // ~68% of 255 (8-bit resolution)
#define SOIL_PWM_CHANNEL    0         // LEDC channel
#define SOIL_SETTLE_MS      300       // Wait after PWM starts before ADC read
// Calibration: ADC millivolt readings for dry and wet soil.
// These are defaults — adjust per-device after testing.
#define SOIL_DRY_MV         2750      // ~2.75V when dry
#define SOIL_WET_MV         1200      // ~1.20V when wet (saturated)
#endif

// ─── Catnip/Chirp I2C Soil Sensor ───────────────────────
// Capacitive sensor from Catnip Electronics, ATtiny44A MCU.
// I2C address 0x20 (configurable). Returns raw capacitance values.
// Read protocol: first read returns PREVIOUS measurement and triggers
// a new one. Poll isBusy(), then read again for fresh data.
// This is the default probe. Set false to use the STEMMA or analog probe.
#ifndef CATNIP_SOIL
#define CATNIP_SOIL         true
#endif

#if CATNIP_SOIL && ANALOG_SOIL
#error "CATNIP_SOIL and ANALOG_SOIL are mutually exclusive"
#endif

#if CATNIP_SOIL
// Calibration: raw capacitance values at 3.3V supply.
// These are estimates — calibrate per-sensor in air and water.
#define CATNIP_DRY          250       // Raw value in open air
#define CATNIP_WET          650       // Raw value submerged in water
#endif

// ─── Noise Measurement ──────────────────────────────────
// Number of sensor reads per cycle for noise analysis.
// Median is reported as the primary reading; (max - min) as noise.
#ifndef NOISE_SAMPLES
#define NOISE_SAMPLES       5
#endif

// ─── Fixed Sleep Interval ────────────────────────────────
// When set > 0, overrides adaptive sleep tiers with a fixed interval.
// Used for devices without reliable battery percentage (e.g. AA cells).
#ifndef FIXED_SLEEP_MIN
#define FIXED_SLEEP_MIN     0         // 0 = use adaptive tiers
#endif

// ─── Battery ADC ──────────────────────────────────────────
// Voltage divider: R1 (top, to BAT+) and R2 (bottom, to GND).
// ADC reads the midpoint. Vbat = Vadc * (R1 + R2) / R2.
// 220kΩ/220kΩ recommended per Seeed wiki (~9µA drain vs ~20µA with 100k).
// If no divider is installed, set both to 1.0 and read directly.
#ifndef BATTERY_R1
#define BATTERY_R1       220000.0f
#endif
#ifndef BATTERY_R2
#define BATTERY_R2       220000.0f
#endif
#ifndef BATTERY_FULL_V
#define BATTERY_FULL_V   4.2f
#endif
#ifndef BATTERY_EMPTY_V
#define BATTERY_EMPTY_V  3.3f
#endif

// ─── Solar ADC ───────────────────────────────────────────
// Voltage divider on solar panel input (CN3065 IN+).
// Same 220kΩ/220kΩ divider halves the panel voltage for ADC.
#ifndef SOLAR_ENABLED
#define SOLAR_ENABLED    false
#endif
#define SOLAR_R1         220000.0f
#define SOLAR_R2         220000.0f

#endif
