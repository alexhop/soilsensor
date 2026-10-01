# Soil Sensor — Assembly Guide

A step-by-step build guide for the battery + solar soil moisture node. Written so a builder with limited soldering experience can complete one successfully. Plan on **2–3 hours** for your first one.

---

## What you're building

A small, self-contained, battery-powered sensor node. A solar panel keeps the battery topped off. The buried Catnip sensor reports soil moisture over I²C. The ESP32-C6 wakes up periodically, powers the sensor on through a MOSFET switch, takes a reading, reports its own battery voltage, and goes back to sleep.

Everything except the solar panel and the buried probe lives inside a small weatherproof enclosure.

---

## Bill of materials

### Electronics

| Qty | Part | Notes |
|-----|------|-------|
| 1 | Seeed XIAO ESP32-C6 | The main microcontroller |
| 1 | CN3065 solar lithium charger module | Manages panel → battery charging |
| 1 | Catnip Electronics I²C soil moisture sensor | The probe that goes in the dirt |
| 1 | 2N7000 N-channel MOSFET (TO-92) | Power switch for the sensor |
| 1 | 1200 mAh single-cell LiPo with JST-PH 2.0 connector | Battery |
| 1 | Voltaic 6V solar panel | With bare leads or matching connector |
| 2 | 1 MΩ resistor (¼ W) | Battery voltage divider |
| 1 | 100 kΩ resistor (¼ W) | MOSFET gate pulldown |

### Wire and connectors

| Qty | Item | Notes |
|-----|------|-------|
| ~3 ft | 26–28 AWG hookup wire | Stranded, multiple colors if possible |
| 1 | JST-PH 2.0 socket pigtail | For the Catnip cable to plug into |
| 1 | ~20 mm dia adhesive-lined heat shrink, 4" piece | Waterproofs the sensor's exposed PCB |
| Assorted | Smaller heat shrink (3/32", 1/8") | For individual joints |

### Mechanical

| Qty | Item | Notes |
|-----|------|-------|
| 1 | IP65 ABS project box (~100×68×50 mm minimum) | Or a 3D-printed PETG enclosure |
| 2 | PG-7 cable glands | One for the sensor cable, one for the panel cable |
| 1 | Small tube of clear silicone | For sealing cable exits |

### Tools

- Soldering iron with a fine tip (a temperature-controlled iron at ~325–350 °C / 620–660 °F is much easier than a fixed-temp one)
- 60/40 leaded solder, 0.6 mm or 0.8 mm (easier for beginners than lead-free)
- Solder sucker or desoldering braid (for fixing mistakes)
- Wire strippers (with a 28 AWG notch)
- Flush cutters
- Tweezers
- Heat gun or hair dryer for heat shrink
- Multimeter (for the continuity check at the end — **non-negotiable**)
- Helping-hands or PCB holder (a vise or even a lump of mounting putty works)
- Magnifying glass or loupe (helpful for inspecting joints)

### Spares to keep on hand

- 1 extra LiPo battery
- 1 extra Catnip sensor
- 1 extra CN3065 module

These are the parts most likely to fail in service. Total spare cost is around $20.

---

## Before you start: soldering for beginners

If you've never soldered or it's been years, do this first:

1. **Tin the tip.** Heat the iron, then melt a thin coat of solder onto the tip and wipe it on a damp sponge or brass wool. The tip should look shiny silver, not black or dull. Re-tin every few minutes.

2. **Heat the joint, not the solder.** Touch the iron to *both* the wire and the pad at the same time, hold for 1–2 seconds, then touch solder to the **joint** (not the iron). Solder should flow and wick. Pull solder away first, then the iron.

3. **A good joint is shiny and concave.** Looks like a tiny volcano with smooth sides. A bad joint looks like a frosty ball sitting on top of the pad — that's a "cold joint" and won't conduct reliably.

4. **Practice on scrap first.** Spend 10 minutes joining bits of wire end-to-end before touching the real boards. Seriously.

5. **Strip wires short.** About 3 mm (1/8"). Twist the strands tight, then "tin" them by melting a thin coat of solder onto the bare end before joining. Tinned wires are dramatically easier to work with.

---

## Step 1 — Identify every pin before you wire anything

This is the part that trips up beginners. Sit down with each component and identify the pins below. Use a Sharpie to label the back of the boards if it helps.

### XIAO ESP32-C6

Hold the board with the **USB-C connector at the top** and the **component side facing you**. Pins run down both edges.

**Left edge, top to bottom:**

| Label | What we use it for |
|-------|--------------------|
| D0 | Battery voltage sense (analog input) |
| D1 | MOSFET gate — turns the sensor on and off |
| D2 | (unused) |
| D3 | (unused) |
| D4 | I²C SDA (data line to sensor) |
| D5 | I²C SCL (clock line to sensor) |
| D6 | (unused) |

**Right edge, top to bottom:**

| Label | What we use it for |
|-------|--------------------|
| 5V | (unused — do not connect) |
| GND | Common ground |
| 3V3 | 3.3 V regulated output (powers the sensor) |
| D10–D7 | (unused) |

**Flip the board over.** On the back are two solder pads labeled **BAT+** and **BAT−**. These connect to the on-board battery rail and charge controller. The battery wires go to these pads.

### CN3065 solar charger

Two pairs of through-hole pads, silkscreened on the board:

- **IN+ / IN−** — solar panel input (accepts ~4.4–6 V)
- **BAT+ / BAT−** — single-cell LiPo battery

There is no separate "load" output — the BAT pads also feed downstream electronics.

### 2N7000 MOSFET

Small black half-cylinder with a flat face and three leads (TO-92 package). **Hold it with the flat face toward you and the leads pointing down.** From left to right:

1. **Source (S)** — left lead
2. **Gate (G)** — middle lead
3. **Drain (D)** — right lead

Memorize this. If you flip the part backward, the circuit won't work and you can damage the MOSFET.

### Catnip soil moisture sensor

Four pads labeled on the board:

- **VCC** — 3.3 V supply
- **GND** — ground (this goes through the MOSFET, not directly to system ground)
- **SDA** — I²C data
- **SCL** — I²C clock

The sensor ships with a 4-wire JST-PH cable. Wire colors vary by batch — **always confirm with a multimeter against the labeled pads** rather than trusting colors. Common is red=VCC, black=GND, but the data lines vary.

### Battery and panel

- LiPo: red wire = positive (+), black wire = negative (−). The JST-PH connector is keyed.
- Solar panel: red = positive, black = negative.

---

## Step 2 — The wiring plan, all in one place

Build to this map. Don't skip ahead — there's a continuity check after assembly that catches almost every mistake.

```
                   ┌──── Solar panel ────┐
                   │  (+)            (−) │
                   ▼                     ▼
              CN3065 IN+            CN3065 IN−
              CN3065 BAT+ ──┬── XIAO BAT+ pad
              CN3065 BAT− ──┼── XIAO BAT− pad
                            │
                  Battery (+)│      Battery (−) — same node as BAT−
                            │
                   1 MΩ resistor (R1)
                            │
                            ●──── XIAO D0 (battery sense)
                            │
                   1 MΩ resistor (R2)
                            │
                       XIAO GND

              XIAO 3V3 ───────────► Catnip VCC
              XIAO D4  ───────────► Catnip SDA
              XIAO D5  ───────────► Catnip SCL
              Catnip GND ──► 2N7000 Drain (right lead)
              2N7000 Source (left)  ──► XIAO GND
              2N7000 Gate (middle) ──► XIAO D1
              100 kΩ between Gate and Source
```

---

## Step 3 — Solder the CN3065 to the XIAO

The CN3065 and XIAO share a common battery rail — they're wired in parallel. The cleanest way to do this is to mount them side by side and bridge them with short jumpers.

1. Place both boards component-side up on your work surface, CN3065 to the **left** of the XIAO, with the XIAO oriented USB-up.
2. Cut two pieces of hookup wire about **20 mm long**. Strip 3 mm off each end and tin the bare strands.
3. Solder one wire from CN3065 **BAT+** to XIAO **BAT+** (back-side pad).
4. Solder the second wire from CN3065 **BAT−** to XIAO **BAT−** (back-side pad).

> The XIAO BAT pads are small. If you struggle with them: pre-tin the pad first by melting a tiny dome of solder onto it, then lay the tinned wire end on top and re-flow with the iron. This avoids fighting solder + wire + heat all at once.

5. Visually inspect — no solder bridges between BAT+ and BAT−. A magnifier helps.

---

## Step 4 — Connect the battery (DO NOT plug it in yet)

If your LiPo has a JST-PH connector and you'd rather solder the battery permanently, snip the connector off **leaving plenty of wire on the battery side**. Otherwise, keep the connector and add a matching socket to the CN3065 BAT pads (lets you swap batteries without desoldering).

For a permanent solder connection:

1. Confirm with a multimeter which battery wire is **+**. Set meter to DC volts, touch red probe to red wire, black to black — should read ~3.7–4.2 V positive. If negative, your colors are reversed (rare but possible). Label them.
2. Cut and strip the battery leads. Leave **at least 50 mm** of wire so the battery can sit comfortably in the enclosure.
3. **Important:** Tin the bare battery wire ends *before* anything else. To do that without short-circuiting:
   - Strip and tin **one wire at a time**.
   - Keep the un-stripped wire well away from anything metal.
   - Never let bare positive and bare negative touch each other or any tool. A LiPo dead-short can vent or catch fire.
4. Solder the **negative (black) wire first** to CN3065 BAT−. Working ground-first means even if you slip, nothing energizes anything yet.
5. Solder the **positive (red) wire** to CN3065 BAT+.
6. Slide a piece of small heat shrink over each joint and shrink it.

Set the assembly aside, battery dangling.

---

## Step 5 — The voltage divider (battery monitoring)

The XIAO's analog input maxes out at 3.3 V, but a fully charged LiPo is 4.2 V. The two 1 MΩ resistors halve the voltage so the ADC sees a safe range, while drawing only ~2 µA.

1. Cut the leads of both 1 MΩ resistors to about **8 mm** each.
2. Twist one lead of **R1** together with one lead of **R2** to form a junction. Solder them together. This junction is the divider's output.
3. Solder a 30 mm hookup wire to that junction. Heat shrink it.
4. Solder the **free lead of R1** to the CN3065 **BAT+** pad (you can reuse the BAT+ node — it's the same as battery positive).
5. Solder the **free lead of R2** to XIAO **GND**.
6. Solder the wire from the junction to XIAO **D0**.

That's it for the divider. It's now reading half the battery voltage continuously.

---

## Step 6 — The MOSFET switch (sensor power gating)

This is the trickiest section. Take it slow.

1. **Bend the MOSFET leads to fit.** With flat face toward you, bend the Source (left) lead down and out so it can reach the XIAO GND. Leave the middle (Gate) and right (Drain) leads roughly straight.
2. **Pre-tin all three MOSFET leads.** Hold each lead briefly with tweezers (they conduct heat — don't burn your fingers) and apply a thin coat of solder to each.
3. Solder the **Source (left lead)** to XIAO **GND** with a short jumper wire.
4. Solder the **100 kΩ resistor** between **Gate (middle lead)** and **Source (left lead)**. This pulldown keeps the MOSFET firmly OFF when the GPIO isn't actively driving it (important during boot and deep sleep, when the pin is floating).
5. Solder a hookup wire from **Gate (middle lead)** to XIAO **D1**.
6. Solder a hookup wire from **Drain (right lead)** to a small junction point that will eventually meet the Catnip GND wire (Step 7). Leave this wire ~50 mm long.

**Sanity check before continuing:** with a multimeter on continuity / beep mode, verify:

- Beep between MOSFET Source and XIAO GND ✓
- Beep between MOSFET Gate and XIAO D1 ✓
- **No** beep between MOSFET Drain and XIAO GND ✓ (it's only connected when the firmware turns the MOSFET on)
- **No** beep between any MOSFET lead and XIAO 3V3 ✓

---

## Step 7 — The sensor cable

The Catnip cable is your only field-serviceable connection. Don't solder it directly — use a JST-PH socket pigtail so you can unplug and replace the sensor when it eventually corrodes (it's the part most likely to fail).

1. Take a JST-PH 2.0 socket pigtail (the kind with bare wire ends and a female socket). Identify which wire goes to which pin of the socket — they should be color-coded, but **verify with a multimeter** in continuity mode by probing pin-to-wire.
2. Note the Catnip's pad labels and which color of its cable goes where. Confirm by plugging in the sensor cable and ringing out continuity from the Catnip pad to each socket pin.
3. Solder the Catnip cable connections **at the XIAO end of the pigtail wires:**
   - **VCC** wire → XIAO **3V3**
   - **SDA** wire → XIAO **D4**
   - **SCL** wire → XIAO **D5**
   - **GND** wire → MOSFET **Drain** (the wire you left dangling in Step 6)
4. Heat shrink each individual joint.

> **Why GND goes through the MOSFET, not straight to ground:** when the MOSFET is off, the sensor has no path to ground and draws zero current. When firmware drives D1 high, the MOSFET turns on and completes the ground path. This is called **low-side switching**. (You could switch the VCC line instead with a P-channel MOSFET, but low-side with an N-channel is simpler and the Catnip doesn't care.)

---

## Step 8 — The solar panel cable

1. Strip the panel's red and black leads. Tin them.
2. Solder red to CN3065 **IN+** and black to CN3065 **IN−**. Heat shrink each joint.
3. Don't expose the panel to light yet (cover it with a cloth or face it down) — you don't want it pushing current into a half-built circuit.

---

## Step 9 — The pre-power continuity check

**Do this before plugging in the battery.** A multimeter and 5 minutes here saves a fried board.

Set your multimeter to **continuity / beep mode** and verify:

| Should beep | Should NOT beep |
|-------------|------------------|
| CN3065 BAT+ to XIAO BAT+ pad | CN3065 BAT+ to CN3065 BAT− |
| CN3065 BAT− to XIAO BAT− pad | XIAO 3V3 to XIAO GND |
| XIAO GND to MOSFET Source (left) | CN3065 IN+ to CN3065 IN− |
| XIAO D1 to MOSFET Gate (middle) | XIAO BAT+ to XIAO GND |
| XIAO D0 to divider junction | MOSFET Drain to XIAO GND (sensor unplugged) |
| Catnip VCC pin to XIAO 3V3 | XIAO 3V3 to XIAO BAT+ |
| Catnip SDA pin to XIAO D4 | |
| Catnip SCL pin to XIAO D5 | |

If all of those check out, you're clear to power up.

---

## Step 10 — First power-on

1. Plug the battery in (or it's already soldered in).
2. The XIAO's onboard LED should blink briefly during boot.
3. Plug the XIAO into your laptop with USB-C. Open the Arduino IDE or ESPHome config and verify the board is detected.
4. Flash a basic I²C scanner sketch. With the firmware driving D1 high, you should see the Catnip at I²C address **0x20** (default).
5. Read D0 with `analogRead()`. Multiply by 2 and your ADC calibration to get battery voltage.
6. Pull D1 low. The Catnip should disappear from the I²C bus — that's the MOSFET cutting power.

If any of that fails, see **Troubleshooting** below.

---

## Step 11 — Waterproofing the sensor probe

The Catnip's top half (chip, passives, connector) is exposed and will corrode in soil. The bottom half (the capacitive blade) is meant to be wet.

1. Slide a piece of **20 mm adhesive-lined heat shrink** over the top half of the sensor, covering everything from just above the connector down to the boundary line on the PCB (where the silkscreen ends and the bare blade begins).
2. Heat with a heat gun, working slowly and evenly. The adhesive lining will melt and seal around the cable and the PCB edges.
3. Add a small bead of clear silicone where the cable exits the heat shrink — water loves to wick down cables, and that's the #1 failure point.

The sensor is now ready to bury. The blade goes in the soil; the heat-shrunk top can sit at or just below the surface.

---

## Step 12 — Mounting in the enclosure

1. Drill two holes in the enclosure for PG-7 cable glands — one for the sensor cable, one for the solar panel cable. Side or bottom is fine; **don't put glands on the top** (rain pools there).
2. Install the cable glands. They thread in from the outside and lock with a nut on the inside. The rubber compression seal grips the cable when you tighten the cap.
3. Pass the sensor cable and solar cable through their respective glands. Tighten the gland caps until you can't pull the cable out by hand.
4. Mount the XIAO/CN3065 board to the enclosure. Double-sided foam tape works fine; for something more permanent, drill mounting holes and use M2 standoffs.
5. Place the battery in any free space — it doesn't need to be secured tightly, but it shouldn't rattle around. A piece of foam helps.
6. Add a small silica gel desiccant pack inside the enclosure before closing it up. Replace it every couple of years.
7. Close the enclosure. Verify the gasket seats cleanly all the way around.

---

## Troubleshooting

**Nothing turns on / no LED.**
Check battery polarity at the CN3065 BAT pads with a multimeter. Should read 3.7–4.2 V with red probe on BAT+. If it's reversed, the XIAO is probably damaged. If it's correct but still nothing, check continuity from the battery to the XIAO BAT pads.

**XIAO boots but the Catnip doesn't show on I²C.**
First, drive D1 high in firmware. If it's still missing, check Catnip VCC pin actually reads 3.3 V (probe with multimeter). If VCC is good, swap SDA and SCL — easy mistake. Also confirm the MOSFET orientation: flat face toward you, leads down, S/G/D left to right.

**Catnip is always on, even with D1 low.**
The MOSFET is wired backward (Source and Drain swapped) or the 100 kΩ pulldown is missing. The MOSFET is symmetric-ish but not identical — get the orientation right.

**ADC reads zero or maxed out for battery voltage.**
Voltage divider problem. Check that R1 goes from BAT+ to junction, R2 goes from junction to GND, and the junction wire goes to D0. A common error is putting both resistors in series to GND with the junction tap on the wrong side.

**Battery drains overnight.**
Almost always the sensor staying powered. Confirm D1 actually goes low in your sleep code. Probe the Catnip VCC pin while in deep sleep — should read 0 V. If it reads 3.3 V, the MOSFET isn't switching off. Check the gate pulldown and confirm the firmware sets D1 LOW (not just floats it) before sleeping.

**Solar panel doesn't seem to charge.**
With the panel in direct sun, measure CN3065 IN+ to IN− — should read 5–6 V. Then measure CN3065 BAT+ to BAT− — should read slightly higher than the battery's resting voltage when charging. If IN reads good but BAT doesn't change, the CN3065 may be defective; swap it.

---

## Quick pin reference card

Tape this inside the enclosure or to the lid for future you.

| XIAO pin | Connects to |
|----------|-------------|
| 3V3 | Catnip VCC |
| GND | MOSFET Source, R2 (low side of divider) |
| BAT+ pad | CN3065 BAT+, battery + |
| BAT− pad | CN3065 BAT−, battery − |
| D0 | Voltage divider junction |
| D1 | MOSFET Gate (sensor power on/off) |
| D4 | Catnip SDA |
| D5 | Catnip SCL |
