#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>

// Per-sensor availability + readings.
// Each sensor is independently failable — if a sensor is missing or broken,
// its `available` flag is false and its values are zeroed.
struct SensorData {
    // STEMMA Soil Sensor (I2C 0x36) or Catnip/Chirp (I2C 0x20)
    bool soilAvailable;
    uint16_t soilMoisture;    // Raw value (STEMMA capacitive / Catnip capacitive / analog mV)
    float soilTempC;          // Soil temperature in °C (STEMMA or Catnip)

    // Multi-sample noise measurement
    bool noiseAvailable;
    uint16_t soilRawMedian;   // Median of NOISE_SAMPLES raw readings
    uint16_t soilRawMin;      // Min of NOISE_SAMPLES raw readings
    uint16_t soilRawMax;      // Max of NOISE_SAMPLES raw readings

    // SHT41 (I2C 0x44)
    bool airAvailable;
    float airTempC;           // Air temperature in °C
    float airHumidity;        // Relative humidity %

    // SCD-41 (I2C 0x62) — optional
    bool co2Available;
    uint16_t co2Ppm;          // CO2 concentration in ppm

    // Battery
    bool batteryAvailable;
    float batteryVoltage;     // Measured voltage in V
    int batteryPercent;       // Estimated 0–100

    // Solar panel
    bool solarAvailable;
    float solarVoltage;       // Panel voltage in V

    // WiFi (filled in by network module)
    int rssi;
};

// Initialize MOSFET power pin, I2C bus, and probe each sensor.
// Returns true if at least one sensor was found.
bool sensorsInit();

// Read all available sensors and battery. Populates the SensorData struct.
void sensorsRead(SensorData* data);

// Power off sensors via MOSFET.
void sensorsPowerOff();

// Utility
float celsiusToFahrenheit(float c);
int lipoBatteryPercent(float voltage);

#endif
