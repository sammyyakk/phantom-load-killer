# Phantom Load Killer — Complete Parts List (Revised)

> **PCB Approach: Carrier Board**
> The ESP32 DevKit V1 plugs into female pin headers soldered on the PCB.
> No bare ESP32 module. No CP2102 programmer. No AMS1117 regulator.
> The DevKit's onboard regulator supplies 3.3V to the chip.
> HLK-PM01 (5V) feeds the DevKit VIN pin directly + relay coil.

## Quick Cost Summary

| Category | Cost (₹) |
|----------|----------|
| Core Processing | ₹700 |
| Sensing | ₹240 |
| Power Supply | ₹230 |
| Switching & Protection | ₹280 |
| Display & Indicators | ₹270 |
| Passives (R, C) | ₹150 (kit) |
| Connectors & Headers | ₹255 |
| PCB & Enclosure | ₹1,010 |
| Wiring & Misc | ₹510 |
| Test & Safety | ₹900 |
| **TOTAL** | **~₹3,500–4,000** |
| **Without test equipment** | **~₹2,500–3,000** |

---

## Category 1: Core Processing

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 1 | ESP32 DevKit V1 | 30-pin, CP2102 USB-UART onboard | 2 | ₹350 | ₹700 | Buy 2 — one spare, mains will kill one |

> Buy the **30-pin** DevKit V1 specifically. The 38-pin version has a different pinout and won't fit the same footprint.
> DevKit plugs into female headers on PCB. USB port accessible from PCB edge for reprogramming.

---

## Category 2: Sensing

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 2 | Current Sensor Module | ACS712-20A | 2 | ₹120 | ₹240 | Measures ±20A, covers up to 4400W. Buy 2. |
| 3 | AC Voltage Sensor Module | ZMPT101B | 1 | ₹150 | ₹150 | 80–260V AC, analog output |

> **MANDATORY — ACS712 voltage divider:**
> ACS712 on 5V Vcc → zero-current output = 2.5V, max output = 4.5V.
> ESP32 ADC max = 3.3V. Without the divider you WILL damage the ESP32.
> Divider: 10kΩ (top) + 20kΩ (bottom) on the VOUT pin. Scales 4.5V → 3.0V. ✓

---

## Category 3: Power Supply (Mains → DC)

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 4 | Isolated AC-DC Module | HLK-PM01 (5V/600mA) | 1 | ₹175 | ₹175 | Powers relay coil (5V) + DevKit VIN (5V) |
| 5 | Electrolytic Cap 100µF/16V | Generic radial | 3 | ₹5 | ₹15 | HLK-PM01 output filtering |
| 6 | Ceramic Cap 100nF | 0.1µF 50V | 10 | ₹2 | ₹20 | Decoupling near DevKit header pins |
| 7 | Electrolytic Cap 10µF/16V | Generic radial | 3 | ₹3 | ₹9 | Output filtering |

> **No AMS1117 needed.** The DevKit already has an AMS1117-3.3 onboard — it converts 5V (from HLK-PM01) to 3.3V for the ESP32 chip automatically.
> Power chain: 220V AC → HLK-PM01 → 5V → DevKit VIN pin → onboard AMS1117 → 3.3V to ESP32.
> The 5V rail also drives the relay coil.

---

## Category 4: Switching & Protection

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 8 | Relay (5V coil, 10A) | SRD-05VDC-SL-C | 2 | ₹35 | ₹70 | 250VAC/10A contacts. Buy spare. |
| 9 | Optocoupler | PC817 | 5 | ₹5 | ₹25 | ESP32 GPIO → relay circuit isolation, DIP-4 |
| 10 | NPN Transistor | BC547 or 2N2222 | 10 | ₹3 | ₹30 | Relay coil driver (3.3V GPIO can't drive relay directly) |
| 11 | Flyback Diode | 1N4007 | 10 | ₹2 | ₹20 | Across relay coil — MANDATORY, prevents voltage spike |
| 12 | Metal Oxide Varistor | S14K275 or 14D271K (275V) | 2 | ₹15 | ₹30 | Mains transient spike clamp across L-N |
| 13 | Glass Fuse 5A | 5×20mm, 5A, 250V | 5 | ₹8 | ₹40 | Inline on Live wire, hardware backup |
| 14 | PCB Fuse Holder | 5×20mm, PCB mount | 2 | ₹20 | ₹40 | Panel-accessible from enclosure |
| 15 | Schottky Diode | 1N5819 | 5 | ₹5 | ₹25 | Reverse polarity protection on 5V DC rail |

---

## Category 5: Display & Indicators

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 16 | OLED Display | SSD1306 0.96" I2C 128×64 | 1 | ₹200 | ₹200 | I2C, 4-pin (VCC GND SCL SDA), blue or white |
| 17 | RGB LED Addressable | WS2812B (5mm or module) | 3 | ₹15 | ₹45 | Status: Green=Active / Yellow=Standby / Red=Cut |
| 18 | Tactile Push Button | 6mm×6mm, 4-pin THT | 5 | ₹5 | ₹25 | BOOT + RST on PCB |

---

## Category 6: Passive Components

| # | Part | Value | Qty | Unit Price (₹) | Total (₹) | Purpose |
|---|------|-------|-----|----------------|-----------|---------|
| 19 | Resistor 10kΩ | 1/4W | 20 | ₹1 | ₹20 | Pull-ups, ACS712 voltage divider (top leg) |
| 20 | Resistor 20kΩ | 1/4W | 20 | ₹1 | ₹20 | ACS712 voltage divider (bottom leg) |
| 21 | Resistor 1kΩ | 1/4W | 20 | ₹1 | ₹20 | Optocoupler LED, transistor base resistor |
| 22 | Resistor 330Ω | 1/4W | 10 | ₹1 | ₹10 | WS2812B data line protection |
| 23 | Resistor 4.7kΩ | 1/4W | 10 | ₹1 | ₹10 | I2C pull-ups for OLED SDA/SCL |
| 24 | Resistor 100Ω | 1/4W | 10 | ₹1 | ₹10 | General purpose |

> **Tip**: Buy a 600-piece resistor assortment kit (~₹150) — covers all values above and gives spares.

---

## Category 7: Connectors & Headers

| # | Part | Model | Qty | Unit Price (₹) | Total (₹) | Notes |
|---|------|-------|-----|----------------|-----------|-------|
| 25 | **Female Pin Headers 2.54mm** | 40-pin strip | 4 | ₹20 | ₹80 | Cut to 15-pin for DevKit. 2 strips per board side. |
| 26 | Male Pin Headers 2.54mm | 40-pin strip | 3 | ₹15 | ₹45 | OLED connector, debug breakout |
| 27 | Screw Terminal 2-pin (5.08mm) | KF301-2P | 4 | ₹15 | ₹60 | AC Live, Neutral, Load out |
| 28 | Screw Terminal 3-pin (5.08mm) | KF301-3P | 2 | ₹20 | ₹40 | Combined AC in/out |
| 29 | JST 2-pin connectors | JST-PH 2.0 | 5 pairs | ₹10 | ₹50 | Detachable sensor connections |

> **Female header sizing for 30-pin DevKit V1:**
> Need 2× 15-pin female headers per board (one per side of DevKit).
> Buy 4× 40-pin strips, cut with a knife/side-cutter. Costs ~₹80 total.
> No micro-USB socket needed on PCB — DevKit's own USB port handles programming.

---

## Category 8: PCB & Enclosure

| # | Part | Spec | Qty | Price (₹) | Notes |
|---|------|------|-----|-----------|-------|
| 30 | Custom PCB fabrication | 100×80mm, 2-layer, FR4 1.6mm | 5 pcs | ₹600 | JLCPCB (~$7 + ~₹300 shipping, 7–10 days) |
| 31 | Prototype PCB / Perfboard | 7×9cm double-sided | 3 | ₹50 | ₹150 for breadboard/perfboard phase before PCB |
| 32 | Project Enclosure | 115×90×55mm ABS | 1 | ₹200 | Cut notch for DevKit USB port access |
| 33 | IEC C14 Power Inlet Socket | Panel mount, 10A | 1 | ₹60 | Clean mains input — professional look |
| 34 | Indian 3-pin Socket (load out) | Panel mount, ISI 6A | 1 | ₹50 | Device plugs into this socket |

---

## Category 9: Wiring & Miscellaneous

| # | Part | Spec | Qty | Price (₹) | Notes |
|---|------|------|-----|-----------|-------|
| 35 | Mains-rated wire | 1.5mm² flexible, red + black + green | 1m each | ₹40/m | HV section ONLY — don't use thin hookup wire |
| 36 | Jumper wires | M-M, M-F, 20cm, 40 pcs each | 2 sets | ₹80 | Breadboard/perfboard prototype stage |
| 37 | Heat shrink tubing | 3mm and 6mm assorted | 1 pack | ₹80 | All HV solder joints — mandatory |
| 38 | Kapton tape | High-temp polyimide insulation | 1 roll | ₹80 | HV/LV isolation layer on PCB |
| 39 | Solder | 60/40 rosin core, 0.8mm | 100g | ₹150 | |
| 40 | Flux pen | Rosin no-clean flux | 1 | ₹80 | Cleaner soldering on small pads |

---

## Category 10: Test & Safety Equipment

| # | Part | Spec | Qty | Price (₹) | Notes |
|---|------|------|-----|-----------|-------|
| 41 | Multimeter | Basic AC/DC voltage + continuity | 1 | ₹350 | Test HV before first power-on |
| 42 | GFCI / RCD Extension Cord | 6A or 10A rated | 1 | ₹400 | **Use this for ALL mains tests — non-negotiable** |
| 43 | Insulated screwdrivers | 1000V rated set | 1 | ₹150 | Any mains work |
| 44 | USB Power Bank | 5V, any capacity | 1 | (you have one) | Power DevKit safely during firmware dev, no mains needed |

---

## Where to Buy (India)

| Store | Best For | URL |
|-------|----------|-----|
| **Robu.in** | ESP32, ACS712, ZMPT101B, relays, WS2812B | robu.in |
| **ElectronicsComp** | ICs, passives, connectors, HLK-PM01 | electronicscomp.com |
| **Probots.co.in** | Sensors, modules | probots.co.in |
| **Amazon India** | Enclosures, tools, mains wire (fast delivery) | amazon.in |
| **JLCPCB** | Custom PCB (cheapest, ~$7 for 5 pcs) | jlcpcb.com |
| **Local electronics market** | Resistors, caps, fuses, wire — 50% cheaper in bulk | SP Road (Bangalore), Lamington Rd (Mumbai), Nehru Place (Delhi) |

---

## Critical Safety Rules

1. **Never work on mains live** — isolate from supply, discharge caps, then work
2. **HLK-PM01 output terminals are mains-adjacent** — treat as dangerous until fully verified
3. **Fuse goes on Live (L) wire only** — never on Neutral
4. **Use GFCI extension cord for every single mains power-on test**
5. **6mm minimum clearance** between HV and LV copper on PCB — measure on KiCad
6. **Relay switches Live wire only** — Neutral connects straight through to load
7. **Conformal coat the finished PCB**, especially the HV zone, before putting in enclosure
8. **ACS712 voltage divider is not optional** — 4.5V into ESP32 ADC = dead ESP32

---

## What Changed from Previous Version

| Removed | Why |
|---------|-----|
| ESP32-WROOM-32 bare module | DevKit used as carrier board — module not needed |
| CP2102 USB-UART programmer | DevKit has USB-UART onboard |
| AMS1117-3.3 regulator | DevKit has it onboard already |
| Micro-USB PCB socket | DevKit's own USB port is used |

| Added | Why |
|-------|-----|
| Female pin headers (4 strips) | DevKit plugs into these on PCB |
| Extra ACS712 (qty 2 total) | One likely casualty during mains testing |
| Extra DevKit (qty 2 total) | Same reason |
