# Phantom Load Killer — Full Project Documentation

**Version:** Live document, updated as project progresses
**Last updated:** April 17, 2026
**Status:** Phase 2 complete (detection logic), pending relay integration

---

## Table of Contents

1. [Project Overview](#1-project-overview)
2. [Problem Statement](#2-problem-statement)
3. [System Architecture](#3-system-architecture)
4. [Hardware](#4-hardware)
5. [Circuit Design](#5-circuit-design)
6. [Firmware](#6-firmware)
7. [Detection Strategy](#7-detection-strategy)
8. [Calibration Data](#8-calibration-data)
9. [Known Limitations](#9-known-limitations)
10. [Build Phases & Status](#10-build-phases--status)
11. [Toolchain & Development Environment](#11-toolchain--development-environment)
12. [Future Upgrades](#12-future-upgrades)
13. [Real-World Data](#13-real-world-data)

---

## 1. Project Overview

The **Phantom Load Killer** is an ESP32-based smart power switch designed to automatically cut mains power to any device that has been idle for a configurable period. It targets the widespread problem of phantom loads — the continuous background power draw of devices left in standby indefinitely.

The device sits inline between a mains wall socket and the target appliance. It monitors AC current and voltage in real time, classifies the device's operational state, and drives a relay to cut power when the device goes idle for too long.

### Core Features (final design)
- Real-time AC current and voltage measurement
- Automatic power cutoff after configurable idle timeout
- Manual restore via tactile button
- Live OLED display: Voltage, Current, Power, State, Countdown
- RGB LED status indicators
- WiFi web dashboard with energy savings log
- OTA firmware updates

---

## 2. Problem Statement

### Phantom Load / Standby Power

Any device that is "off" but remains plugged in still draws power. This is called a phantom load, idle current, standby power, or vampire power. Sources include:

- Internal power supplies maintaining DC rails
- Microcontrollers waiting for remote control signals
- LED indicator lamps and clocks
- Network interfaces waiting for wake-on-LAN packets
- Capacitors charging/discharging in switching supplies

### Scale of the Problem (Indian Context)

| Device | Standby Power | Annual cost at ₹8/kWh |
|---|---|---|
| LED TV 40" | 0.5–5W | ₹35–350 |
| Set-top box | 8–15W | ₹560–1050 |
| Microwave (clock) | 2–5W | ₹140–350 |
| Washing machine | 1–3W | ₹70–210 |
| Laptop charger (idle) | 2–5W | ₹140–350 |
| Phone charger (idle) | 0.1–2W | ₹7–140 |
| **5 devices typical home** | **~20–40W** | **₹1,400–2,800/year** |

A single household wastes ₹1,400–2,800 per year purely from standby loads. At scale, phantom loads account for approximately 5–10% of total residential electricity consumption in India.

### The Gap in Existing Solutions

- **Smart plugs (Tp-Link, Wipro)**: Require manual scheduling or phone control. Do not auto-detect idle state.
- **Power strips with switches**: Require manual user action — the behavior we're trying to eliminate.
- **Monitoring-only devices (Kill-A-Watt)**: Show consumption but take no action.

The Phantom Load Killer automates the detection and cutoff — no user action required after initial setup.

---

## 3. System Architecture

```
                    ┌─────────────────────────────────────────┐
                    │              ESP32 DevKit V1             │
                    │                                          │
  ACS712 ──div──►  │ GPIO34 (ADC)   GPIO22 (SCL) ──► OLED    │
  ZMPT101B ──────► │ GPIO35 (ADC)   GPIO21 (SDA) ──► OLED    │
  Button ─────────► │ GPIO32         GPIO4  ──────► WS2812B  │
                    │ GPIO33 ──► Optocoupler ──► BC547 ──► Relay │
                    │                                          │
                    │ USB (micro) ◄──── 5V USB adapter        │
                    │ VIN (5V out) ──────────────────────────► Sensors, Relay coil │
                    └─────────────────────────────────────────┘

  Mains input ──► [Fuse] ──► [MOV] ──► ACS712 ──► Relay (COM/NO) ──► Load socket
  Neutral ──────────────────────────────────────────────────────────► Load socket
  Live ──────────────────────────────────────────────────────────────► ZMPT101B L
  Neutral ─────────────────────────────────────────────────────────► ZMPT101B N
```

### Power Supply Chain

```
220V AC ──► 5V USB Adapter ──► ESP32 micro-USB
                                    │
                              ESP32 onboard AMS1117
                                    │
                              3.3V to ESP32 chip, OLED
                                    │
                        ESP32 VIN pin (5V out)
                                    │
                  ┌─────────────────┼─────────────────┐
                  │                 │                  │
              ACS712 VCC       ZMPT101B VCC      Relay coil
              (5V, 10mA)       (5V, 20mA)       (5V, 70mA)
```

No HLK isolated AC-DC module used in prototype — 5V USB adapter powers everything through ESP32's onboard regulator and VIN pin.

---

## 4. Hardware

### Bill of Materials (Prototype)

| # | Component | Model | Qty | Cost (₹) | Notes |
|---|---|---|---|---|---|
| 1 | Microcontroller | ESP32 DevKit V1, 30-pin, CP2102 | 2 | ₹700 | Buy 2, one spare |
| 2 | Current sensor | ACS712-20A module | 2 | ₹240 | Both as spare |
| 3 | Voltage sensor | ZMPT101B module | 1 | ₹150 | |
| 4 | Relay | SRD-05VDC-SL-C, 10A/250VAC | 2 | ₹70 | 5V coil |
| 5 | Optocoupler | PC817 | 5 | ₹25 | GPIO → relay isolation |
| 6 | NPN transistor | BC547 | 10 | ₹30 | Relay coil driver |
| 7 | Flyback diode | 1N4007 | 10 | ₹20 | Across relay coil — mandatory |
| 8 | Schottky diode | 1N5819 | 5 | ₹25 | 5V rail reverse polarity |
| 9 | MOV | S14K275 (275V) | 2 | ₹30 | Mains transient clamp |
| 10 | Fuse 5A | 5×20mm, 250V | 5 | ₹40 | Live wire inline |
| 11 | Fuse holder | PCB mount, 5×20mm | 2 | ₹40 | |
| 12 | OLED display | SSD1306, 0.96", I2C, 128×64 | 1 | ₹200 | |
| 13 | RGB LEDs | WS2812B ×3 | 3 | ₹45 | Status indicators |
| 14 | Push button | 6mm×6mm tactile, 4-pin THT | 5 | ₹25 | BOOT, RST, manual restore |
| 15 | Resistor 10kΩ | 1/4W | 20 | ₹20 | Divider top leg, pull-ups |
| 16 | Resistor 22kΩ | 1/4W | 20 | ₹20 | Divider bottom leg (spec: 20kΩ, 22kΩ used) |
| 17 | Resistor 1kΩ | 1/4W | 20 | ₹20 | Optocoupler LED, transistor base |
| 18 | Resistor 330Ω | 1/4W | 10 | ₹10 | WS2812B data line protection |
| 19 | Resistor 4.7kΩ | 1/4W | 10 | ₹10 | I2C pull-ups |
| 20 | Cap 100µF/16V | Electrolytic | 3 | ₹15 | Power filtering |
| 21 | Cap 100nF | Ceramic | 10 | ₹20 | Decoupling |
| 22 | Female headers | 2.54mm, 40-pin strips | 4 | ₹80 | DevKit socket |
| 23 | Screw terminals | KF301-2P, KF301-3P | 6 | ₹100 | AC connections |
| 24 | Mains wire | 1.5mm² flexible, L+N+E | 3m | ₹120 | HV wiring |
| 25 | Heat shrink | 3mm + 6mm assorted | 1 pack | ₹80 | HV joints |
| 26 | PCB / perfboard | 7×9cm double-sided | 3 | ₹150 | Prototype phase |

**Power supply change from original design:** HLK-PM01 / HLK-5M05 isolated AC-DC modules removed from prototype. 5V USB adapter used instead, powering ESP32 via micro-USB. Eliminates mains-to-DC conversion complexity during development.

### Key Pin Assignments

| Function | GPIO | ADC Channel | Notes |
|---|---|---|---|
| ACS712 VOUT (divided) | 34 | ADC1_CH6 | Input-only pin |
| ZMPT101B VOUT | 35 | ADC1_CH7 | Input-only pin |
| Relay control | 33 | — | Via PC817 + BC547 |
| Manual button | 32 | — | Internal pull-up |
| OLED SCL | 22 | — | I2C clock |
| OLED SDA | 21 | — | I2C data |
| WS2812B data | 4 | — | Through 330Ω |

GPIO34 and GPIO35 used intentionally — ADC1 only (ADC2 is unusable with WiFi), and input-only (no risk of driving output voltage into a sensor accidentally).

---

## 5. Circuit Design

### ACS712 Voltage Divider

ACS712 runs on 5V. Peak output at maximum current = 4.5V. ESP32 ADC maximum = 3.3V. A voltage divider is mandatory.

**Design values:** 10kΩ (top) + 22kΩ (bottom)
*Note: 20kΩ was specified originally but 22kΩ was substituted — unavailable locally.*

$$V_{max\_at\_GPIO} = 4.5 \times \frac{22}{10+22} = 4.5 \times 0.6875 = 3.09V \quad ✓$$

$$V_{zero\_at\_GPIO} = 2.5 \times 0.6875 = 1.72V$$

$$Sensitivity = 100\ mV/A \times 0.6875 = 68.75\ mV/A$$

$$ADC\ counts/A = \frac{68.75}{3300} \times 4095 = 85.4$$

```
ACS712 VOUT ──[10kΩ]──┬── GPIO34
                       │
                     [22kΩ]
                       │
                      GND
```

### ZMPT101B — No Divider Needed
The ZMPT101B's onboard op-amp limits its output swing to stay within the 3.3V ADC range when the module is powered at 5V. Connected directly: VOUT → GPIO35.

### Relay Driver Circuit

ESP32 GPIO (3.3V, max 12mA) cannot directly drive a 5V relay coil (~70mA). Isolation circuit:

```
GPIO33 ──[1kΩ]──► PC817 LED anode
                  PC817 LED cathode ──► GND
                  PC817 collector ──[1kΩ]──► BC547 base
                                   BC547 emitter ──► GND
                                   BC547 collector ──► Relay coil (-)
                                   Relay coil (+) ──► 5V
                                   [1N4007 across coil, cathode to 5V]
```

PC817 optocoupler provides galvanic isolation between the ESP32 GPIO and the relay driver circuit, protecting the microcontroller from inductive spikes.

### Mains Layout

```
Wall socket Live ──► [5A Fuse] ──► [MOV to Neutral] ──► ACS712 IP+ ──► ACS712 IP- ──► Relay COM
                                                                                        Relay NO ──► Load Live
Wall socket Neutral ──────────────────────────────────────────────────────────────────► Load Neutral

Live ──► ZMPT101B L terminal
Neutral ──► ZMPT101B N terminal
```

Relay switches Live wire only. Neutral is hardwired through. Fuse and MOV on Live wire at inlet.

---

## 6. Firmware

### Structure

```
firmware/
├── step1_sense/
│   ├── step1_sense.ino     — Phase 1: ADC read, calibrate, print V/A/W
│   └── circuit.md           — wiring diagram
├── step2_detection/
│   └── step2_detection.ino — Phase 2: State machine, idle timer, Serial output
├── step3_relay/             — Phase 3: coming
├── step4_oled/              — Phase 4: coming
├── step5_leds/              — Phase 5: coming
├── step6_wifi/              — Phase 6: coming
└── phantom_load_killer/     — Final merged firmware
```

### Phase 1 Firmware Summary (`step1_sense.ino`)

| Function | Purpose |
|---|---|
| `calibrateZero()` | 2000 ADC samples at startup, stores DC bias of both sensors |
| `measureCurrentRMS()` | 1000 samples, subtract zero, compute RMS, noise-floor clamp at 0.05A |
| `measureVoltageRMS()` | 1000 samples, subtract zero, multiply by ZMPT_VFACTOR |
| `loop()` | Measure both, compute W = V×I, print to Serial every 500ms |

Key constants:
```cpp
const float ZMPT_VFACTOR       = 0.428f;   // Calibrated (see Section 8)
const float ACS712_SENSITIVITY  = 85.4f;   // ADC counts per Amp
const int   SAMPLES             = 1000;    // Per measurement window
```

### Phase 2 Firmware Summary (`step2_detection.ino`)

Adds a 3-state machine on top of Phase 1 sensing:

| State | Condition | Action |
|---|---|---|
| `ACTIVE` | I ≥ ACTIVE_THRESHOLD_A | Monitor, reset timer |
| `IDLE` | I < ACTIVE_THRESHOLD_A, timer not expired | Count down, print timer |
| `CUT` | Timer expired | (Phase 3: open relay). Stay until current detected |

Transitions:
- `ACTIVE → IDLE`: current drops below threshold
- `IDLE → ACTIVE`: current rises above threshold (resets timer)
- `IDLE → CUT`: idle timer expires
- `CUT → ACTIVE`: current detected (Phase 3: button press restores power)

Configurable:
```cpp
const float ACTIVE_THRESHOLD_A = 0.40f;   // Tune per device
const int   IDLE_TIMEOUT_MIN   = 2;       // 2 min for testing, 5–10 for production
```

---

## 7. Detection Strategy

### Revised Approach (from hardware testing)

**Original plan:** Measure standby wattage directly (e.g. TV at 5W standby).
**Revised plan:** Detect transition from active load to near-zero load.

The ACS712-20A + ESP32 ADC combination has a noise floor of ~0.3–0.5A (~70–115W). Direct standby measurement is not possible with this hardware.

Instead:
1. Device is ON → current above threshold → state: `ACTIVE`
2. User turns device off → current drops to noise floor → state: `IDLE`
3. Timer runs → if device stays off for N minutes → state: `CUT` → relay opens
4. User turns device on again → current spike → relay closes → state: `ACTIVE`

**Why this still achieves the goal:** Phantom loads occur because devices are left plugged in indefinitely after use. This strategy cuts power after a configurable idle period, eliminating the long-term phantom draw, even if it can't measure the phantom draw directly.

---

## 8. Calibration Data

### ZMPT101B — Voltage Calibration

| Measurement | Value |
|---|---|
| Initial ZMPT_VFACTOR | 0.5 (theoretical guess) |
| Serial reading with factor 0.5 | 286V |
| Multimeter reading (same socket) | 245V |
| Calibrated factor | `0.5 × (245/286) = 0.428` |
| Post-calibration Serial reading | 238–240V |
| Acceptable error | ~2–3% |

Mains voltage varies throughout the day (230V nominal, typically 220–245V actual).

### ACS712 — Zero Calibration

Zero point is measured automatically at startup via `calibrateZero()`:
- 2000 samples averaged
- Typical result: ~2130–2140 ADC counts (theoretical: 2133)
- **Critical:** Wire must be connected to GPIO34 before startup, or zero=0 is stored, causing wildly wrong readings

### ACS712 — Sensitivity Check

Not fully calibrated — no stable test load in the 0.5A–5A range available during Phase 1.
- Theoretical: 85.4 counts/A
- Using default value — recalibration pending with known resistive load

---

## 9. Known Limitations

### L1 — ACS712 Cannot Detect Standby Power

**Impact:** High. Cannot directly measure phantom loads in the 0.5–15W range.
**Workaround:** Transition-based detection (see Section 7).
**Proper fix:** Replace ACS712 with HLW8012 energy metering IC (~₹80).

### L2 — ESP32 ADC Nonlinearity

**Impact:** Medium. ADC is nonlinear below 150mV and above 3.1V.
**Mitigation:** Both sensors operate in the 1.5–2.5V range — middle of the linear zone.
**Effect on accuracy:** ±3–5% error on current and voltage readings.

### L3 — Cannot Distinguish Standby vs Unplugged Load

If the load device is physically unplugged from the output socket (not just turned off by its own switch), the system still enters IDLE → CUT. This is benign (relay opens an already-dead circuit) but the system doesn't distinguish the two cases.

### L4 — No Power Factor Measurement

Power calculation uses `W = V × I` assuming unity power factor (cos φ = 1). Most real devices have a power factor of 0.5–0.95. Real power = apparent power × PF. The displayed wattage is apparent power (VA), not true watts.

**Effect:** Overestimates real power consumption. Acceptable for threshold detection purposes.

### L5 — Single Phase, Single Load

System handles one load circuit. Not designed for multi-circuit monitoring.

### L6 — Clone ESP32 Upload Speed

This specific ESP32 board crashes esptool at default 921600 baud. Upload must use `UploadSpeed=115200`. Approximately 2× slower upload time.

---

## 10. Build Phases & Status

| Phase | Description | Status |
|---|---|---|
| 1 — Sensing | ACS712 + ZMPT101B → Serial output | ✅ Complete |
| 2 — Detection | State machine, idle timer | ✅ Complete |
| 3 — Relay | Drive relay via PC817 + BC547, manual button | 🔄 Next |
| 4 — OLED | SSD1306 display: V/A/W/state/timer | 🔲 Pending |
| 5 — LEDs | WS2812B RGB status indicators | 🔲 Pending |
| 6 — WiFi | Web dashboard, energy log | 🔲 Pending |
| 7 — Integration | Merge all, EEPROM config, OTA | 🔲 Pending |
| 8 — PCB | KiCad 2-layer board, JLCPCB order | 🔲 Pending |
| 9 — Enclosure | ABS box, panel connectors, conformal coat | 🔲 Pending |

---

## 11. Toolchain & Development Environment

### OS
Arch Linux, Wayland compositor (Hyprland).

### Arduino IDE — Not Used
Arduino IDE 2.x fails on Arch/Wayland with an Electron EPIPE crash at startup. Not investigated further.

### arduino-cli — Primary Tool

```fish
# Install
sudo pacman -S arduino-cli

# One-time setup
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Compile + Upload + Monitor (one-liner)
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 firmware/stepX/stepX.ino && \
arduino-cli upload  --fqbn esp32:esp32:esp32:UploadSpeed=115200 --port /dev/ttyUSB0 firmware/stepX/stepX.ino && \
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

### Serial Output
`arduino-cli monitor` runs in raw terminal mode. `\n` alone does not return cursor to column 0. All Serial output uses `\r\n` explicitly.

### Port Permissions
`/dev/ttyUSB0` requires user in `uucp` group:
```fish
sudo usermod -aG uucp $USER   # then re-login
```
Temporary workaround: `sudo chmod 666 /dev/ttyUSB0`

---

## 12. Future Upgrades

### Sensor Upgrade: HLW8012 (Recommended)

The HLW8012 is a single-chip AC energy metering IC used in commercial smart plugs (Sonoff POW, etc.). It outputs pulse trains on two GPIO pins — pulse frequency is proportional to power consumption. The ESP32 counts pulses via GPIO interrupts.

| Property | ACS712-20A (current) | HLW8012 |
|---|---|---|
| Min detectable power | ~70–115W | ~0.1W |
| Voltage measurement | ZMPT101B (separate) | Built-in |
| Power factor | Not measured | Measured |
| Output interface | Analogue → ADC | Pulse → GPIO interrupt |
| Cost | ₹120 + ₹150 | ₹80 total |
| PCB redesign needed | — | Yes |

### ADS1115 External ADC (Interim Option)

If HLW8012 is not immediately available, adding an ADS1115 16-bit ADC on the I2C bus (GPIO21/22, shared with OLED) improves minimum detection to ~2W. Acts as bridge until full redesign.

### OTA Updates (Phase 7)

Arduino OTA via WiFi — allows firmware updates without physical USB access. Important for a device installed inline in a wall socket circuit.

### EEPROM/NVS Storage (Phase 7)

Persist calibration values (ZMPT_VFACTOR, ACS712_SENSITIVITY, ACTIVE_THRESHOLD_A, IDLE_TIMEOUT_MIN) across reboots using ESP32's NVS (non-volatile storage). Currently lost on every reset.

---

## 13. Real-World Data

### Mains Voltage (measured, this installation)

| Reading | Value |
|---|---|
| Multimeter | 245V |
| ESP32 post-calibration | 238–240V |
| Nominal | 230V |
| Variation observed | ±7V across measurements |

### Current Sensor Observations

| Load | Expected current | ACS712 reading | Detected? |
|---|---|---|---|
| Nothing (noise floor baseline) | 0A | 0.000A | — |
| 25W soldering iron | 0.10A | 0.000A | ❌ Below noise floor |
| 1000W hair dryer | 4.08A | Spikes on mode switch | ⚠️ Partial (EMI spikes, not steady) |
| 1000W hair dryer (wire through IP+/IP-) | 4.08A | Pending re-test | 🔄 |

**Note on EMI spikes:** When the hair dryer was switched between power modes (Low/High), voltage spikes coupled into the ACS712 output, appearing as brief 0.1–0.5A readings. These are electromagnetic interference from the brush motor — not actual current measurements through the sensor. Confirmed because raw ADC was 0 throughout (wire not routed through sensor).

### Calibration Constants (this build)

| Constant | Value | Source |
|---|---|---|
| `ZMPT_VFACTOR` | 0.428 | Measured vs multimeter |
| `ACS712_SENSITIVITY` | 85.4 | Theoretical (field calibration pending) |
| `ACS712_ZERO` | ~2133 (runtime) | Auto-calibrated at startup |
| `ZMPT_ZERO` | ~2050 (runtime) | Auto-calibrated at startup |
| `ACTIVE_THRESHOLD_A` | 0.40A | Chosen above noise floor |
| `IDLE_TIMEOUT_MIN` | 2 (testing) / 5–10 (production) | Configurable |
