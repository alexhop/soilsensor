# TODO

## Production Build (Catnip + LiPo + Solar)

- [ ] Field-test the MOSFET-gated Catnip path on the assembly guide hardware
      and record deep sleep current
- [ ] Verify battery readings with the 1 MOhm divider (high source impedance
      for the ESP32 ADC; may need a capacitor at the midpoint)
- [ ] Add a low-voltage cutoff so a drained LiPo is not cycled further
- [ ] Update the guide's first power-on step to use the `scanner` environment
      and this firmware

## Known Issues

- [ ] Nodes sometimes wake twice per cycle; root cause unknown

## Other Probes

These paths are kept but are not the focus.

- [ ] Analog probe occasionally reports a phantom fully-wet reading after
      wake, even with the 300 ms settle time and discarded first read
- [ ] AA-powered nodes become unstable below about 0.6 V per cell and start
      waking every few minutes
- [ ] `config.h` says setting `BATTERY_R1` and `BATTERY_R2` to 1.0 reads the
      battery directly, but the divider formula then multiplies by 2
- [ ] A solar test rig using a 220k/220k panel-sense divider slowly drained its
      battery overnight through the divider; fit a series Schottky diode on the
      panel input when using solar sense

## Housekeeping

- [ ] `test_logic.cpp` re-declares the functions it tests; share the real
      implementations with the firmware instead
