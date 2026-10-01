#include "ota.h"
#include "config.h"

#include <Arduino.h>
#include <Network.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

// ─── Helpers ───────────────────────────────────────────────

// Minimal JSON value extractor.  Finds "key":value and returns
// the value as a String (without surrounding quotes for strings).
static String jsonValue(const String& json, const char* key) {
    String needle = String("\"") + key + "\":";
    int idx = json.indexOf(needle);
    if (idx < 0) return "";

    idx += needle.length();

    // Skip whitespace
    while (idx < (int)json.length() && json[idx] == ' ') idx++;

    if (json[idx] == '"') {
        // String value — extract between quotes
        int start = idx + 1;
        int end = json.indexOf('"', start);
        if (end < 0) return "";
        return json.substring(start, end);
    }

    // Numeric / boolean — read until comma, brace, or end
    int end = idx;
    while (end < (int)json.length() && json[end] != ',' && json[end] != '}') end++;
    return json.substring(idx, end);
}

// ─── Public API ────────────────────────────────────────────

void otaCheck() {
    Serial.printf("[ota] Checking for updates (current: v%s build %d)...\n",
                  FW_VERSION, BUILD_NUMBER);

    // Ask the server if a newer build exists
    char url[256];
    snprintf(url, sizeof(url),
        "http://%s:%d/api/firmware/check?deviceId=%s&currentBuild=%d",
        SERVER_HOST, SERVER_PORT, DEVICE_ID, BUILD_NUMBER);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(HTTP_TIMEOUT_MS);

    int httpCode = http.GET();
    if (httpCode != 200) {
        Serial.printf("[ota] Check failed (HTTP %d)\n", httpCode);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    // Parse response: { "updateAvailable": true, "buildNumber": 9, "url": "..." }
    String available = jsonValue(body, "updateAvailable");
    if (available != "true") {
        Serial.println("[ota] Firmware is up to date");
        return;
    }

    String downloadUrl = jsonValue(body, "url");
    String newBuild = jsonValue(body, "buildNumber");

    if (downloadUrl.length() == 0) {
        Serial.println("[ota] Server reported update but no download URL");
        return;
    }

    Serial.printf("[ota] Update available! Build %s — downloading from: %s\n",
                  newBuild.c_str(), downloadUrl.c_str());

    // Perform the OTA update (writes to inactive partition, sets boot flag)
    WiFiClient client;
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    t_httpUpdate_return ret = httpUpdate.update(client, downloadUrl);

    switch (ret) {
        case HTTP_UPDATE_OK:
            Serial.println("[ota] Update flashed — rebooting...");
            Serial.flush();
            ESP.restart();
            break;
        case HTTP_UPDATE_FAILED:
            Serial.printf("[ota] Update FAILED: %s (error %d)\n",
                httpUpdate.getLastErrorString().c_str(),
                httpUpdate.getLastError());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("[ota] Server said no update needed");
            break;
    }
}
