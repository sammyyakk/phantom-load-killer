# Step 6 — WiFi Dashboard

> **No new wiring** — this step adds a WiFi web dashboard using the ESP32's
> built-in WiFi. All previous connections (ACS712, ZMPT101B, relay + BC547,
> button, OLED, RGB LED) remain exactly the same.

---

## 1. What's New

| Feature | Details |
|---------|---------|
| WiFi mode | Access Point (AP) — no router needed |
| SSID | `PhantomKiller` |
| Password | `phantom123` |
| Dashboard URL | `http://192.168.4.1` |
| Live updates | Auto-refresh every 2 seconds via fetch API |
| Relay control | ON / OFF buttons on the dashboard |
| Auto-mode toggle | Enable/disable automatic idle detection from dashboard |

---

## 2. How to Connect

1. Flash the firmware
2. On your phone or laptop, connect to WiFi **PhantomKiller** (password: `phantom123`)
3. Open a browser and go to **http://192.168.4.1**
4. The dashboard shows live voltage, current, power, state, and relay controls

---

## 3. Dashboard Features

- **Live readings** — Voltage (V), Current (A), Power (W) update every 2s
- **State indicator** — Shows ACTIVE / IDLE / CUT with colour-coded badge
- **Relay controls** — Manual ON and OFF buttons to override the relay
- **Auto mode toggle** — Disable auto-detection to keep relay in manual control
- **Uptime** — Shows how long the system has been running
- **Responsive** — Works on mobile and desktop browsers

---

## 4. Pin Map (unchanged from Step 5)

```
         +--------------------+----------------------------+
         |                 ESP32 DevKit V1                  |
         |                                                  |
         |  GPIO34 -------- ACS712 VOUT (voltage divider)   |
         |  GPIO35 -------- ZMPT101B VOUT                   |
         |  GPIO26 --[1kR]-- BC547 Base -> Relay IN         |
         |  GPIO32 -------- Manual button (pull-up)         |
         |  GPIO21 (SDA) --- OLED SDA                       |
         |  GPIO22 (SCL) --- OLED SCL                       |
         |  GPIO23 --[330R]-- RGB LED Red                   |
         |  GPIO18 --[330R]-- RGB LED Green                 |
         |  GPIO19 --[330R]-- RGB LED Blue                  |
         +--------------------------------------------------+
```

---

## 5. Libraries Required

Same as Step 5 — no new libraries:
- Adafruit SSD1306
- Adafruit GFX Library
- WiFi (built-in with ESP32 core)
- WebServer (built-in with ESP32 core)
