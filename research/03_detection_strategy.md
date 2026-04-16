# 03 — Detection Strategy: Original Plan vs Revised Approach

## Original Plan

Measure precise standby wattage using ACS712. Classify device into three states based on thresholds:
- **ACTIVE**: Drawing normal operating power (e.g. >50W)
- **STANDBY**: Drawing small standby power (e.g. 2–15W)
- **CUT**: Timer expired while in standby → cut power via relay

The assumption was that we could detect a TV consuming 5W in standby vs 120W while actively playing.

---

## Why the Original Plan Failed

The ACS712-20A paired with the ESP32's onboard ADC has a noise floor of ~0.3–0.5A (~70–115W at 230V). This makes it physically impossible to distinguish between:
- A device drawing 5W standby
- A device turned off (0W)
- ADC noise

The standby measurement that the project name implies — detecting that a device is *still consuming power but shouldn't be* — is not achievable with this sensor+ADC combination.

### Why Upgrading to ACS712-5A Alone Doesn't Help
The 5A model has higher sensitivity (185mV/A vs 100mV/A), but the limiting factor is the ADC noise floor (~±50mV), not the sensor sensitivity. The improvement gets minimum detection down to ~30W — still far above any standby draw.

---

## Revised Strategy

Instead of measuring the standby wattage, detect the **transition from active load to near-zero load**:

```
State: ACTIVE
  └── Current drops below threshold (e.g. 0.4A / ~90W)
      └── State: IDLE (timer starts)
          ├── Current returns above threshold → back to ACTIVE (reset timer)
          └── Timer expires (e.g. 5 minutes)
              └── State: CUT (relay opens)
```

### Why This Still Works for the Use Case

The original problem being solved is: **devices left in standby indefinitely drain power**. The user turns off their TV, hair dryer, laptop, etc. — the device drops from active draw to near-zero. The system detects the drop and starts a timer. If the user doesn't turn it back on within N minutes, power is cut.

The system doesn't need to know the device is drawing 5W. It only needs to know the device **stopped drawing significant power**. That's detectable with ACS712.

### Limitation of Revised Strategy

Cannot distinguish between:
- Device in standby (drawing 5W ghost load) — power should be cut eventually
- Device fully unplugged from load socket — already zero, no issue

Both are treated identically. This is acceptable: both cases result in the relay opening, which is either beneficial (kills phantom load) or neutral (already unplugged).

Cannot handle devices that cycle between active and idle naturally (e.g. a refrigerator compressor). For this project scope, the target devices are consumer electronics and small appliances with clear ON/OFF behaviour.

---

## Future Upgrade Path for True Standby Detection

If the project scope expands to require true milliwatt-level standby detection, the sensor must be replaced. The ADC issue can also be bypassed with an external 16-bit ADC.

### Option A: HLW8012 Energy Metering IC (Recommended)
- Used in Sonoff POW and many commercial smart plugs
- Dedicated energy metering IC — measures V, I, power factor simultaneously
- Minimum detectable: ~0.1W
- Pulse output on two GPIO pins — ESP32 reads via interrupt, very low CPU overhead
- Cost: ~₹80
- Requires PCB redesign (not a drop-in module like ACS712)

### Option B: ACS712-5A + ADS1115 External ADC
- ADS1115: 16-bit I2C ADC, noise ~0.1mV vs ESP32's ~50mV
- Minimum detectable: ~2W
- Same ACS712 module, different ADC path
- Cost: ~₹180 additional
- I2C wiring: SDA→GPIO21, SCL→GPIO22 (same as OLED — shared bus)

### Option C: YHDC SCT-013 Split-Core CT Clamp
- Clamps around wire non-invasively (no breaking circuit required)
- Minimum detectable: ~1W with burden resistor
- Safer for retrofit into existing wiring
- Cost: ~₹250

---

## State Machine: Current Implementation (Phase 2)

```
         ┌─────────────────────────────┐
         │                             │
         ▼                             │ I >= threshold
┌──────────────┐   I < threshold   ┌──────────────┐
│    ACTIVE    │──────────────────►│     IDLE     │
└──────────────┘                   └──────┬───────┘
         ▲                                │
         │                                │ timer expires
         │                                ▼
         │                        ┌──────────────┐
         └────────────────────────│     CUT      │
           I >= threshold         └──────────────┘
           (Phase 3: button press  (relay open in Phase 3)
            restores power)
```

### Configurable Parameters
| Parameter | Current value | Purpose |
|---|---|---|
| `ACTIVE_THRESHOLD_A` | 0.40A | Below this = device considered idle |
| `IDLE_TIMEOUT_MIN` | 2 min (test) / 5–10 min (real) | Countdown before cut |

---

## Real-World Standby Power Reference

Documented for literature section — these are the loads the *intended* final design (with HLW8012 or equivalent) should detect:

| Device | Active power | Standby power | Ideal threshold |
|---|---|---|---|
| LED TV 40" | 80–120W | 0.5–5W | ~15W |
| Set-top box | 15–20W | 8–15W | ~8W |
| Laptop charger (charging) | 45–65W | 2–5W (idle) | ~10W |
| Hair dryer | 1000–1800W | 0W (mechanical off) | ~50W |
| Washing machine | 500W | 2–5W (standby) | ~20W |
| Microwave | 1200W | 2–5W (clock) | ~20W |
| Phone charger (charging) | 5–18W | 0.1–2W | ~3W |

**Indian context:** Average Indian household has 5–8 devices in standby simultaneously, consuming 20–60W continuously. At ₹8/kWh, 40W standby = ~₹2,800/year wasted.
