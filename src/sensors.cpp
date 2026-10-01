#include "sensors.h"
#include "config.h"

#include <Arduino.h>
#include <Wire.h>

#if !ANALOG_SOIL
#include "Adafruit_seesaw.h"
#include <Adafruit_SHT4x.h>
#endif

#if SCD41_ENABLED
#include <SensirionI2CScd4x.h>
#endif

#if CATNIP_SOIL
#include <I2CSoilMoistureSensor.h>
#endif

// ─── Sensor instances ──────────────────────────────────────
#if !ANALOG_SOIL
static Adafruit_seesaw stemma;
static Adafruit_SHT4x  sht41;
#endif

#if SCD41_ENABLED
static SensirionI2CScd4x scd41;
#endif

#if CATNIP_SOIL
static I2CSoilMoistureSensor catnip(ADDR_CATNIP);
static bool catnipFound = false;
#endif

#if !ANALOG_SOIL && !CATNIP_SOIL
static bool stemmaFound = false;
static bool sht41Found  = false;
#endif
static bool scd41Found  = false;

// ─── LiPo discharge curve lookup ─────────────────────────
// Typical single-cell LiPo voltage-to-SoC mapping (no load).
// Points are {voltage, percent} in descending voltage order.
// Interpolates linearly between points for a much better estimate
// than a simple linear V→% mapping — LiPo cells hold ~3.7V for
// most of their capacity and drop off sharply below ~3.5V.
struct VoltPct { float v; int pct; };
static const VoltPct LIPO_CURVE[] = {
    { 4.20f, 100 },
    { 4.10f,  90 },
    { 4.00f,  80 },
    { 3.90f,  70 },
    { 3.80f,  60 },
    { 3.70f,  45 },
    { 3.60f,  30 },
    { 3.50f,  18 },
    { 3.40f,   8 },
    { 3.30f,   0 },
};
static const int LIPO_CURVE_LEN = sizeof(LIPO_CURVE) / sizeof(LIPO_CURVE[0]);

int lipoBatteryPercent(float voltage) {
    // AA battery: simple linear mapping (1.5V fresh → 0.9V dead)
    if (BATTERY_FULL_V < 2.0f) {
        float pct = (voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;
        if (pct > 100) return 100;
        if (pct < 0) return 0;
        return (int)pct;
    }
    // LiPo: curve lookup
    if (voltage >= LIPO_CURVE[0].v) return 100;
    if (voltage <= LIPO_CURVE[LIPO_CURVE_LEN - 1].v) return 0;
    for (int i = 0; i < LIPO_CURVE_LEN - 1; i++) {
        if (voltage >= LIPO_CURVE[i + 1].v) {
            float vRange = LIPO_CURVE[i].v - LIPO_CURVE[i + 1].v;
            float pRange = (float)(LIPO_CURVE[i].pct - LIPO_CURVE[i + 1].pct);
            float frac = (voltage - LIPO_CURVE[i + 1].v) / vRange;
            return LIPO_CURVE[i + 1].pct + (int)(frac * pRange);
        }
    }
    return 0;
}

// ─── ADC median helper ────────────────────────────────────
// Take 16 samples from an ADC pin and return the median in millivolts.
// Insertion sort on 16 elements is faster than qsort overhead.
// Median rejects RF-induced ADC glitches that would drag down an average.
static float medianAdcMillivolts(int pin) {
    uint16_t samples[16];
    for (int i = 0; i < 16; i++)
        samples[i] = (uint16_t)analogReadMilliVolts(pin);
    for (int i = 1; i < 16; i++) {
        uint16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    return (samples[7] + samples[8]) / 2.0f;
}

// ─── I2C probe helper ──────────────────────────────────────
#if !ANALOG_SOIL && !CATNIP_SOIL
static bool i2cProbe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}
#endif

// ─── Multi-sample noise measurement ──────────────────────
// Takes NOISE_SAMPLES readings, computes median/min/max.
// Insertion sort on small array for median.
static void computeNoise(uint16_t* samples, int count,
                         uint16_t& median, uint16_t& lo, uint16_t& hi) {
    // Find min/max before sorting
    lo = samples[0];
    hi = samples[0];
    for (int i = 1; i < count; i++) {
        if (samples[i] < lo) lo = samples[i];
        if (samples[i] > hi) hi = samples[i];
    }
    // Insertion sort for median
    for (int i = 1; i < count; i++) {
        uint16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    median = samples[count / 2];  // Odd count → middle element
}

// ─── Public API ────────────────────────────────────────────

bool sensorsInit() {
    // Reset flags so USB-mode cycles re-probe (battery mode reboots each cycle)
#if !ANALOG_SOIL && !CATNIP_SOIL
    stemmaFound = false;
    sht41Found  = false;
#endif
#if CATNIP_SOIL
    catnipFound = false;
#endif
    scd41Found  = false;

#if ANALOG_SOIL
    // XIAO Soil Sensor board power control pins.
    // Seeed's official ESPHome firmware sets GPIO3 LOW and GPIO14 HIGH at boot.
    // These likely gate power to the sensor excitation/detection circuit on the PCB.
    pinMode(3, OUTPUT);
    digitalWrite(3, LOW);
    pinMode(14, OUTPUT);
    digitalWrite(14, HIGH);
    delay(50);

    // Start PWM excitation for capacitive sensor
    ledcAttach(SOIL_PWM_PIN, SOIL_PWM_FREQ, 8);  // 8-bit resolution
    ledcWrite(SOIL_PWM_PIN, SOIL_PWM_DUTY);
    delay(SOIL_SETTLE_MS);
    Serial.println("[sensors] Analog soil sensor: PWM excitation started on D3");
    return true;
#elif CATNIP_SOIL
    // Catnip/Chirp I2C soil sensor. Its ground returns through the MOSFET,
    // so it is unpowered until the gate goes high.
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, HIGH);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);
    delay(100);

    catnip.begin();
    delay(1000);  // Catnip ATtiny needs time after power-on

    // Verify communication by reading version
    unsigned int ver = catnip.getVersion();
    if (ver > 0 && ver < 0xFFFF) {
        catnipFound = true;
        Serial.printf("[sensors] Catnip/Chirp found (0x%02X), firmware v%u\n",
                      ADDR_CATNIP, ver);
    } else {
        Serial.println("[sensors] Catnip/Chirp NOT FOUND — skipping");
    }

    Serial.printf("[sensors] %d of 1 sensors online\n", (int)catnipFound);
    return catnipFound;
#else
    // I2C sensor board: power on via MOSFET, probe each sensor
    pinMode(SENSOR_POWER_PIN, OUTPUT);
    digitalWrite(SENSOR_POWER_PIN, HIGH);
    delay(SENSOR_WARMUP_MS);

    // Init I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(100000);  // 100 kHz — safe default for seesaw
    delay(100);             // Let bus stabilize before probing

    // Probe STEMMA soil sensor
    if (i2cProbe(ADDR_STEMMA)) {
        if (stemma.begin(ADDR_STEMMA)) {
            stemmaFound = true;
            Serial.println("[sensors] STEMMA soil sensor found (0x36)");
        } else {
            Serial.println("[sensors] STEMMA at 0x36 but begin() failed");
        }
    } else {
        Serial.println("[sensors] STEMMA soil sensor NOT FOUND — skipping");
    }

    // Probe SHT41
    if (i2cProbe(ADDR_SHT41)) {
        if (sht41.begin()) {
            sht41.setPrecision(SHT4X_HIGH_PRECISION);
            sht41Found = true;
            Serial.println("[sensors] SHT41 found (0x44)");
        } else {
            Serial.println("[sensors] SHT41 at 0x44 but begin() failed");
        }
    } else {
        Serial.println("[sensors] SHT41 NOT FOUND — skipping");
    }

    // Probe SCD-41
#if SCD41_ENABLED
    scd41.begin(Wire);
    if (i2cProbe(ADDR_SCD41)) {
        // Stop any lingering periodic measurement from a previous boot
        scd41.stopPeriodicMeasurement();
        delay(500);
        scd41Found = true;
        Serial.println("[sensors] SCD-41 found (0x62)");
    } else {
        Serial.println("[sensors] SCD-41 NOT FOUND — skipping");
    }
#else
    Serial.println("[sensors] SCD-41 disabled in config");
#endif

    int found = (int)stemmaFound + (int)sht41Found + (int)scd41Found;
    Serial.printf("[sensors] %d of %d sensors online\n",
                  found, SCD41_ENABLED ? 3 : 2);
    return found > 0;
#endif
}

void sensorsRead(SensorData* data) {
    // Zero out
    memset(data, 0, sizeof(SensorData));

    // ── Multi-sample noise measurement ──
    uint16_t noiseSamples[NOISE_SAMPLES];

#if ANALOG_SOIL
    // ── Analog Soil Sensor (XIAO Soil Sensor board) ──
    // Read capacitive sensor via ADC. PWM excitation is already running.
    // Higher voltage = drier soil, lower voltage = wetter soil.
    analogSetAttenuation(ADC_11db);
    // Discard first read — ADC and PWM excitation need time to stabilize
    // after deep sleep wake. Without this, the first read returns ~0 mV
    // which the server converts to 100% moisture (a phantom reading).
    medianAdcMillivolts(SOIL_ADC_PIN);
    delay(200);

    for (int i = 0; i < NOISE_SAMPLES; i++) {
        float soilMv = medianAdcMillivolts(SOIL_ADC_PIN);
        noiseSamples[i] = (uint16_t)soilMv;
        if (i < NOISE_SAMPLES - 1) delay(50);
    }

    uint16_t med, lo, hi;
    computeNoise(noiseSamples, NOISE_SAMPLES, med, lo, hi);

#if VERBOSE_LOG
    Serial.printf("[sensors] Soil ADC: median=%u min=%u max=%u spread=%u (pin D1)\n",
                  med, lo, hi, hi - lo);
#endif

    if (med > 0) {
        data->soilMoisture = med;
        data->soilAvailable = true;
        data->noiseAvailable = true;
        data->soilRawMedian = med;
        data->soilRawMin = lo;
        data->soilRawMax = hi;
        Serial.printf("[sensors] Soil moisture: %u mV (noise: %u)\n", med, hi - lo);
    }

    // Stop PWM to save power
    ledcWrite(SOIL_PWM_PIN, 0);

#elif CATNIP_SOIL
    // ── Catnip/Chirp I2C Soil Sensor ──
    if (catnipFound) {
        for (int i = 0; i < NOISE_SAMPLES; i++) {
            // Read-discard-wait-read protocol:
            // First getCapacitance() returns stale data and triggers new measurement
            catnip.getCapacitance();
            // Poll until measurement completes (up to 2 seconds)
            for (int w = 0; w < 20 && catnip.isBusy(); w++) {
                delay(100);
            }
            // Second read gets the fresh value
            noiseSamples[i] = catnip.getCapacitance();
        }

        uint16_t med, lo, hi;
        computeNoise(noiseSamples, NOISE_SAMPLES, med, lo, hi);

#if VERBOSE_LOG
        Serial.printf("[sensors] Catnip: median=%u min=%u max=%u spread=%u\n",
                      med, lo, hi, hi - lo);
#endif

        data->soilMoisture = med;
        data->soilAvailable = true;
        data->noiseAvailable = true;
        data->soilRawMedian = med;
        data->soilRawMin = lo;
        data->soilRawMax = hi;
        Serial.printf("[sensors] Soil moisture: %u raw (noise: %u)\n", med, hi - lo);

        // Temperature (returns tenths of °C, e.g. 252 = 25.2°C)
        int rawTemp = catnip.getTemperature();
        if (rawTemp > -400 && rawTemp < 850) {  // -40.0°C to 85.0°C
            data->soilTempC = rawTemp / 10.0f;
            Serial.printf("[sensors] Catnip temp: %.1f°C (%.1f°F)\n",
                          data->soilTempC, celsiusToFahrenheit(data->soilTempC));
        }
    }

#else
    // ── STEMMA Soil Sensor ──
    if (stemmaFound) {
        delay(250);

        for (int i = 0; i < NOISE_SAMPLES; i++) {
            noiseSamples[i] = stemma.touchRead(0);
            if (i < NOISE_SAMPLES - 1) delay(100);
        }

        uint16_t med, lo, hi;
        computeNoise(noiseSamples, NOISE_SAMPLES, med, lo, hi);

#if VERBOSE_LOG
        Serial.printf("[sensors] STEMMA: median=%u min=%u max=%u spread=%u\n",
                      med, lo, hi, hi - lo);
#endif

        data->soilMoisture = med;
        data->soilTempC    = stemma.getTemp();
        data->soilAvailable = true;
        data->noiseAvailable = true;
        data->soilRawMedian = med;
        data->soilRawMin = lo;
        data->soilRawMax = hi;
        Serial.printf("[sensors] Soil: moisture=%u, temp=%.1f°C (noise: %u)\n",
                      med, data->soilTempC, hi - lo);
    }

    // ── SHT41 ──
    if (sht41Found) {
        sensors_event_t humEvent, tempEvent;
        if (sht41.getEvent(&humEvent, &tempEvent)) {
            data->airTempC    = tempEvent.temperature;
            data->airHumidity = humEvent.relative_humidity;
            data->airAvailable = true;
            Serial.printf("[sensors] Air: temp=%.1f°C (%.1f°F), humidity=%.1f%%\n",
                          data->airTempC,
                          celsiusToFahrenheit(data->airTempC),
                          data->airHumidity);
        } else {
            Serial.println("[sensors] SHT41 read failed");
        }
    }

    // ── SCD-41 (CO2) ──
#if SCD41_ENABLED
    if (scd41Found) {
        // Single-shot measurement — lower power than continuous mode
        uint16_t error = scd41.measureSingleShot();
        if (error == 0) {
            // Poll ready flag instead of blind 5.5s wait — exits early when data is available
            bool ready = false;
            for (int i = 0; i < 12 && !ready; i++) {
                delay(500);
#ifdef ESP_TASK_WDT_ENABLED
                esp_task_wdt_reset();
#endif
                scd41.getDataReadyFlag(ready);
            }
            uint16_t co2 = 0;
            float scdTemp = 0, scdHum = 0;
            if (ready) {
                error = scd41.readMeasurement(co2, scdTemp, scdHum);
                if (error == 0 && co2 > 0) {
                    data->co2Ppm = co2;
                    data->co2Available = true;
                    Serial.printf("[sensors] CO2: %u ppm\n", co2);
                } else {
                    Serial.println("[sensors] SCD-41 read returned invalid data");
                }
            } else {
                Serial.println("[sensors] SCD-41 data not ready after wait");
            }
        } else {
            Serial.printf("[sensors] SCD-41 measureSingleShot error: %u\n", error);
        }
    }
#endif
#endif

    // ── Battery ADC ──
    // Use analogReadMilliVolts() for factory-calibrated readings (ESP32-C6 errata:
    // analogRead() returns wrong values due to missing lower 4 bits on rev 0.01).
    analogSetAttenuation(ADC_11db);
    float adcMv = medianAdcMillivolts(BATTERY_ADC_PIN);
    float adcV = adcMv / 1000.0f;
#if VERBOSE_LOG
    Serial.printf("[sensors] Battery ADC: medianMv=%.0f (pin %d)\n", adcMv, BATTERY_ADC_PIN);
#endif
    if (adcMv > 0) {
        data->batteryVoltage = adcV * ((BATTERY_R1 + BATTERY_R2) / BATTERY_R2);
        if (data->batteryVoltage > BATTERY_FULL_V) data->batteryVoltage = BATTERY_FULL_V;
        if (data->batteryVoltage < 0) data->batteryVoltage = 0;
        data->batteryPercent = lipoBatteryPercent(data->batteryVoltage);
        data->batteryAvailable = true;
        Serial.printf("[sensors] Battery: %.2fV (%d%%)\n",
                      data->batteryVoltage, data->batteryPercent);
    }

    // ── Solar Panel ADC ──
#if SOLAR_ENABLED
    float solarMv = medianAdcMillivolts(SOLAR_ADC_PIN);
    float solarAdcV = solarMv / 1000.0f;
#if VERBOSE_LOG
    Serial.printf("[sensors] Solar ADC: medianMv=%.0f (pin %d)\n", solarMv, SOLAR_ADC_PIN);
#endif
    if (solarMv > 0) {
        data->solarVoltage = solarAdcV * ((SOLAR_R1 + SOLAR_R2) / SOLAR_R2);
        data->solarAvailable = true;
        Serial.printf("[sensors] Solar: %.2fV\n", data->solarVoltage);
    }
#endif
}

void sensorsPowerOff() {
#if ANALOG_SOIL
    Serial.println("[sensors] Analog soil — no MOSFET to power off");
#elif CATNIP_SOIL
    // sleep() covers a probe wired straight to GND; the MOSFET covers the rest.
    catnip.sleep();
    digitalWrite(SENSOR_POWER_PIN, LOW);
    Serial.println("[sensors] Catnip off (sleep + MOSFET LOW)");
#else
    digitalWrite(SENSOR_POWER_PIN, LOW);
    Serial.println("[sensors] Power off (MOSFET LOW)");
#endif
}

float celsiusToFahrenheit(float c) {
    return c * 9.0f / 5.0f + 32.0f;
}
