# Phantom Load Killer — ESP32 Smart AC Power Switch

An ESP32-based smart power switch that monitors real-time AC power consumption and automatically cuts power when a device goes into standby/idle mode — saving electricity 24/7, no cloud required.

**How it works:** ACS712 + ZMPT101B sense live current and voltage. When load drops below a threshold (`ACTIVE_THRESHOLD_A = 0.40A`) and stays low for `IDLE_TIMEOUT_SEC` seconds, the relay opens and kills power. One button press (or the web dashboard) restores it. The device tracks how much energy it has saved.

**Access point dashboard:** Connect to Wi-Fi `PhantomKiller` / `phantom123` → open `http://192.168.4.1`

---

## Hardware at a Glance

| Component | Model | Purpose |
|---|---|---|
| Microcontroller | ESP32 DevKit V1 (30-pin) | Brain, WiFi AP, ADC |
| Current sensor | ACS712-20A | Measures load current (AC) |
| Voltage sensor | ZMPT101B | Measures AC mains voltage |
| Relay | SRD-05VDC-SL-C (10A) | Cuts/restores power to load |
| Relay driver | BC547 NPN transistor | Buffers ESP32 GPIO to relay IN |
| Display | SSD1306 0.96" OLED (I2C) | Shows live V / A / W / state |
| Status LED | Common-cathode RGB LED | Green=Active / Amber=Idle / Red=Cut |
| Button | Tactile push button | Manual restore from CUT state |
| Power supply | 5V USB adapter → DevKit micro-USB | Powers ESP32 + sensors + relay |
| Protection | 5A fuse, MOV | Mains safety |

**ADC pins used:**
- `GPIO34` — ACS712 output (after 10kΩ/22kΩ voltage divider)
- `GPIO35` — ZMPT101B output (direct)

Both are ADC1 input-only pins → safe when WiFi is active.

---

## Web Dashboard

The ESP32 hosts its own WiFi access point — no router needed.

- **SSID:** `PhantomKiller`
- **Password:** `phantom123`
- **URL:** `http://192.168.4.1`

Features:
- Live voltage, current, and wattage meters (2s refresh)
- Device state indicator (ACTIVE / IDLE / CUT)
- Idle countdown progress bar
- Manual relay ON/OFF buttons
- Auto-detection toggle (enable/disable idle cut)
- Power saved tracker (Wh saved, cut count, total time off)
- Dark/light theme toggle (persisted in browser localStorage)

---

## Calibration Values

| Parameter | Value | Notes |
|---|---|---|
| `ZMPT_VFACTOR` | 0.222 | Calibrated for this specific ZMPT101B module |
| `ACS712_SENSITIVITY` | 85.4 counts/A | After voltage divider scaling |
| `ACTIVE_THRESHOLD_A` | 0.40 A | Below this = device idle |
| `IDLE_TIMEOUT_SEC` | 10 s | Time below threshold before auto-cut |
| ADC noise clamp | 0.12 A | Readings below this clamped to 0 |

---

## Known Limitations

### ACS712 + ESP32 ADC Noise Floor
The ACS712-20A has a sensitivity of 100mV/A. After the 10kΩ/22kΩ voltage divider, this drops to ~68.75mV/A. The ESP32's onboard ADC has ~±50mV of inherent noise, meaning anything below ~0.3–0.5A (~70–115W) is indistinguishable from zero.

**Impact:** Cannot directly measure standby draws (TV at 5W, charger at 2W).

**Workaround:** Detect the *transition* from active → near-zero current. When current drops below `ACTIVE_THRESHOLD_A` and stays low → auto-cut. Works for: TV turned off by remote, hair dryer switched off, laptop charger idle, etc.

---

## Development Phases

All six firmware phases are complete, each building on the previous:

### Phase 1 — Current & Voltage Sensing ✅
Read ACS712 and ZMPT101B, print live V / A / W to Serial.
- Zero calibration at startup, voltage divider for ACS712
- `ZMPT_VFACTOR` tuned from 0.428 → 0.222 after calibration testing

**Firmware:** `firmware/step1_sense/` · **Circuit:** `firmware/step1_sense/circuit.md`

### Phase 2 — Phantom Load Detection ✅
3-state machine: ACTIVE → IDLE (countdown) → CUT.
- Current threshold detection, configurable timeout
- Serial output with state transitions and timer

**Firmware:** `firmware/step2_detection/`

### Phase 3 — Relay Switching ✅
BC547 NPN buffer drives relay module. Manual button restores from CUT.
- GPIO26 → 1kΩ → BC547 Base, Collector → Relay IN
- Button on GPIO32 with INPUT_PULLUP, debounced

**Firmware:** `firmware/step3_relay/` · **Circuit:** `firmware/step3_relay/circuit.md`

### Phase 4 — OLED Display ✅
SSD1306 128×64 I2C display showing live readings and state.
- V/A on top line, large W display, state + countdown bar

**Firmware:** `firmware/step4_oled/` · **Circuit:** `firmware/step4_oled/circuit.md`

### Phase 5 — RGB Status LED ✅
Common-cathode RGB LED with LEDC PWM (5kHz, 8-bit).
- Green solid = ACTIVE, amber breathing = IDLE, red solid = CUT
- 330Ω current-limiting resistors on each colour pin

**Firmware:** `firmware/step5_leds/` · **Circuit:** `firmware/step5_leds/circuit.md`

### Phase 6 — WiFi Dashboard ✅
ESP32 AP mode web dashboard with live monitoring and relay control.
- REST API: `/api/data`, `/api/relay?action=on|off`, `/api/auto?mode=1|0`
- Dark/light theme, power saved tracking, responsive design

**Firmware:** `firmware/step6_dashboard/` · **Circuit:** `firmware/step6_dashboard/circuit.md`

---

## Flashing

```bash
# One-time setup
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Install required libraries
arduino-cli lib install "Adafruit SSD1306" "Adafruit GFX Library"

# Compile final firmware
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 phantom_load_killer.ino

# Upload (check port with: ls /dev/ttyUSB*)
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 \
  --port /dev/ttyUSB0 phantom_load_killer.ino

# Serial monitor
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```

> **Note:** This ESP32 clone requires `UploadSpeed=115200` (default 921600 causes crash).
>
> **Arch Linux:** Run `sudo usermod -aG uucp $USER` and re-login before first upload.

---

## Safety Rules

1. Always use a **GFCI/RCD extension cord** for every mains-powered test
2. **Never touch** the mains side while powered
3. **Fuse on Live wire only** — never Neutral
4. ACS712 voltage divider is not optional — 4.5V into ESP32 ADC = dead ESP32
5. Relay switches **Live wire only** — Neutral goes straight through to load
6. Use 1.5mm² mains-rated wire for all HV connections
7. Heat shrink all exposed mains conductors

---

## Project Structure

```
phantom-load-killer/
├── README.md                        ← this file
├── PARTS_LIST.md                    ← full BOM with costs (₹)
├── CIRCUIT.md                       ← complete circuit diagram (all components)
├── phantom_load_killer.ino          ← final firmware (ready to flash)
├── firmware/
│   ├── step1_sense/                 ← Phase 1: current & voltage sensing
│   │   ├── step1_sense.ino
│   │   └── circuit.md
│   ├── step2_detection/             ← Phase 2: state machine logic
│   │   └── step2_detection.ino
│   ├── step3_relay/                 ← Phase 3: relay + button
│   │   ├── step3_relay.ino
│   │   └── circuit.md
│   ├── step4_oled/                  ← Phase 4: OLED display
│   │   ├── step4_oled.ino
│   │   └── circuit.md
│   ├── step5_leds/                  ← Phase 5: RGB status LED
│   │   ├── step5_leds.ino
│   │   └── circuit.md
│   └── step6_dashboard/             ← Phase 6: WiFi AP dashboard
│       ├── step6_dashboard.ino
│       └── circuit.md
└── research/
    └── README.md                    ← research notes index
```

---

## Current Status

| Phase | Status |
|---|---|
| Phase 1 — Sensing | ✅ Complete |
| Phase 2 — Detection | ✅ Complete |
| Phase 3 — Relay | ✅ Complete |
| Phase 4 — OLED | ✅ Complete |
| Phase 5 — RGB LED | ✅ Complete |
| Phase 6 — Dashboard | ✅ Complete |
| **All phases done** | **Compile: 77% flash, 14% RAM, zero errors** |
| Phase 8 — PCB design | 🔲 Not started |
| Phase 9 — Enclosure | 🔲 Not started |
