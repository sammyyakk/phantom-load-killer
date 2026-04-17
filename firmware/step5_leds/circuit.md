# Step 5 — Circuit Diagram: RGB Status LED

> Builds on Step 4 wiring. Keep ALL existing connections (ACS712, ZMPT101B,
> relay module with BC547 buffer, manual button, SSD1306 OLED).
> Adds a single 4-pin common-cathode RGB LED for at-a-glance status.

---

## 1. Full System Pin Map

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
         |                 +------------------------------>| RGB LED cathode [NEW]
         |                                                  |
         |  GPIO34 ------------------- ACS712 VOUT (/ divider)
         |  GPIO35 ------------------- ZMPT101B VOUT
         |  GPIO26 --[1kR]---> BC547 Base -> Collector -> Relay IN
         |  GPIO32 ------------------- Manual button
         |  GPIO21 (SDA) ------------- OLED SDA
         |  GPIO22 (SCL) ------------- OLED SCL
         |  GPIO23 --[330R]----------- RGB LED Red pin        [NEW]
         |  GPIO18 --[330R]----------- RGB LED Green pin      [NEW]
         |  GPIO19 --[330R]----------- RGB LED Blue pin       [NEW]
         +--------------------------------------------------+
```

---

## 2. RGB LED Wiring

Standard 4-pin common-cathode RGB LED. The longest pin is the cathode (GND).

```
  RGB LED pinout (flat side facing you, legs down):

     R   K   G   B
     |   |   |   |
     |  GND  |   |
     |(long) |   |
     |       |   |
   [330R] [GND] [330R] [330R]
     |       |     |      |
   GPIO23  ESP32  GPIO18  GPIO19
           GND
```

- Each colour pin needs a **330R current-limiting resistor** (150R-330R works)
- The longest pin (cathode) connects directly to ESP32 GND
- ESP32 PWM drives each colour independently — mix any colour

> **Common anode?** If your LED lights up when you connect a colour pin to GND
> instead of 3.3V, it's common anode. In that case: connect the long pin to
> 3.3V, and tell me — I'll flip the PWM logic in firmware (255 - value).

---

## 3. LED Colour Assignments

| State | Colour | Behaviour |
|-------|--------|-----------|
| ACTIVE | Green | Solid — device drawing power |
| IDLE | Yellow/Amber | Breathing — countdown running |
| CUT | Red | Solid — relay open, power cut |

During IDLE, the LED breathes yellow (fades in/out) to show the timer
is ticking. Brightness is kept moderate to avoid blinding in dark rooms.

---

## 4. Full Connection Table

| From | To | Colour | Notes |
|---|---|---|---|
| ESP32 VIN | ACS712 VCC | Red | 5V power |
| ESP32 VIN | ZMPT101B VCC | Red | 5V power |
| ESP32 VIN | Relay module VCC | Red | 5V power |
| ESP32 3V3 | OLED VCC | Orange | 3.3V |
| ESP32 GND | ACS712 GND | Black | |
| ESP32 GND | ZMPT101B GND | Black | |
| ESP32 GND | Relay module GND | Black | |
| ESP32 GND | OLED GND | Black | |
| ESP32 GND | BC547 Emitter | Black | |
| ESP32 GND | RGB LED cathode (long) | Black | Common cathode |
| ESP32 GPIO34 | ACS712 divider mid | Yellow | Analog in |
| ESP32 GPIO35 | ZMPT101B VOUT | Yellow | Analog in |
| ESP32 GPIO26 | 1kR -> BC547 Base | Green | Relay drive |
| BC547 Collector | Relay module IN | Green | |
| ESP32 GPIO21 | OLED SDA | Blue | I2C data |
| ESP32 GPIO22 | OLED SCL | White | I2C clock |
| ESP32 GPIO23 | 330R -> RGB LED Red | Red | PWM channel 0 |
| ESP32 GPIO18 | 330R -> RGB LED Green | Green | PWM channel 1 |
| ESP32 GPIO19 | 330R -> RGB LED Blue | Blue | PWM channel 2 |
| ESP32 GPIO32 | Button -> GND | Any | Internal pull-up |

---

## 5. Library Dependency

None — uses ESP32 built-in LEDC PWM hardware. No external library needed.
