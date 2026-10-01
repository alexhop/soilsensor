/*
 * Soil Sensor — ESP32C6 Firmware
 *
 * Hardware: Seeed XIAO ESP32C6
 * Sensors:  STEMMA soil moisture/temp, SHT41 air temp/humidity, SCD-41 CO2
 * Power:    3.7V LiPo + CN3065 solar charger + Voltaic P123 panel
 *           Sensors power-gated via 2N7000 MOSFET on SENSOR_POWER_PIN
 *
 * Each wake cycle:
 *   1. Power on sensors via MOSFET
 *   2. Read all available sensors (gracefully skips missing ones)
 *   3. Power off sensors
 *   4. Connect WiFi
 *   5. HTTP POST batch of readings to the server
 *   6. Check for OTA firmware update (pull-based from the server)
 *   7. Sleep — delay() loop on USB, deep sleep on battery
 *
 * Sleep interval adapts to battery level:
 *   USB power:  30 seconds (fast feedback for development)
 *   ≥ 90%:       5 minutes
 *   ≥ 60%:      15 minutes
 *   ≥ 30%:      30 minutes
 *   < 30%:      60 minutes
 */

#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include "config.h"
#include "sensors.h"
#include "network.h"
#include "status_led.h"
#include "ota.h"

// USB SOF detection registers for ESP32-C6
#include "soc/usb_serial_jtag_reg.h"

// ─── USB detection ────────────────────────────────────────

static bool isUsbPowered() {
    // A USB host sends Start-of-Frame packets every 1ms.
    // Clear the SOF interrupt flag, wait 5ms, then check if new SOFs arrived.
    // This is the same approach ESP-IDF uses in usb_serial_jtag_is_connected().
    REG_WRITE(USB_SERIAL_JTAG_INT_CLR_REG, USB_SERIAL_JTAG_SOF_INT_CLR);
    delay(5);
    return (REG_READ(USB_SERIAL_JTAG_INT_RAW_REG) & USB_SERIAL_JTAG_SOF_INT_RAW) != 0;
}

// ─── Adaptive sleep ──────────────────────────────────────

static int getSleepMinutes(int batteryPercent) {
#if FIXED_SLEEP_MIN > 0
    (void)batteryPercent;
    return FIXED_SLEEP_MIN;
#else
    if (batteryPercent >= 90) return SLEEP_TIER_FULL_MIN;   //  5 min
    if (batteryPercent >= 60) return SLEEP_TIER_HIGH_MIN;   // 15 min
    if (batteryPercent >= 30) return SLEEP_TIER_MED_MIN;    // 30 min
    return SLEEP_TIER_LOW_MIN;                              // 60 min
#endif
}

// Last battery percent from the most recent sensor read (used for sleep decision)
static int lastBatteryPercent = 50;

// ─── Core cycle ──────────────────────────────────────────

void readAndPost() {
    // Read sensors (orange blink)
    ledSensorRead();
    bool anySensor = sensorsInit();
    if (!anySensor) {
        Serial.println("[main] WARNING: No sensors found");
    }
    SensorData data;
    sensorsRead(&data);
    sensorsPowerOff();

    // Stash battery level for sleep decision
    if (data.batteryAvailable) {
        lastBatteryPercent = data.batteryPercent;
    }

    // Count how many sensors are online
    int sensorsOnline = (int)data.soilAvailable + (int)data.airAvailable + (int)data.co2Available;

    // Connect WiFi and post
    ledWifiStart();
    Serial.println("[main] Connecting WiFi...");
    if (networkConnect()) {
        ledWifiConnected();
        data.rssi = WiFi.RSSI();

        // Always send heartbeat (even with zero sensors)
        networkHeartbeat(data, sensorsOnline);

        // Send sensor readings (if any)
        bool posted = networkPost(data);
        Serial.printf("[main] POST %s\n", posted ? "OK" : "FAILED");

        // Check for OTA firmware update (after posting, so we don't lose data)
        otaCheck();

        networkDisconnect();
    } else {
        Serial.println("[main] WARNING: WiFi failed — readings lost");
    }

    ledOperational();
}

// ─── Entry point ─────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(100);

    // Immediately show we're alive
    ledBoot();

    bool usb = FORCE_USB_MODE || isUsbPowered();

    Serial.println();
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║  Soil Sensor — Wake Cycle              ║");
    Serial.println("╚════════════════════════════════════════╝");
    Serial.printf("  Device:   %s\n", DEVICE_ID);
    Serial.printf("  Location: %s\n", DEVICE_LOCATION);
    Serial.printf("  Firmware: v%s (build %d)\n", FW_VERSION, BUILD_NUMBER);
    Serial.printf("  Power:    %s\n", usb ? "USB" : "Battery");
    Serial.println();

    // First reading cycle
    readAndPost();

    if (usb) {
        // USB mode: stay awake, loop with short delay
        Serial.printf("[main] USB powered — looping every %ds\n", SLEEP_SECONDS_USB);
        Serial.println();
        Serial.flush();
    } else {
        // Battery mode: deep sleep based on charge level
        int sleepMin = getSleepMinutes(lastBatteryPercent);
        Serial.printf("[main] Battery at %d%% — deep sleeping for %d minutes\n",
                      lastBatteryPercent, sleepMin);
        Serial.flush();
        esp_deep_sleep(sleepMin * 60ULL * 1000000ULL);
        // Device reboots after deep sleep — never reaches loop()
    }
}

void loop() {
    // Only runs in USB mode — battery mode deep sleeps from setup()
    delay((uint32_t)SLEEP_SECONDS_USB * 1000);

    Serial.println();
    Serial.printf("─── Cycle ─── v%s build %d ─── uptime %lus ───\n",
                  FW_VERSION, BUILD_NUMBER, millis() / 1000);

    readAndPost();
}
