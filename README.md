# Phantom Load Killer

An ESP32-based smart power switch that detects phantom/standby loads on AC mains and automatically cuts power when a device goes idle — saving electricity 24/7.

**Core idea:** Measure real-time AC current. If a device drops below a wattage threshold (e.g. TV in standby = 5W), start a timer. If it stays below threshold for N minutes → relay opens → power cut. Button or auto-detection restores power.

---

## Hardware at a Glance

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32 DevKit V1 (30-pin) | Brain, WiFi, ADC |
| Current sensor | ACS712-20A | Measures load current (AC) |
| Voltage sensor | ZMPT101B | Measures AC mains voltage |
| Relay | SRD-05VDC-SL-C (10A) | Cuts/restores power to load |
| Relay driver | BC547 NPN + PC817 optocoupler | Isolates ESP32 GPIO from relay coil |
| Display | SSD1306 0.96" OLED (I2C) | Shows live V / A / W |
| Status LEDs | WS2812B × 3 | Green=Active / Yellow=Standby / Red=Cut |
| Power supply | 5V USB adapter → DevKit micro-USB | Powers ESP32 + sensors + relay |
| Protection | 1N4007 flyback diode, 5A fuse, MOV | Mains safety |

**ADC pins used:**
- `GPIO34` — ACS712 output (after 10kΩ/22kΩ voltage divider)
- `GPIO35` — ZMPT101B output (direct)

Both are ADC1 input-only pins → safe when WiFi is active.

---

## Voltage Divider (MANDATORY for ACS712)

ACS712 on 5V → peak output 4.5V → exceeds ESP32 ADC max (3.3V). Divider scales it down:

```
ACS712 VOUT ── [10kΩ] ── GPIO34 ── [22kΩ] ── GND
```

$$V_{max} = 4.5 \times \frac{22}{10+22} = 3.09V \quad ✓$$

---

## Known Limitations & Design Decisions

### ACS712 + ESP32 ADC Noise Floor
The ACS712-20A has a sensitivity of 100mV/A. After the 10kΩ/22kΩ voltage divider, this drops to ~68.75mV/A. The ESP32's onboard ADC has ~±50mV of inherent noise, meaning anything below ~0.3–0.5A (~70–115W) is indistinguishable from zero.

**Impact:** Cannot directly measure standby draws (TV at 5W, charger at 2W, set-top box at 8W).

**Current workaround:** Detect the *transition* from active → near-zero current instead of measuring the standby value itself. Works for the core use case.

**Proper fix options (in order of preference):**
| Option | Min detectable | Cost | Notes |
|---|---|---|---|
| HLW8012 IC | ~0.1W | ₹80 | Purpose-built energy metering IC, used in Sonoff POW |
| ACS712-5A + ADS1115 16-bit ADC | ~2W | ₹200 extra | Same sensor family, external ADC removes noise floor |
| ACS712-5A alone | ~30W | ₹120 | Marginal improvement, ADC still the bottleneck |

The bottleneck is **not the sensor** — it's the ESP32's ADC. Swapping to ACS712-5A alone does not solve the problem.

---


Work through these in order. Each phase is self-contained and testable before moving to the next.

---

### Phase 1 — Current & Voltage Sensing ✅ COMPLETE
> **Goal:** Read ACS712 and ZMPT101B, print live V / A / W to Serial at 115200 baud.

- [x] Install `arduino-cli` and ESP32 core
- [x] Wire ACS712: VCC→5V, GND→GND, VOUT→10kΩ→GPIO34→22kΩ→GND
- [x] Wire ZMPT101B: VCC→5V, GND→GND, VOUT→GPIO35
- [x] Flash `firmware/step1_sense/step1_sense.ino`
- [x] Verify zero calibration prints on Serial Monitor startup
- [x] Connect ZMPT101B to mains via GFCI extension cord
- [x] Tune `ZMPT_VFACTOR` to 0.428 — reads 239V, multimeter reads 245V (within 3%, acceptable)
- [x] Verified current spikes on hairdryer mode switching — ACS712 responsive

**Key findings from Phase 1:**
- ACS712-20A noise floor is ~0.3–0.5A (~75W at 230V). Cannot measure small standby loads directly.
- Detection strategy revised: detect **active → zero transition**, not the standby wattage itself.
- Zero calibration must run at startup with the wire already connected to GPIO34.
- `arduino-cli` with `UploadSpeed=115200` required for this ESP32 clone (default 921600 causes crash).
- `ZMPT_VFACTOR = 0.428` calibrated for this specific ZMPT101B module.

**Firmware:** `firmware/step1_sense/step1_sense.ino`
**Circuit:** `firmware/step1_sense/circuit.md`

---

### Phase 2 — Phantom Load Detection Logic
> **Goal:** Classify device state (ACTIVE / IDLE / CUT) based on current threshold. No relay yet — just print state + timer to Serial.

> **Revised strategy (from Phase 1 findings):** ACS712-20A cannot detect watt-level standby draws.
> Instead: detect when current drops **below a minimum threshold** (e.g. 0.4A / ~90W) and start a
> countdown timer. If current stays low for N minutes → CUT state. If it spikes again → back to ACTIVE.
> This catches: TV turned off by remote, hair dryer switched off, laptop charger idle, etc.

- [ ] Define `ACTIVE_THRESHOLD_A` (e.g. 0.4A) — above this = device is actively drawing power
- [ ] Define `IDLE_TIMEOUT_MIN` (e.g. 5 min) — how long below threshold before cutting
- [ ] Implement 3-state machine: `ACTIVE` → `IDLE` (timer counting) → `CUT`
- [ ] Any current spike above threshold while IDLE → reset timer, back to `ACTIVE`
- [ ] Print state + countdown timer to Serial on every state change
- [ ] Test: turn hairdryer on → confirm ACTIVE, turn off → confirm IDLE + countdown, wait → CUT

**Firmware:** `firmware/step2_detection/`

---

### Phase 3 — Relay Switching
> **Goal:** Drive the relay via BC547 + PC817 optocoupler. Cut power when state = CUT. Restore on button press.

- [ ] Wire relay driver circuit: GPIO→1kΩ→PC817 LED→GND, PC817 output→BC547 base via 1kΩ, BC547 collector→relay coil, 1N4007 flyback across coil
- [ ] Wire relay contacts: COM→Live in, NO→Live out to load
- [ ] Wire tactile button: GPIO with internal pull-up
- [ ] Test relay clicks on GPIO HIGH (use LED first, no mains)
- [ ] Test full loop: standby detected → relay opens → button press → relay closes
- [ ] Test with mains + load via GFCI cord (with someone present)

**Firmware:** `firmware/step3_relay/`

---

### Phase 4 — OLED Display
> **Goal:** Show live readings and device state on the SSD1306 0.96" I2C display.

- [ ] Wire OLED: VCC→3V3, GND→GND, SCL→GPIO22, SDA→GPIO21
- [ ] Install `Adafruit SSD1306` and `Adafruit GFX` libraries via arduino-cli
- [ ] Display: Line 1 = Voltage (V), Line 2 = Current (A), Line 3 = Power (W), Line 4 = State
- [ ] Show countdown timer when in STANDBY state
- [ ] Show "POWER CUT" screen when relay is open

**Firmware:** `firmware/step4_oled/`

---

### Phase 5 — WS2812B Status LEDs
> **Goal:** RGB LEDs give at-a-glance status without looking at the display.

- [ ] Wire 3× WS2812B: data line through 330Ω to GPIO4 (or GPIO5), VCC→5V, GND→GND
- [ ] Install `FastLED` or `Adafruit NeoPixel` library
- [ ] Green = ACTIVE (device drawing normal power)
- [ ] Yellow/Orange = STANDBY (countdown running)
- [ ] Red = CUT (relay open, power off)
- [ ] Breathing animation on STANDBY to show timer progress

**Firmware:** `firmware/step5_leds/`

---

### Phase 6 — WiFi Dashboard
> **Goal:** Host a simple web page on the ESP32's local IP showing live stats and controls.

- [ ] Connect to home WiFi (credentials in config, not hardcoded)
- [ ] Serve a minimal HTML page: live V / A / W / state, manual relay toggle button
- [ ] Auto-refresh every 2 seconds (or WebSocket for real-time)
- [ ] Show total energy saved (kWh) since last reset
- [ ] Optional: MQTT publish to Home Assistant

**Firmware:** `firmware/step6_wifi/`

---

### Phase 7 — Integration & Tuning
> **Goal:** Combine all phases into one production firmware. Tune thresholds for your specific devices.

- [ ] Merge all modules into `firmware/phantom_load_killer/`
- [ ] Add EEPROM/NVS storage for thresholds and calibration values (survive reboot)
- [ ] Add over-the-air (OTA) update support
- [ ] Field test: TV, laptop charger, set-top box, washing machine, microwave standby
- [ ] Document actual standby wattages found for each device
- [ ] Tune per-device profiles if needed

**Firmware:** `firmware/phantom_load_killer/`

---

### Phase 8 — PCB Design
> **Goal:** Move from perfboard to a proper 2-layer PCB (100×80mm).

- [ ] Install KiCad
- [ ] Draw schematic from verified perfboard circuit
- [ ] Layout PCB: HV zone (mains) isolated from LV zone (ESP32/sensors) — 6mm min clearance
- [ ] Add silkscreen labels for all connectors and test points
- [ ] DRC check — zero errors
- [ ] Export Gerbers → order from JLCPCB (~₹600 for 5 pcs)
- [ ] Solder and test assembled PCB

**Files:** `hardware/kicad/`

---

### Phase 9 — Enclosure & Final Assembly
> **Goal:** Finished, safe, enclosured device ready for continuous use.

- [ ] Cut notch in ABS enclosure for DevKit USB port (reprogramming access)
- [ ] Mount IEC C14 inlet socket and Indian 3-pin load socket
- [ ] Panel-mount fuse holder (5A, on Live wire)
- [ ] Conformal coat PCB — especially HV zone
- [ ] Label enclosure: input voltage, max load (1000W/5A), fuse rating
- [ ] Final safety check: Megger test or 500V insulation resistance check between HV and LV

---

## Flashing (arduino-cli, no IDE needed)

```fish
# One-time setup
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Compile
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/step1_sense/step1_sense.ino

# Upload (check your port with: ls /dev/ttyUSB*)
arduino-cli upload --fqbn esp32:esp32:esp32 --port /dev/ttyUSB0 firmware/step1_sense/step1_sense.ino

# Serial monitor
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

> **Arch Linux:** Run `sudo usermod -aG uucp $USER` and re-login before first upload.

---

## Safety Rules (non-negotiable)

1. Always use a **GFCI/RCD extension cord** for every mains-powered test
2. **Never touch** the mains side while powered
3. **Fuse on Live wire only** — never Neutral
4. ACS712 voltage divider is not optional — 4.5V into ESP32 ADC = dead ESP32
5. 6mm minimum copper clearance between HV and LV on PCB
6. Relay switches **Live wire only** — Neutral goes straight through to load
7. Use 1.5mm² mains-rated wire for all HV connections
8. Conformal coat the finished PCB before enclosing

---

## Project Structure

```
phantom-load-killer/
├── README.md                        ← you are here
├── PARTS_LIST.md                    ← full BOM with costs (₹)
├── firmware/
│   ├── step1_sense/
│   │   ├── step1_sense.ino          ← Phase 1 firmware
│   │   └── circuit.md               ← wiring diagram
│   ├── step2_detection/             ← Phase 2 (coming)
│   ├── step3_relay/                 ← Phase 3 (coming)
│   ├── step4_oled/                  ← Phase 4 (coming)
│   ├── step5_leds/                  ← Phase 5 (coming)
│   ├── step6_wifi/                  ← Phase 6 (coming)
│   └── phantom_load_killer/         ← Phase 7 final merged firmware
└── hardware/
    └── kicad/                       ← Phase 8 PCB files
```

---

## Current Status

| Phase | Status |
|---|---|
| Phase 1 — Sensing | ✅ Complete — voltage calibrated (0.428 factor), current responsive, noise floor documented |
| Phase 2 — Detection logic | 🔄 In progress |
| Phase 3 — Relay switching | 🔲 Not started |
| Phase 4 — OLED display | 🔲 Not started |
| Phase 5 — Status LEDs | 🔲 Not started |
| Phase 6 — WiFi dashboard | 🔲 Not started |
| Phase 7 — Integration | 🔲 Not started |
| Phase 8 — PCB design | 🔲 Not started |
| Phase 9 — Enclosure | 🔲 Not started |
