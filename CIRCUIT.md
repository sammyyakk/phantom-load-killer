# Phantom Load Killer — Complete Circuit Diagram

All components, all connections, one document.

---

## Full System Pin Map

```
                         5V USB Adapter
                              |
                         [Micro USB]
                              |
         +--------------------+----------------------------+
         |                 ESP32 DevKit V1                  |
         |                                                  |
         |  VIN (5V out) --+------------------------------>| ACS712 VCC
         |                 +------------------------------>| ZMPT101B VCC
         |                 +------------------------------>| Relay module VCC
         |                                                  |
         |  3V3 ------------------------------------------>| OLED VCC
         |                                                  |
         |  GND -----------+------------------------------>| ACS712 GND
         |                 +------------------------------>| ZMPT101B GND
         |                 +------------------------------>| Relay module GND
         |                 +------------------------------>| OLED GND
         |                 +------------------------------>| BC547 Emitter
         |                 +------------------------------>| RGB LED cathode
         |                                                  |
         |  GPIO34 ------------------- ACS712 VOUT (through voltage divider)
         |  GPIO35 ------------------- ZMPT101B VOUT (direct)
         |  GPIO26 --[1kR]---> BC547 Base -> Collector -> Relay IN
         |  GPIO32 ------------------- Manual button -> GND
         |  GPIO21 (SDA) ------------- OLED SDA
         |  GPIO22 (SCL) ------------- OLED SCL
         |  GPIO23 --[330R]----------- RGB LED Red pin
         |  GPIO18 --[330R]----------- RGB LED Green pin
         |  GPIO19 --[330R]----------- RGB LED Blue pin
         +--------------------------------------------------+
```

---

## 1. ACS712-20A — Current Sensor

```
                      ACS712-20A Module
                    +---------------------+
    AC Load wire -->| IP+              VCC |<--- 5V (ESP32 VIN)
    AC Load wire <--| IP-              GND |<--- GND
                    |                 VOUT |--+
                    +---------------------+   |
                                              |
                              Voltage Divider (scales 4.5V -> 3.09V)
                                              |
                                            [10kR]
                                              |
                                              +---------> GPIO34
                                              |
                                            [22kR]
                                              |
                                             GND
```

**Why the divider:** ACS712 runs on 5V, peak output = 4.5V. ESP32 ADC max = 3.3V.
Divider scales: 4.5 x 22/(10+22) = **3.09V**. Without this, the ESP32 is damaged.

---

## 2. ZMPT101B — AC Voltage Sensor

```
                      ZMPT101B Module
                    +---------------------+
    AC Live (L) --->| L                VCC |<--- 5V (ESP32 VIN)
    AC Neutral (N)->| N                GND |<--- GND
                    |                 VOUT |---------> GPIO35 (direct)
                    +---------------------+
```

No divider needed — the module's onboard op-amp limits output to 3.3V swing.

The blue trimmer pot on the module adjusts the output amplitude. Turn it to get
voltage readings in the right ballpark, then fine-tune `ZMPT_VFACTOR` in firmware.

---

## 3. Relay Module + BC547 Buffer

The relay module has an active-LOW input. The ESP32 clone cannot sink enough current
to drive it directly, so a BC547 NPN transistor is used as a buffer.

```
  ESP32 GPIO26 --[1kR]--> BC547 Base (middle pin)
                          BC547 Emitter (right pin, flat side facing you) --> GND
                          BC547 Collector (left pin) --> Relay module IN

  Relay Module
  +--------------+
  | VCC  GND  IN |  <-- control side
  |              |
  | NO  COM  NC  |  <-- AC switching side
  +--------------+

  VCC --> ESP32 VIN (5V)
  GND --> ESP32 GND
  IN  --> BC547 Collector
```

**Logic (from ESP32's perspective):**
- GPIO26 HIGH → BC547 ON → Relay IN pulled LOW → relay energises → **load ON**
- GPIO26 LOW  → BC547 OFF → Relay IN floats HIGH → relay off → **load CUT**

### Relay AC side wiring

```
  NO  --> Mains Live out (to load socket)
  COM --> Mains Live in  (from ACS712 IP-)
  NC  --> leave unconnected
```

---

## 4. Manual Restore Button

```
  ESP32 GPIO32 --------+---- [BUTTON] ---- GND
                        |
                  (INPUT_PULLUP: internal ~45kR to 3.3V)

  Not pressed: GPIO32 reads HIGH
  Pressed:     GPIO32 reads LOW  (button shorts to GND)
```

Two wires only — GPIO32 and GND. No external resistor needed.

---

## 5. SSD1306 OLED Display (I2C, 128x64)

```
  OLED Module
  +------------------+
  |  VCC SDA SCL GND |
  +---+---+---+------+
      |   |   |   |
     3V3 GPIO21 GPIO22 GND
     (NOT 5V!)
```

| OLED Pin | ESP32 Pin | Notes |
|---|---|---|
| VCC | 3V3 | **Must be 3.3V** — 5V will damage the display |
| SDA | GPIO21 | Default I2C data |
| SCL | GPIO22 | Default I2C clock |
| GND | GND | |

I2C address: **0x3C** (most common for 0.96" modules).

---

## 6. RGB LED (Common Cathode)

```
  RGB LED pinout (flat side facing you, legs down):

     Red  Cathode  Green  Blue
      |     |       |      |
      |   (long)    |      |
      |    pin      |      |
    [330R]  |     [330R] [330R]
      |     |       |      |
   GPIO23  GND   GPIO18  GPIO19
```

- Longest pin = cathode → connects to ESP32 GND
- Each colour pin gets a **330R** current-limiting resistor
- Driven by ESP32 LEDC PWM (5kHz, 8-bit resolution)

| State | Colour | Pattern |
|---|---|---|
| ACTIVE | Green | Solid |
| IDLE | Amber (R+G) | Breathing (sine wave) |
| CUT | Red | Solid |

---

## 7. Mains Wiring

```
  !! DANGER — 220/240V AC. Disconnect from wall before touching any wire !!

  Wall Socket
    |
    +-- LIVE --[5A Fuse]--[MOV]-- ACS712 IP+ -- ACS712 IP- -- Relay COM
    |                                                          Relay NO -- Load LIVE
    |
    +-- LIVE --------------------------------- ZMPT101B terminal L
    |
    +-- NEUTRAL ----------------------------- ZMPT101B terminal N
    +-- NEUTRAL ----------------------------- Load NEUTRAL (direct, no sensor)
```

**Rules:**
- Fuse goes on Live wire first, before anything else
- MOV (Metal Oxide Varistor) clamps voltage spikes between Live and Neutral
- Relay switches Live wire only — never switch Neutral
- Use 1.5mm² mains-rated wire for all HV connections
- Screw terminals or crimped connectors only — no twisted bare joints
- Heat shrink all exposed mains conductors
- Always test with a GFCI/RCD extension cord

---

## Complete Connection Table

| # | From | To | Wire | Notes |
|---|---|---|---|---|
| 1 | ESP32 VIN | ACS712 VCC | Red | 5V power |
| 2 | ESP32 VIN | ZMPT101B VCC | Red | 5V power |
| 3 | ESP32 VIN | Relay module VCC | Red | 5V power |
| 4 | ESP32 3V3 | OLED VCC | Orange | 3.3V only |
| 5 | ESP32 GND | ACS712 GND | Black | |
| 6 | ESP32 GND | ZMPT101B GND | Black | |
| 7 | ESP32 GND | Relay module GND | Black | |
| 8 | ESP32 GND | OLED GND | Black | |
| 9 | ESP32 GND | BC547 Emitter | Black | Pin 3 (flat side right) |
| 10 | ESP32 GND | RGB LED cathode | Black | Longest leg |
| 11 | ESP32 GND | Button pin B | Black | |
| 12 | ACS712 VOUT | 10kR top | Yellow | Voltage divider |
| 13 | 10kR / 22kR junction | GPIO34 | Yellow | ADC input |
| 14 | 22kR bottom | GND | Black | |
| 15 | ZMPT101B VOUT | GPIO35 | Yellow | Direct, no divider |
| 16 | GPIO26 | 1kR leg A | Orange | Relay driver |
| 17 | 1kR leg B | BC547 Base | Orange | Pin 2 (middle) |
| 18 | BC547 Collector | Relay IN | Orange | Pin 1 (flat side left) |
| 19 | GPIO32 | Button pin A | Green | Internal pull-up |
| 20 | GPIO21 | OLED SDA | Blue | I2C data |
| 21 | GPIO22 | OLED SCL | Blue | I2C clock |
| 22 | GPIO23 | 330R → RGB Red | Red | PWM ch 0 |
| 23 | GPIO18 | 330R → RGB Green | Green | PWM ch 1 |
| 24 | GPIO19 | 330R → RGB Blue | Blue | PWM ch 2 |

**Total connections: 24 wires** (low-voltage side only, excluding mains wiring).

---

## Pin Summary

| GPIO | Direction | Function |
|---|---|---|
| 34 | Input (ADC1_CH6) | ACS712 current sensor (via divider) |
| 35 | Input (ADC1_CH7) | ZMPT101B voltage sensor (direct) |
| 26 | Output | Relay driver via BC547 (HIGH = ON) |
| 32 | Input (pull-up) | Manual restore button (LOW = pressed) |
| 21 | I2C SDA | OLED display data |
| 22 | I2C SCL | OLED display clock |
| 23 | PWM output | RGB LED Red (LEDC ch 0) |
| 18 | PWM output | RGB LED Green (LEDC ch 1) |
| 19 | PWM output | RGB LED Blue (LEDC ch 2) |

> GPIO34 and GPIO35 are input-only pins on ADC1 — safe to use when WiFi is active (ADC2 is blocked by WiFi).
