# Step 3 — Circuit Diagram: Relay Control + Manual Button

> Builds on Step 1 wiring (ACS712 + ZMPT101B). Keep all existing connections.
> This file covers only the new additions: relay driver and manual button.

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
         │                 └──────────────────────────────►│ Relay coil (+) [NEW]
         │                                                  │
         │  GND ───────────┬──────────────────────────────►│ ACS712 GND
         │                 ├──────────────────────────────►│ ZMPT101B GND
         │                 ├──────────────────────────────►│ PC817 cathode  [NEW]
         │                 └──────────────────────────────►│ BC547 emitter  [NEW]
         │                                                  │
         │  GPIO34 ─────────────────── ACS712 VOUT (÷ divider)
         │  GPIO35 ─────────────────── ZMPT101B VOUT
         │  GPIO33 ─────────────────── Relay driver input   [NEW]
         │  GPIO32 ─────────────────── Manual button        [NEW]
         └──────────────────────────────────────────────────┘
```

---

## 2. Relay Driver Circuit

**Why not drive the relay directly from GPIO?**
ESP32 GPIO: 3.3V logic, 12mA max.
Relay coil: 5V, ~70mA. Direct connection would damage the ESP32.

**Solution chain:** GPIO33 → PC817 (optocoupler) → BC547 (NPN transistor) → relay coil

```
                                              5V (VIN)
                                                 │
                                          Relay coil (+)
                                                 │
                                          Relay coil (–)──────────┐
                                                 │                 │
                                           BC547 collector        [1N4007]
                                                 │              (flyback diode)
 GPIO33 ──[1kΩ]── PC817 ──[1kΩ]── BC547 base    │                 │
                    │              BC547 emitter  │         cathode─┘ (to 5V)
                   GND                  │         anode──────────────(to collector)
                                       GND
```

### Step-by-step connections

```
GPIO33
  │
 [1kΩ]                     ← limits LED current to ~2mA
  │
PC817 pin 1  (Anode)       ← current into LED turns on internal transistor
PC817 pin 2  (Cathode) ── GND
PC817 pin 4  (Collector)
  │
 [1kΩ]                     ← base current limiter for BC547
  │
BC547 pin 2  (Base)
BC547 pin 3  (Emitter) ── GND
BC547 pin 1  (Collector)
  │
  ├── Relay coil (–)
  │
  └── 1N4007 Anode         ← flyback diode — MANDATORY

Relay coil (+) ── 5V (ESP32 VIN)
1N4007 Cathode ── 5V (ESP32 VIN)   ← completes flyback path

Relay NO  ── Mains Live out  (to load socket)
Relay COM ── Mains Live in   (from ACS712 IP–)
Relay NC  ── leave unconnected
```

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
| ESP32 VIN | Relay coil (+) | Red |
| ESP32 VIN | 1N4007 Cathode | Red |
| ESP32 GND | ACS712 GND | Black |
| ESP32 GND | ZMPT101B GND | Black |
| ESP32 GND | PC817 pin 2 (Cathode) | Black |
| ESP32 GND | BC547 pin 3 (Emitter) | Black |
| ESP32 GPIO34 | ACS712 divider midpoint | Yellow |
| ESP32 GPIO35 | ZMPT101B VOUT | Yellow |
| ESP32 GPIO33 | 1kΩ → PC817 pin 1 | Orange |
| PC817 pin 4 (Collector) | 1kΩ → BC547 pin 2 (Base) | Blue |
| BC547 pin 1 (Collector) | Relay coil (–) | Blue |
| BC547 pin 1 (Collector) | 1N4007 Anode | Blue |
| Relay COM | Mains Live in (from ACS712) | Brown |
| Relay NO | Mains Live out (to load) | Brown |
| ESP32 GPIO32 | Button pin A | Green |
| Button pin B | GND | Black |
| ACS712 VOUT | 10kΩ top leg | Yellow |
| 10kΩ/22kΩ junction | GPIO34 | Yellow |
| 22kΩ bottom leg | GND | Black |

---

## 6. Component Pinouts

### PC817 Optocoupler (DIP-4, top view)

```
        ┌── notch ──┐
  Anode │1         4│ Collector   ┐ transistor
Cathode │2         3│ Emitter     ┘ side
        └───────────┘
         LED side

  pin 1 (Anode)    ← [1kΩ] ← GPIO33
  pin 2 (Cathode)  → GND
  pin 4 (Collector)→ [1kΩ] → BC547 Base
  pin 3 (Emitter)  → GND
```

### BC547 NPN Transistor (TO-92, flat face towards you)

```
   Flat face:
   ┌─────────┐
   │ C  B  E │
   │ 1  2  3 │
   └──┘ └──┘
      legs

  pin 1  Collector → Relay coil (–) and 1N4007 Anode
  pin 2  Base      ← [1kΩ] ← PC817 pin 4 (Collector)
  pin 3  Emitter   → GND
```

### SRD-05VDC-SL-C Relay (top view, pins facing you)

```
  ┌────────────────────────┐
  │  [COIL]   [CONTACTS]   │
  │   +   –   NO  COM  NC  │
  └────────────────────────┘

  Coil (+) → 5V
  Coil (–) → BC547 Collector
  NO       → Mains Live out (to load)
  COM      → Mains Live in  (from ACS712)
  NC       → not connected
```

### 1N4007 Flyback Diode

```
  Anode ──►|── Cathode
               (silver band = cathode)

  Anode   → BC547 Collector  (= Relay coil –)
  Cathode → 5V rail          (= Relay coil +)
```

Without this diode: relay coil generates a voltage spike when switched off
that can exceed 50V and instantly destroy the BC547 or the ESP32.

---

## 7. Logic Table

| GPIO33 | PC817 LED | BC547 | Relay | Load |
|--------|-----------|-------|-------|------|
| HIGH   | ON        | Saturated (conducting) | Energised (coil active) | ✅ Power ON |
| LOW    | OFF       | Cut off               | De-energised            | ❌ Power OFF |

**Boot sequence:**
1. `pinMode(RELAY_PIN, OUTPUT)` + `digitalWrite(RELAY_PIN, HIGH)` — relay closes before calibration runs
2. Load has power throughout startup and calibration
3. Power only cuts when firmware explicitly sets GPIO33 LOW (CUT state)
4. Button on GPIO32 sets GPIO33 HIGH again → relay closes → ACTIVE state
