# Step 1 — Circuit Diagram: Current & Voltage Sensing

## Overview

```
                        USB 5V Adapter
                             │
                        [Micro USB]
                             │
                      ┌──────┴──────┐
                      │  ESP32      │
                      │  DevKit V1  │
                      │             │
          3V3 ────────┤ 3V3         │
          GND ────────┤ GND         │
           5V ────────┤ VIN/5V  out │──────────────────────────────┐
                      │             │                              │
                      │       GPIO34├──── ACS712 signal (divided)  │
                      │       GPIO35├──── ZMPT101B signal           │
                      └─────────────┘                              │
                                                                   │ 5V rail
                                        ┌──────────────────────────┤
                                        │                          │
                                   [ACS712]                   [ZMPT101B]
```

---

## ACS712-20A — Current Sensor

```
                      ACS712-20A Module
                    ┌─────────────────────┐
    AC Load wire ───┤ IP+              VCC ├──── 5V (DevKit VIN)
    AC Load wire ───┤ IP-              GND ├──── GND
                    │                 VOUT ├──── ──┐
                    └─────────────────────┘        │
                                                   │
                                    Voltage Divider (scales 4.5V → ~3.09V)
                                                   │
                                                 [10kΩ]
                                                   │
                                                   ├──────────── GPIO34
                                                   │
                                                 [22kΩ]
                                                   │
                                                  GND
```

**Why the divider:**
ACS712 runs on 5V → peak output = 4.5V → over ESP32's 3.3V ADC limit.
Divider: `4.5V × 22/(10+22) = 3.09V` → safe. ✓

---

## ZMPT101B — AC Voltage Sensor

```
                      ZMPT101B Module
                    ┌─────────────────────┐
    AC Live (L) ────┤ L                VCC ├──── 5V (DevKit VIN)
    AC Neutral (N) ─┤ N                GND ├──── GND
                    │                 VOUT ├──────────────────── GPIO35
                    └─────────────────────┘

     (No divider needed — ZMPT101B's onboard op-amp limits its output
      swing to fit within the ESP32's ADC range at 3.3V)
```

---

## Full Breadboard Diagram

```
USB Adapter
    │
    │ (Micro USB into ESP32)
    │
┌───┴─────────────────────────────────────────────┐
│                 ESP32 DevKit V1                  │
│  [EN]        [3V3] [GND] [VIN] [GPIO34] [GPIO35] │
└───┬────────────┬────┬────┬──────────┬────────┬──┘
    │            │    │    │          │        │
    │           3V3  GND   5V       ADC1    ADC2
    │            │    │    │
    │            │    │    └─────────────┬──────────────────┐
    │            │    │                  │                   │
    │            │    │           ┌──────┴──────┐    ┌──────┴──────┐
    │            │    │           │  ACS712-20A │    │  ZMPT101B   │
    │            │    │           │  VCC ← 5V   │    │  VCC ← 5V   │
    │            │    ├──────────►│  GND        │    │  GND        │
    │            │    ├──────────►│  GND        │    │  GND        │
    │            │               │  VOUT       │    │  VOUT ──────────► GPIO35
    │            │               └──────┬──────┘    └─────────────┘
    │            │                      │
    │            │                    [10kΩ]
    │            │                      │
    │            │                      ├──────────────────────────► GPIO34
    │            │                    [22kΩ]
    │            │                      │
    │            └──────────────────────┴─── GND rail
    │
  (Floating — USB supplies power)
```

---

## Mains Warning

```
  ⚠️  MAINS WIRING — HIGH VOLTAGE

  Mains socket
      │L (Live)
      ├─────────────────────────────────────► ZMPT101B  L  terminal
      │                                       (voltage sensing)
      ├── through ACS712 IP+ pin ──► ACS712 IP- pin ──► Load device
      │   (current sensing — wire is threaded through ACS712 hall sensor)
      │
      │N (Neutral)
      ├─────────────────────────────────────► ZMPT101B  N  terminal
      └─────────────────────────────────────► Load device (direct, no sensor)

  Rules:
  - Mains wires: use 1.5mm² rated wire
  - Insulate ALL mains connections with heat shrink
  - Never touch mains side while powered
  - Use GFCI/RCD extension cord for every powered test
```

---

## Pin Summary

| Signal | ESP32 Pin | Notes |
|---|---|---|
| ACS712 VOUT (divided) | GPIO34 | ADC1_CH6, input-only pin |
| ZMPT101B VOUT | GPIO35 | ADC1_CH7, input-only pin |
| Sensors VCC | VIN (5V out) | From DevKit when powered by USB |
| Sensors GND | GND | Common ground |

> GPIO34 and GPIO35 are used intentionally — they are **input-only** (no accidental output, no risk of driving voltage into a sensor) and on **ADC1** (ADC2 is blocked when WiFi is on).
