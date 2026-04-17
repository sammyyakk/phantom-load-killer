# Step 3 — Circuit Diagram: Relay Module + Manual Button

> Builds on Step 1 wiring (ACS712 + ZMPT101B). Keep all existing connections.
> Uses a **relay module** (not a bare relay) — transistor, flyback diode, and driver
> are already built into the module. Only 3 wires needed on the control side.

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
         │                 └──────────────────────────────►│ Relay module VCC [NEW]
         │                                                  │
         │  GND ───────────┬──────────────────────────────►│ ACS712 GND
         │                 ├──────────────────────────────►│ ZMPT101B GND
         │                 └──────────────────────────────►│ Relay module GND [NEW]
         │                                                  │
         │  GPIO34 ─────────────────── ACS712 VOUT (÷ divider)
         │  GPIO35 ─────────────────── ZMPT101B VOUT
         │  GPIO26 ──[1kΩ]──► BC547 Base → BC547 Collector → Relay module IN [NEW]
         │  GPIO32 ─────────────────── Manual button        [NEW]
         └──────────────────────────────────────────────────┘
```

---

## 2. Relay Module Wiring

The relay module's internal pull-up to 5V is too strong for this ESP32 clone to
pull LOW directly. A BC547 NPN transistor is used as a buffer — ESP32 drives the
base, collector pulls relay IN hard to GND.

```
  ESP32 GPIO26 ──[1kΩ]──► BC547 Base (pin 2)
  BC547 Emitter (pin 3) → GND
  BC547 Collector (pin 1) → Relay module IN

  Relay Module
  ┌──────────────┐
  │ VCC  GND  IN │  ← control side
  │              │
  │ NO  COM  NC  │  ← AC side (mains voltage)
  └──────────────┘

  VCC → ESP32 VIN  (5V)
  GND → ESP32 GND
  IN  → BC547 Collector
```

**Logic (now active HIGH from ESP32's perspective):**
- GPIO26 HIGH → BC547 conducts → Relay IN pulled to GND → relay energises → **load ON**
- GPIO26 LOW  → BC547 off → Relay IN floats high → relay de-energises → **load cut**

> Firmware uses RELAY_ON = LOW, RELAY_OFF = HIGH for active LOW module.
> BC547 inverts: GPIO HIGH → relay IN LOW. So firmware must be flipped:
> **RELAY_ON = HIGH, RELAY_OFF = LOW** when using BC547 buffer.

---

## 3. Manual Restore Button

```
GPIO32 ──────────┬──── [BUTTON] ──── GND
                 │
         (INPUT_PULLUP: internal 45kΩ to 3.3V)

  Not pressed:  GPIO32 = HIGH  (pull-up holds it high)
  Pressed:      GPIO32 = LOW   (button shorts to GND)
```

No external resistor needed. Two wires only: GPIO32 and GND.

---

## 4. Mains Wiring

**⚠ DANGER — 220V AC. Unplug from wall before touching any mains wire.**

```
Wall Socket
  │
  ├─ LIVE ──[5A Fuse]──[MOV]── ACS712 IP+ ── ACS712 IP– ── Relay COM
  │                                                          Relay NO ── Load LIVE
  │
  └─ NEUTRAL ──────────────────────────────────────────────── Load NEUTRAL

  LIVE    ── ZMPT101B terminal L
  NEUTRAL ── ZMPT101B terminal N
```

Safety rules:
- Fuse first on Live wire, before any other component.
- MOV clamps transients between Live and Neutral (after fuse).
- Relay always on the Live wire, never on Neutral.
- Screw terminals or crimps only — no bare twisted joints.
- Heat shrink all exposed mains conductors.

---

## 5. Full Connection Table

| From | To | Colour |
|---|---|---|
| ESP32 VIN | ACS712 VCC | Red |
| ESP32 VIN | ZMPT101B VCC | Red |
| ESP32 VIN | Relay module VCC | Red |
| ESP32 GND | ACS712 GND | Black |
| ESP32 GND | ZMPT101B GND | Black |
| ESP32 GND | Relay module GND | Black |
| ESP32 GPIO34 | ACS712 divider midpoint | Yellow |
| ESP32 GPIO35 | ZMPT101B VOUT | Yellow |
| ESP32 GPIO26 | 1kΩ resistor leg A | Orange |
| 1kΩ resistor leg B | BC547 Base (pin 2) | Orange |
| BC547 Emitter (pin 3) | GND | Black |
| BC547 Collector (pin 1) | Relay module IN | Orange |
| Relay module VCC | ESP32 VIN | Red |
| Relay module GND | ESP32 GND | Black |
| ESP32 GPIO32 | Button pin A | Green |
| Button pin B | GND | Black |
| ACS712 VOUT | 10kΩ top leg | Yellow |
| 10kΩ/22kΩ junction | GPIO34 | Yellow |
| 22kΩ bottom leg | GND | Black |

---

## 6. Relay Module AC Pinout

```
  Top of module (AC contacts side):
  ┌────────────────────────┐
  │   NO      COM     NC   │
  └────────────────────────┘

  NO  → Mains Live out (to load socket)
  COM → Mains Live in  (from ACS712 IP–)
  NC  → leave unconnected
```

---

## 7. Logic Table

BC547 inverts the signal — ESP32 drives HIGH to energise relay.

| GPIO26 | BC547 | Relay IN | Relay | Load |
|--------|-------|----------|-------|------|
| HIGH | Saturated | ~0V (GND) | Energised | ✅ Power ON |
| LOW | Cut off | ~5V (pulled up) | De-energised | ❌ Power OFF |

**Firmware defines (with BC547 buffer):**
```cpp
#define RELAY_ON  HIGH   // GPIO HIGH → BC547 on → relay IN LOW → relay energises
#define RELAY_OFF LOW    // GPIO LOW  → BC547 off → relay de-energises
```

**Boot sequence:**
1. `pinMode(RELAY_PIN, OUTPUT)` + `setRelay(true)` → GPIO26 goes HIGH → BC547 conducts → relay closes
2. Load has power throughout startup and calibration
3. Power cuts when firmware sets GPIO26 LOW (CUT state)
4. Button on GPIO32 sets GPIO26 HIGH again → relay closes → ACTIVE state
