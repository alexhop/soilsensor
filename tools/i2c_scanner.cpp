/*
 * I2C Bus Scanner — Diagnostic Utility
 *
 * Upload this to the XIAO ESP32C6 to verify sensor wiring.
 * Powers on the MOSFET, scans the I2C bus, and labels known devices.
 * Rescans every 5 seconds.
 *
 * Build with PlatformIO:  pio run -e scanner -t upload
 * Then open serial monitor: pio device monitor
 *
 * Expected results:
 *   0x20 — Catnip/Chirp Soil Sensor
 *   0x36 — STEMMA Soil Sensor
 *   0x44 — SHT41 Temp/Humidity
 *   0x62 — SCD-41 CO2
 */

#include <Arduino.h>
#include <Wire.h>

// Must match SENSOR_POWER_PIN in src/config.h for your build.
#ifndef SENSOR_POWER_PIN
#define SENSOR_POWER_PIN  D9
#endif
#define I2C_SDA           D4
#define I2C_SCL           D5

static const char* knownDevice(uint8_t addr) {
    switch (addr) {
        case 0x20: return "Catnip/Chirp Soil Sensor";
        case 0x36: return "STEMMA Soil Sensor";
        case 0x44: return "SHT41 Temp/Humidity";
        case 0x62: return "SCD-41 CO2";
        default:   return nullptr;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("═══════════════════════════════════");
    Serial.println("  Soil Sensor I2C Scanner");
    Serial.println("═══════════════════════════════════");

    // Power on sensors via MOSFET
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, HIGH);
    Serial.println("MOSFET on — sensors powered");
    delay(1000);  // Let sensors stabilize

    Wire.begin(I2C_SDA, I2C_SCL);
    Serial.println("I2C initialized (SDA=D4, SCL=D5)");
    Serial.println();
}

void loop() {
    Serial.println("Scanning I2C bus (0x01 - 0x7F)...");
    Serial.println("─────────────────────────────────");

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();

        if (err == 0) {
            const char* name = knownDevice(addr);
            if (name) {
                Serial.printf("  0x%02X  ✓  %s\n", addr, name);
            } else {
                Serial.printf("  0x%02X  ?  Unknown device\n", addr);
            }
            found++;
        }
    }

    if (found == 0) {
        Serial.println("  No devices found!");
        Serial.println("  Check: MOSFET wiring, SDA/SCL connections, sensor power");
    } else {
        Serial.printf("\n  %d device(s) found.\n", found);
    }

    Serial.println();
    delay(5000);
}
