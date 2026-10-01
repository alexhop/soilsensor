#ifndef STATUS_LED_H
#define STATUS_LED_H

// Status signalling on the built-in single-color LED of the XIAO ESP32C6.
// The LED is on while a cycle is in progress and off once it completes.

void ledBoot();         // On  — MCU just started
void ledWifiStart();    // On  — attempting WiFi connection
void ledWifiConnected();// On  — WiFi connected
void ledOperational();  // Off — cycle complete
void ledSensorRead();   // 200ms blink — reading sensors
void ledOff();          // Off

#endif
