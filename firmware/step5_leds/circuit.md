# Step 5 — Circuit Diagram: WS2812B Status LEDs

> Builds on Step 4 wiring. Keep ALL existing connections (ACS712, ZMPT101B,
> relay module with BC547 buffer, manual button, SSD1306 OLED).
> Adds 3× WS2812B addressable RGB LEDs for at-a-glance status.

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
         │                 ├──────────────────────────────►│ Relay module VCC
         │                 └──────────────────────────────►│ WS2812B VCC [NEW]
         │                                                  │
         │  3V3 ───────────────────────────────────────────►│ OLED VCC
         │                                                  │
         │  GND ───────────┬──────────────────────────────►│ ACS712 GND
         │                 ├──────────────────────────────►│ ZMPT101B GND
         │                 ├──────────────────────────────►│ Relay module GND
         │                 ├──────────────────────────────►│ OLED GND
         │                 └──────────────────────────────►│ WS2812B GND [NEW]
         │                                                  │
         │  GPIO34 ─────────────────── ACS712 VOUT (÷ divider)
         │  GPIO35 ─────────────────── ZMPT101B VOUT
         │  GPIO26 ──[1kΩ]──► BC547 Base → Collector → Relay IN
         │  GPIO32 ─────────────────── Manual button
         │  GPIO21 (SDA) ──────────── OLED SDA
         │  GPIO22 (SCL) ──────────── OLED SCL
         │  GPIO4 ───[330Ω]────────── WS2812B DIN             [NEW]
         └──────────────────────────────────────────────────┘
```

---

## 2. WS2812B Wiring

Three WS2812B LEDs daisy-chained. Can be individual 5mm through-hole WS2812B,
or a strip cut to 3 LEDs, or 3 separate WS2812B breakout modules.

```
  ESP32 GPIO4 ──[330Ω]──► LED #0 DIN
                          LED #0 DOUT → LED #1 DIN
                                        LED #1 DOUT → LED #2 DIN
                                                      LED #2 DOUT → (unused)

  ESP32 VIN (5V) ──────► All WS2812B VCC (common rail)
  ESP32 GND ───────────► All WS2812B GND (common rail)
```

```
  Individual WS2812B pinout (top view, flat side down):
  ┌─────────┐
  │  DIN  1 ├── Data in (from previous LED or GPIO)
  │  VCC  2 ├── 5V
  │  GND  3 ├── Ground
  │  DOUT 4 ├── Data out (to next LED)
  └─────────┘
```

> **330Ω resistor** on the data line prevents signal reflections. Place it as
> close to LED #0 DIN as possible. Without it, long wires may cause flickering.

> **100µF capacitor** across VCC/GND near the first LED is recommended for
> strips but optional for just 3 LEDs at low brightness.

> **3.3V data with 5V power:** WS2812B wants 0.7×VCC = 3.5V as logic HIGH.
> ESP32 outputs 3.3V — right at the edge. Usually works fine with short wires
> (<15cm). If LEDs don't respond, add a simple level shifter (BSS138 module)
> or power WS2812B from 3.3V instead of 5V (dimmer but reliable).

---

## 3. LED Colour Assignments

| LED # | State | Colour | Behaviour |
|-------|-------|--------|-----------|
| 0 | ACTIVE | Green | Solid — device drawing power |
| 1 | IDLE | Yellow/Amber | Breathing — countdown running |
| 2 | CUT | Red | Solid — relay open, power cut |

Only one LED is lit at a time. During IDLE, LED #1 breathes (fades in/out)
to show the timer is ticking. Brightness is kept low (~30/255) to reduce
power draw and avoid blinding in dark rooms.

---

## 4. Full Connection Table

| From | To | Colour | Notes |
|---|---|---|---|
| ESP32 VIN | ACS712 VCC | Red | 5V power |
| ESP32 VIN | ZMPT101B VCC | Red | 5V power |
| ESP32 VIN | Relay module VCC | Red | 5V power |
| ESP32 VIN | WS2812B VCC | Red | 5V power |
| ESP32 3V3 | OLED VCC | Orange | 3.3V |
| ESP32 GND | ACS712 GND | Black | |
| ESP32 GND | ZMPT101B GND | Black | |
| ESP32 GND | Relay module GND | Black | |
| ESP32 GND | OLED GND | Black | |
| ESP32 GND | WS2812B GND | Black | |
| ESP32 GND | BC547 Emitter | Black | |
| ESP32 GPIO34 | ACS712 divider mid | Yellow | Analog in |
| ESP32 GPIO35 | ZMPT101B VOUT | Yellow | Analog in |
| ESP32 GPIO26 | 1kΩ → BC547 Base | Green | Relay drive |
| BC547 Collector | Relay module IN | Green | |
| ESP32 GPIO21 | OLED SDA | Blue | I2C data |
| ESP32 GPIO22 | OLED SCL | White | I2C clock |
| ESP32 GPIO4 | 330Ω → WS2812B #0 DIN | Purple | LED data |
| WS2812B #0 DOUT | WS2812B #1 DIN | Purple | Daisy chain |
| WS2812B #1 DOUT | WS2812B #2 DIN | Purple | Daisy chain |
| ESP32 GPIO32 | Button → GND | Any | Internal pull-up |

---

## 5. Library Dependency

```bash
arduino-cli lib install "Adafruit NeoPixel"
```
