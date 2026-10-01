/*
 * Native unit tests for soil-sensor pure logic.
 * Run with:  pio test -e native
 */

#include <unity.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

// ─── Re-declare testable functions (avoid pulling in Arduino deps) ───

static float celsiusToFahrenheit(float c) {
    return c * 9.0f / 5.0f + 32.0f;
}

// Copied from wifi_client.cpp / network.cpp for testing in isolation.
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

// ─── Temperature conversion ──────────────────────────────

void test_c_to_f_freezing(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 32.0f, celsiusToFahrenheit(0.0f));
}

void test_c_to_f_boiling(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 212.0f, celsiusToFahrenheit(100.0f));
}

void test_c_to_f_body_temp(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 98.6f, celsiusToFahrenheit(37.0f));
}

void test_c_to_f_negative(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -40.0f, celsiusToFahrenheit(-40.0f));
}

// ─── JSON payload builder ────────────────────────────────

void test_append_reading_int_no_battery(void) {
    char buf[256];
    int len = appendReading(buf, sizeof(buf),
        "soil-T-moisture", "soil_moisture",
        750.0f, "raw", "Test", 0, false, true);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_EQUAL_STRING(
        "{\"sensorId\":\"soil-T-moisture\",\"type\":\"soil_moisture\","
        "\"value\":750,\"unit\":\"raw\",\"location\":\"Test\"}",
        buf);
}

void test_append_reading_float_with_battery(void) {
    char buf[256];
    int len = appendReading(buf, sizeof(buf),
        "soil-T-air-temp", "temperature",
        72.5f, "°F", "Garden", 85, true, false);
    TEST_ASSERT_GREATER_THAN(0, len);
    // Check key fields are present
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"value\":72.5"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery\":85"));
}

void test_append_reading_buffer_overflow(void) {
    char buf[10];  // Intentionally too small
    int len = appendReading(buf, sizeof(buf),
        "soil-T-moisture", "soil_moisture",
        750.0f, "raw", "Test", 0, false, true);
    // snprintf returns the number of chars that WOULD have been written
    TEST_ASSERT_GREATER_THAN(10, len);
    // Buffer should be null-terminated and not overflow
    TEST_ASSERT_EQUAL('\0', buf[9]);
}

// ─── LiPo battery curve (mirror from sensors.cpp) ───────

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

static int lipoBatteryPercent(float voltage) {
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

void test_battery_full(void) {
    TEST_ASSERT_EQUAL(100, lipoBatteryPercent(4.2f));
}

void test_battery_empty(void) {
    TEST_ASSERT_EQUAL(0, lipoBatteryPercent(3.3f));
}

void test_battery_half(void) {
    // 3.75V is between 3.70V (45%) and 3.80V (60%) — expect ~52%
    int pct = lipoBatteryPercent(3.75f);
    TEST_ASSERT_INT_WITHIN(3, 52, pct);
}

void test_battery_over_full(void) {
    TEST_ASSERT_EQUAL(100, lipoBatteryPercent(4.5f));
}

void test_battery_below_empty(void) {
    TEST_ASSERT_EQUAL(0, lipoBatteryPercent(2.8f));
}

void test_battery_plateau(void) {
    // The LiPo plateau: 3.6–3.8V should span ~30–60%
    // (linear would say 33–55% — curve gives more weight to this range)
    int at_3_7 = lipoBatteryPercent(3.70f);
    TEST_ASSERT_EQUAL(45, at_3_7);
    int at_3_6 = lipoBatteryPercent(3.60f);
    TEST_ASSERT_EQUAL(30, at_3_6);
}

void test_battery_low_end(void) {
    // Below 3.5V drops off sharply
    int at_3_4 = lipoBatteryPercent(3.40f);
    TEST_ASSERT_EQUAL(8, at_3_4);
}

// ─── Voltage divider calculation ─────────────────────────

static float dividerVoltage(float adcV, float r1, float r2) {
    return adcV * ((r1 + r2) / r2);
}

void test_divider_220k_half_battery(void) {
    // 220k/220k divider: ADC sees half of battery voltage
    // If ADC reads 2.1V, actual battery is 4.2V
    float result = dividerVoltage(2.1f, 220000.0f, 220000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 4.2f, result);
}

void test_divider_220k_solar_panel(void) {
    // 220k/220k divider: ADC sees half of panel voltage
    // If ADC reads 2.7V, actual panel is 5.4V
    float result = dividerVoltage(2.7f, 220000.0f, 220000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.4f, result);
}

// ─── Solar/battery reading format ────────────────────────

void test_append_reading_solar_voltage(void) {
    char buf[256];
    int len = appendReading(buf, sizeof(buf),
        "soil-1-solar", "solar",
        5.4f, "V", "Garden", 85, true, false);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"type\":\"solar\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"value\":5.4"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"unit\":\"V\""));
}

void test_append_reading_battery_voltage(void) {
    char buf[256];
    int len = appendReading(buf, sizeof(buf),
        "soil-1-battery", "battery",
        3.95f, "V", "Garden", 72, true, false);
    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"type\":\"battery\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"value\":4.0"));  // %.1f rounds 3.95 → 4.0
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"battery\":72"));
}

// ─── Adaptive sleep tiers ───────────────────────────

// Mirror the config defines for testing
#define SLEEP_TIER_FULL_MIN    5
#define SLEEP_TIER_HIGH_MIN   15
#define SLEEP_TIER_MED_MIN    30
#define SLEEP_TIER_LOW_MIN    60

static int getSleepMinutes(int batteryPercent) {
    if (batteryPercent >= 90) return SLEEP_TIER_FULL_MIN;
    if (batteryPercent >= 60) return SLEEP_TIER_HIGH_MIN;
    if (batteryPercent >= 30) return SLEEP_TIER_MED_MIN;
    return SLEEP_TIER_LOW_MIN;
}

void test_sleep_full_battery(void) {
    TEST_ASSERT_EQUAL(5, getSleepMinutes(100));
    TEST_ASSERT_EQUAL(5, getSleepMinutes(95));
    TEST_ASSERT_EQUAL(5, getSleepMinutes(90));
}

void test_sleep_high_battery(void) {
    TEST_ASSERT_EQUAL(15, getSleepMinutes(89));
    TEST_ASSERT_EQUAL(15, getSleepMinutes(60));
}

void test_sleep_medium_battery(void) {
    TEST_ASSERT_EQUAL(30, getSleepMinutes(59));
    TEST_ASSERT_EQUAL(30, getSleepMinutes(30));
}

void test_sleep_low_battery(void) {
    TEST_ASSERT_EQUAL(60, getSleepMinutes(29));
    TEST_ASSERT_EQUAL(60, getSleepMinutes(10));
    TEST_ASSERT_EQUAL(60, getSleepMinutes(0));
}

// ─── AA battery linear mapping (mirrors sensors.cpp AA path) ──

static int aaBatteryPercent(float voltage, float fullV, float emptyV) {
    float pct = (voltage - emptyV) / (fullV - emptyV) * 100.0f;
    if (pct > 100) return 100;
    if (pct < 0) return 0;
    return (int)pct;
}

void test_aa_battery_fresh(void) {
    TEST_ASSERT_EQUAL(100, aaBatteryPercent(1.5f, 1.5f, 0.9f));
}

void test_aa_battery_dead(void) {
    TEST_ASSERT_EQUAL(0, aaBatteryPercent(0.9f, 1.5f, 0.9f));
}

void test_aa_battery_mid(void) {
    // 1.2V = (1.2 - 0.9) / (1.5 - 0.9) = 0.3 / 0.6 = 50%
    TEST_ASSERT_EQUAL(50, aaBatteryPercent(1.2f, 1.5f, 0.9f));
}

void test_aa_battery_below_empty(void) {
    TEST_ASSERT_EQUAL(0, aaBatteryPercent(0.5f, 1.5f, 0.9f));
}

void test_aa_battery_above_full(void) {
    TEST_ASSERT_EQUAL(100, aaBatteryPercent(1.7f, 1.5f, 0.9f));
}

// ─── Median sort (mirrors medianAdcMillivolts logic) ─────

static float medianOfSorted(uint16_t* samples, int count) {
    // Insertion sort
    for (int i = 1; i < count; i++) {
        uint16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    return (samples[count/2 - 1] + samples[count/2]) / 2.0f;
}

void test_median_sorted_input(void) {
    uint16_t samples[] = {100,200,300,400,500,600,700,800,900,1000,1100,1200,1300,1400,1500,1600};
    float med = medianOfSorted(samples, 16);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 850.0f, med);  // (800+900)/2
}

void test_median_reversed_input(void) {
    uint16_t samples[] = {1600,1500,1400,1300,1200,1100,1000,900,800,700,600,500,400,300,200,100};
    float med = medianOfSorted(samples, 16);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 850.0f, med);
}

void test_median_with_outlier(void) {
    // One glitch sample at 0 should not affect median
    uint16_t samples[] = {0,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100,2100};
    float med = medianOfSorted(samples, 16);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2100.0f, med);
}

void test_median_identical_values(void) {
    uint16_t samples[] = {1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500,1500};
    float med = medianOfSorted(samples, 16);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1500.0f, med);
}

// ─── Multi-sample noise computation ─────────────────────

static void computeNoise(uint16_t* samples, int count,
                         uint16_t& median, uint16_t& lo, uint16_t& hi) {
    lo = samples[0];
    hi = samples[0];
    for (int i = 1; i < count; i++) {
        if (samples[i] < lo) lo = samples[i];
        if (samples[i] > hi) hi = samples[i];
    }
    for (int i = 1; i < count; i++) {
        uint16_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
    median = samples[count / 2];
}

void test_noise_stable_readings(void) {
    uint16_t samples[] = {500, 501, 500, 502, 500};
    uint16_t med, lo, hi;
    computeNoise(samples, 5, med, lo, hi);
    TEST_ASSERT_EQUAL(500, med);   // Middle of sorted [500,500,500,501,502]
    TEST_ASSERT_EQUAL(500, lo);
    TEST_ASSERT_EQUAL(502, hi);
}

void test_noise_noisy_readings(void) {
    uint16_t samples[] = {400, 600, 500, 700, 300};
    uint16_t med, lo, hi;
    computeNoise(samples, 5, med, lo, hi);
    TEST_ASSERT_EQUAL(500, med);   // Middle of sorted [300,400,500,600,700]
    TEST_ASSERT_EQUAL(300, lo);
    TEST_ASSERT_EQUAL(700, hi);
}

void test_noise_identical_readings(void) {
    uint16_t samples[] = {1000, 1000, 1000, 1000, 1000};
    uint16_t med, lo, hi;
    computeNoise(samples, 5, med, lo, hi);
    TEST_ASSERT_EQUAL(1000, med);
    TEST_ASSERT_EQUAL(1000, lo);
    TEST_ASSERT_EQUAL(1000, hi);
}

void test_noise_spread_calculation(void) {
    uint16_t samples[] = {250, 270, 260, 240, 255};
    uint16_t med, lo, hi;
    computeNoise(samples, 5, med, lo, hi);
    TEST_ASSERT_EQUAL(255, med);   // Middle of sorted [240,250,255,260,270]
    // Spread = max - min = 270 - 240 = 30
    TEST_ASSERT_EQUAL(30, hi - lo);
}

// ─── Runner ──────────────────────────────────────────────

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_c_to_f_freezing);
    RUN_TEST(test_c_to_f_boiling);
    RUN_TEST(test_c_to_f_body_temp);
    RUN_TEST(test_c_to_f_negative);

    RUN_TEST(test_append_reading_int_no_battery);
    RUN_TEST(test_append_reading_float_with_battery);
    RUN_TEST(test_append_reading_buffer_overflow);

    RUN_TEST(test_battery_full);
    RUN_TEST(test_battery_empty);
    RUN_TEST(test_battery_half);
    RUN_TEST(test_battery_over_full);
    RUN_TEST(test_battery_below_empty);
    RUN_TEST(test_battery_plateau);
    RUN_TEST(test_battery_low_end);

    RUN_TEST(test_divider_220k_half_battery);
    RUN_TEST(test_divider_220k_solar_panel);
    RUN_TEST(test_append_reading_solar_voltage);
    RUN_TEST(test_append_reading_battery_voltage);

    RUN_TEST(test_sleep_full_battery);
    RUN_TEST(test_sleep_high_battery);
    RUN_TEST(test_sleep_medium_battery);
    RUN_TEST(test_sleep_low_battery);

    RUN_TEST(test_aa_battery_fresh);
    RUN_TEST(test_aa_battery_dead);
    RUN_TEST(test_aa_battery_mid);
    RUN_TEST(test_aa_battery_below_empty);
    RUN_TEST(test_aa_battery_above_full);

    RUN_TEST(test_median_sorted_input);
    RUN_TEST(test_median_reversed_input);
    RUN_TEST(test_median_with_outlier);
    RUN_TEST(test_median_identical_values);

    RUN_TEST(test_noise_stable_readings);
    RUN_TEST(test_noise_noisy_readings);
    RUN_TEST(test_noise_identical_readings);
    RUN_TEST(test_noise_spread_calculation);

    return UNITY_END();
}
