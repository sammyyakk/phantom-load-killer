# Step 4 — Circuit Diagram: OLED Display

> Builds on Step 3 wiring. Keep ALL existing connections (ACS712, ZMPT101B,
> relay module with BC547 buffer, manual button).
> Adds a 0.96" SSD1306 128×64 I2C OLED module.

---

## 1. Full System Pin Map

```
                         5V USB Adapter
                              │
                         [Micro USB]
                              │
         ┌────────────────────┴────────────────────────────┐
         │                 ESP32 DevKit V1                  │
         │                                                  │
         │  VIN (5V out) ──┬──────────────────────────────►│ ACS712 VCC
         │                 ├──────────────────────────────►│ ZMPT101B VCC
         │                 └──────────────────────────────►│ Relay module VCC
         │                                                  │
         │  3V3 ───────────────────────────────────────────►│ OLED VCC [NEW]
         │                                                  │
         │  GND ───────────┬──────────────────────────────►│ ACS712 GND
         │                 ├──────────────────────────────►│ ZMPT101B GND
         │                 ├──────────────────────────────►│ Relay module GND
         │                 └──────────────────────────────►│ OLED GND [NEW]
         │                                                  │
         │  GPIO34 ─────────────────── ACS712 VOUT (÷ divider)
         │  GPIO35 ─────────────────── ZMPT101B VOUT
         │  GPIO26 ──[1kΩ]──► BC547 Base → Collector → Relay IN
         │  GPIO32 ─────────────────── Manual button
         │  GPIO21 (SDA) ──────────── OLED SDA              [NEW]
         │  GPIO22 (SCL) ──────────── OLED SCL              [NEW]
         └──────────────────────────────────────────────────┘
```

---

## 2. OLED Module Wiring

Standard 4-pin I2C SSD1306 module (0.96", 128×64).

```
  OLED Module
  ┌──────────────────┐
  │  VCC SDA SCL GND │
  └──┬───┬───┬───┬───┘
     │   │   │   │
     │   │   │   └──► ESP32 GND
     │   │   └──────► ESP32 GPIO22 (SCL)
     │   └──────────► ESP32 GPIO21 (SDA)
     └──────────────► ESP32 3V3
```

- I2C address: **0x3C** (most common for 128×64 SSD1306 modules)
- Power: 3.3V from ESP32's 3V3 pin (not VIN — OLED runs on 3.3V)
- No external pull-ups needed — most modules have 4.7kΩ on board

> Some modules have VCC and GND swapped! Check your module's silk screen
> before powering on. Applying 3.3V to GND will kill the OLED instantly.

---

## 3. Display Layout

```
  ┌────────────────────────────┐
  │ PHANTOM LOAD KILLER        │  ← Row 0: title (only on boot)
  │                            │
  │  238.5V   0.421A   101.0W  │  ← Row 2-3: live readings
  │                            │
  │  State: IDLE               │  ← Row 4: current state
  │  Timeout: 0m 07s left      │  ← Row 5: countdown / status
  │                            │
  │  ░░░░░░░░░░░░░░████████   │  ← Row 7: progress bar (idle only)
  └────────────────────────────┘
```

During CUT state, row 5 shows "BTN to restore" and the progress bar disappears.

---

## 4. Full Connection Table

| From | To | Colour | Notes |
|---|---|---|---|
| ESP32 VIN | ACS712 VCC | Red | 5V power |
| ESP32 VIN | ZMPT101B VCC | Red | 5V power |
| ESP32 VIN | Relay module VCC | Red | 5V power |
| ESP32 3V3 | OLED VCC | Orange | 3.3V — NOT 5V |
| ESP32 GND | ACS712 GND | Black | |
| ESP32 GND | ZMPT101B GND | Black | |
| ESP32 GND | Relay module GND | Black | |
| ESP32 GND | OLED GND | Black | |
| ESP32 GND | BC547 Emitter | Black | |
| ESP32 GPIO34 | ACS712 divider mid | Yellow | Analog in |
| ESP32 GPIO35 | ZMPT101B VOUT | Yellow | Analog in |
| ESP32 GPIO26 | 1kΩ → BC547 Base | Green | Relay drive |
| BC547 Collector | Relay module IN | Green | |
| ESP32 GPIO21 | OLED SDA | Blue | I2C data |
| ESP32 GPIO22 | OLED SCL | White | I2C clock |
| ESP32 GPIO32 | Button → GND | Any | Internal pull-up |

---

## 5. Library Dependencies

Install via Arduino Library Manager or `arduino-cli`:

```bash
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
```

These pull in `Adafruit_BusIO` automatically (I2C/SPI abstraction).
