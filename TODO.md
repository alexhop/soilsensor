# TODO

## Firmware for the Assembly Guide Build

The assembly guide describes a Catnip + LiPo + solar node with the MOSFET gate
on D1 and a 1 MOhm battery divider on D0. No environment supports it yet.

- [ ] Drive `SENSOR_POWER_PIN` in the `CATNIP_SOIL` path of `sensorsInit()` and
      release it in `sensorsPowerOff()`
- [ ] Add a `soil-catnip` environment: `CATNIP_SOIL=true`, `SCD41_ENABLED=false`,
      `SOLAR_ENABLED=false`, `SENSOR_POWER_PIN=D1`
- [ ] Verify battery readings with the 1 MOhm divider (high source impedance
      for the ESP32 ADC; may need a capacitor at the midpoint)
- [ ] Field-test on the guide hardware and record deep sleep current
- [ ] Update the guide's first power-on step to use the `scanner` environment

## Known Issues

- [ ] Analog probe (`soil-2`) occasionally reports a phantom fully-wet reading
      after wake, even with the 300 ms settle time and discarded first read
- [ ] Nodes sometimes wake twice per cycle; root cause unknown
- [ ] AA-powered nodes become unstable below about 0.6 V per cell and start
      waking every few minutes; add a low-voltage cutoff
- [ ] `config.h` says setting `BATTERY_R1` and `BATTERY_R2` to 1.0 reads the
      battery directly, but the divider formula then multiplies by 2; confirm
      which is intended for the `soil-2` board
- [ ] A solar test rig using the same 220k/220k panel-sense divider slowly
      drained its battery overnight through the divider; fit a series Schottky
      diode on the panel input when using solar sense

## Housekeeping

- [ ] Choose a license
- [ ] `test_logic.cpp` re-declares the functions it tests; share the real
      implementations with the firmware instead
