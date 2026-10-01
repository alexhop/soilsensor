#include "network.h"
#include "config.h"

#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_netif.h"

// ─── Helpers ───────────────────────────────────────────────

// Advance pos by snprintf return value, clamping to prevent overflow.
// snprintf returns the count that *would have* been written, which can
// exceed bufSize on truncation — so we cap at (bufSize - 1).
static void advancePos(int& pos, int written, int bufSize) {
    if (written > 0) {
        int remaining = bufSize - pos;
        pos += (written < remaining) ? written : remaining - 1;
    }
}

// Append a single sensor reading JSON object to the buffer.
// Returns number of chars written (excluding null terminator).
static int appendReading(char* buf, int bufSize,
                         const char* sensorId, const char* type,
                         float value, const char* unit,
                         const char* location, int battery,
                         bool hasBattery, bool isInt) {
    if (isInt) {
        if (hasBattery) {
            return snprintf(buf, bufSize,
                "{\"sensorId\":\"%s\",\"type\":\"%s\",\"value\":%d,"
                "\"unit\":\"%s\",\"location\":\"%s\",\"battery\":%d}",
                sensorId, type, (int)value, unit, location, battery);
        }
        return snprintf(buf, bufSize,
            "{\"sensorId\":\"%s\",\"type\":\"%s\",\"value\":%d,"
            "\"unit\":\"%s\",\"location\":\"%s\"}",
            sensorId, type, (int)value, unit, location);
    }
    if (hasBattery) {
        return snprintf(buf, bufSize,
            "{\"sensorId\":\"%s\",\"type\":\"%s\",\"value\":%.1f,"
            "\"unit\":\"%s\",\"location\":\"%s\",\"battery\":%d}",
            sensorId, type, value, unit, location, battery);
    }
    return snprintf(buf, bufSize,
        "{\"sensorId\":\"%s\",\"type\":\"%s\",\"value\":%.1f,"
        "\"unit\":\"%s\",\"location\":\"%s\"}",
        sensorId, type, value, unit, location);
}

// ─── Public API ────────────────────────────────────────────

bool networkConnect() {
    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    // Arduino setHostname() doesn't propagate to DHCP on ESP32-C6/S3.
    // Use ESP-IDF netif API directly.
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_set_hostname(netif, DEVICE_HOSTNAME);
    }
    WiFi.setAutoReconnect(false);

    for (int attempt = 1; attempt <= WIFI_RETRIES; attempt++) {
        Serial.printf("[network] WiFi attempt %d/%d (hostname=%s)...\n",
                      attempt, WIFI_RETRIES, DEVICE_HOSTNAME);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - start > WIFI_TIMEOUT_MS) break;
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[network] Connected in %lu ms — IP: %s, RSSI: %d dBm\n",
                          millis() - start,
                          WiFi.localIP().toString().c_str(),
                          WiFi.RSSI());
            return true;
        }

        Serial.println("[network] WiFi timeout, retrying...");
        WiFi.disconnect(true);
        delay(1000);
    }

    Serial.println("[network] WiFi failed after all attempts");
    return false;
}

bool networkPost(const SensorData& data) {
    // Build JSON array payload using snprintf.
    // Max 12 readings × ~130 chars each = ~1560 bytes.
    char payload[1600];
    const int maxPos = sizeof(payload) - 2;  // reserve room for ] and \0
    int pos = 0;
    int count = 0;
    bool hasBatt = data.batteryAvailable;
    int batt = data.batteryPercent;

    payload[pos++] = '[';

    // Soil moisture (raw capacitive value or millivolt ADC)
    if (data.soilAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-moisture", DEVICE_ID);
#if ANALOG_SOIL
        // Analog sensor: send millivolts (server converts using mV calibration)
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "soil_moisture",
                             (float)data.soilMoisture, "mV",
                             DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
#elif CATNIP_SOIL
        // Catnip/Chirp: send raw capacitance (server converts using chirp calibration)
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "soil_moisture",
                             (float)data.soilMoisture, "chirp",
                             DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
#else
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "soil_moisture",
                             (float)data.soilMoisture, "raw",
                             DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
#endif
        count++;

        // Raw median value (stored as-is for dynamic range analysis)
        if (data.noiseAvailable) {
            payload[pos++] = ',';
            snprintf(id, sizeof(id), "%s-moisture-raw", DEVICE_ID);
            advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                                 id, "adc_raw",
                                 (float)data.soilRawMedian, "mV",
                                 DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
            count++;

            // Noise spread (max - min of NOISE_SAMPLES readings)
            payload[pos++] = ',';
            snprintf(id, sizeof(id), "%s-noise", DEVICE_ID);
            advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                                 id, "adc_raw",
                                 (float)(data.soilRawMax - data.soilRawMin), "mV",
                                 DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
            count++;
        }
    }

    // Soil temperature (from STEMMA or Catnip — not available on analog soil boards)
#if !ANALOG_SOIL
    if (data.soilAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-soil-temp", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "temperature",
                             celsiusToFahrenheit(data.soilTempC), "°F",
                             DEVICE_LOCATION, batt, hasBatt, false), sizeof(payload));
        count++;
    }
#endif

    // Air temperature (from SHT41)
    if (data.airAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-air-temp", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "temperature",
                             celsiusToFahrenheit(data.airTempC), "°F",
                             DEVICE_LOCATION, batt, hasBatt, false), sizeof(payload));
        count++;
    }

    // Air humidity (from SHT41)
    if (data.airAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-humidity", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "humidity",
                             data.airHumidity, "%",
                             DEVICE_LOCATION, batt, hasBatt, false), sizeof(payload));
        count++;
    }

    // CO2 (from SCD-41)
    if (data.co2Available) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-co2", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "co2",
                             (float)data.co2Ppm, "ppm",
                             DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
        count++;
    }

    // Battery voltage
    if (data.batteryAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-battery", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "battery",
                             data.batteryVoltage, "V",
                             DEVICE_LOCATION, batt, hasBatt, false), sizeof(payload));
        count++;

        // Battery percentage (from LiPo discharge curve)
        payload[pos++] = ',';
        snprintf(id, sizeof(id), "%s-battery-pct", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "battery",
                             (float)data.batteryPercent, "%",
                             DEVICE_LOCATION, batt, hasBatt, true), sizeof(payload));
        count++;
    }

    // Solar panel voltage
    if (data.solarAvailable) {
        if (count > 0) payload[pos++] = ',';
        char id[64];
        snprintf(id, sizeof(id), "%s-solar", DEVICE_ID);
        advancePos(pos, appendReading(payload + pos, sizeof(payload) - pos,
                             id, "solar",
                             data.solarVoltage, "V",
                             DEVICE_LOCATION, batt, hasBatt, false), sizeof(payload));
        count++;
    }

    if (pos >= (int)sizeof(payload) - 1) pos = sizeof(payload) - 2;
    payload[pos++] = ']';
    payload[pos] = '\0';

    if (count == 0) {
        Serial.println("[network] No sensor data to send — skipping POST");
        return true;  // Not a failure, just nothing to report
    }

    Serial.printf("[network] Posting %d readings (%d bytes)\n", count, pos);
#if VERBOSE_LOG
    Serial.printf("[network] Payload: %s\n", payload);
#endif

    // Build URL
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", SERVER_HOST, SERVER_PORT, API_PATH);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.POST(payload);
    String response = http.getString();
    http.end();

    if (httpCode == 201) {
        Serial.printf("[network] Server response: 201 OK — %s\n", response.c_str());
        return true;
    } else if (httpCode > 0) {
        Serial.printf("[network] Server response: %d — %s\n", httpCode, response.c_str());
        return false;
    } else {
        Serial.printf("[network] HTTP error: %s\n", http.errorToString(httpCode).c_str());
        return false;
    }
}

bool networkHeartbeat(const SensorData& data, int sensorsOnline) {
    // Build heartbeat JSON payload
    char payload[512];
    int pos = 0;

    advancePos(pos, snprintf(payload + pos, sizeof(payload) - pos,
        "{\"deviceId\":\"%s\""
        ",\"firmwareVersion\":\"%s\""
        ",\"buildNumber\":%d"
        ",\"location\":\"%s\""
        ",\"ip\":\"%s\""
        ",\"rssi\":%d"
        ",\"uptimeSeconds\":%lu"
        ",\"sensorsOnline\":%d",
        DEVICE_ID, FW_VERSION, BUILD_NUMBER,
        DEVICE_LOCATION,
        WiFi.localIP().toString().c_str(),
        data.rssi,
        millis() / 1000,
        sensorsOnline), sizeof(payload));

    if (data.batteryAvailable) {
        advancePos(pos, snprintf(payload + pos, sizeof(payload) - pos,
            ",\"batteryVoltage\":%.2f,\"batteryPercent\":%d",
            data.batteryVoltage, data.batteryPercent), sizeof(payload));
    }

    if (data.solarAvailable) {
        advancePos(pos, snprintf(payload + pos, sizeof(payload) - pos,
            ",\"solarVoltage\":%.2f", data.solarVoltage), sizeof(payload));
    }

    if (pos >= (int)sizeof(payload) - 1) pos = sizeof(payload) - 2;
    payload[pos++] = '}';
    payload[pos] = '\0';

#if VERBOSE_LOG
    Serial.printf("[network] Heartbeat: %s\n", payload);
#endif

    // Build URL for heartbeat endpoint
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/api/network/heartbeat",
             SERVER_HOST, SERVER_PORT);

    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.POST(payload);
    String response = http.getString();
    http.end();

    if (httpCode == 200) {
        Serial.printf("[network] Heartbeat OK — %s\n", response.c_str());
        return true;
    } else if (httpCode > 0) {
        Serial.printf("[network] Heartbeat response: %d — %s\n", httpCode, response.c_str());
        return false;
    } else {
        Serial.printf("[network] Heartbeat error: %s\n", http.errorToString(httpCode).c_str());
        return false;
    }
}

void networkDisconnect() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    Serial.println("[network] WiFi off");
}
