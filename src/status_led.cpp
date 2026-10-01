#include <Arduino.h>
#include "status_led.h"

// Uses LED_BUILTIN (GPIO15) — single-color amber LED on XIAO ESP32C6

static bool initialized = false;

static void ensureInit() {
    if (!initialized) {
        pinMode(LED_BUILTIN, OUTPUT);
        digitalWrite(LED_BUILTIN, LOW);
        initialized = true;
    }
}

void ledBoot()         { ensureInit(); digitalWrite(LED_BUILTIN, HIGH); }
void ledWifiStart()    { ensureInit(); digitalWrite(LED_BUILTIN, HIGH); }
void ledWifiConnected(){ ensureInit(); digitalWrite(LED_BUILTIN, HIGH); }
void ledOperational()  { ensureInit(); digitalWrite(LED_BUILTIN, LOW); }

void ledSensorRead() {
    ensureInit();
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
}

void ledOff() {
    ensureInit();
    digitalWrite(LED_BUILTIN, LOW);
}
