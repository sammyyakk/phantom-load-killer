/*
 * Phantom Load Killer — Step 3: Relay Control + Manual Button
 * =============================================================
 * Adds over Step 2:
 *   - Relay physically cuts the mains Live wire on CUT state
 *   - Manual button restores power from CUT state
 *   - Safe relay init: relay ON (closed) at startup before calibration
 *
 * Wiring (new in this step):
 *   GPIO33 → [1kΩ] → PC817 LED anode
 *            PC817 LED cathode → GND
 *            PC817 transistor collector → [1kΩ] → BC547 base
 *            BC547 emitter → GND
 *            BC547 collector → Relay coil (–)
 *            Relay coil (+) → 5V (VIN pin)
 *            1N4007 across relay coil (cathode to 5V)
 *            Relay NO → Load Live out
 *            Relay COM → Live in (from ACS712)
 *
 *   GPIO32 → Tactile button → GND   (pull-up enabled internally)
 *            Press while in CUT → restores power (ACTIVE state)
 *
 * Relay logic:
 *   RELAY_ON  (HIGH) = relay energised = coil active = COM→NO closed = power flows
 *   RELAY_OFF (LOW)  = relay de-energised                             = power cut
 *
 * Pins (all):
 *   GPIO34 — ACS712 output (10kΩ/22kΩ divider)
 *   GPIO35 — ZMPT101B output
 *   GPIO33 — Relay control (through PC817 + BC547)
 *   GPIO32 — Manual restore button (active LOW, internal pull-up)
 */

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define ACS712_PIN    34
#define ZMPT101B_PIN  35
#define RELAY_PIN     33
#define BUTTON_PIN    32

// ── Relay helpers ─────────────────────────────────────────────────────────────
#define RELAY_ON  HIGH   // Relay energised → COM-NO closed → power flows
#define RELAY_OFF LOW    // Relay de-energised → COM-NO open → power cut

// ── Calibration (from Phase 1) ────────────────────────────────────────────────
const float ZMPT_VFACTOR       = 0.428f;   // Calibrated to this specific module
const float ACS712_SENSITIVITY  = 85.4f;   // ADC counts per Amp

// ── Detection Thresholds ──────────────────────────────────────────────────────
const float ACTIVE_THRESHOLD_A  = 0.40f;   // Above this = device is ON
const int   IDLE_TIMEOUT_MIN    = 2;       // Minutes idle before CUT (use 2 for testing)

// ── Button debounce ───────────────────────────────────────────────────────────
const unsigned long DEBOUNCE_MS = 50;

// ── Sampling ──────────────────────────────────────────────────────────────────
const int SAMPLES = 1000;

// ── State Machine ─────────────────────────────────────────────────────────────
enum DeviceState { ACTIVE, IDLE, CUT };
DeviceState state         = ACTIVE;
unsigned long idleStartMs = 0;

// ── Button debounce state ─────────────────────────────────────────────────────
bool          lastButtonRaw   = HIGH;   // physical pin state last loop
unsigned long lastDebounceMs  = 0;
bool          buttonPressed   = false;  // single-shot flag per press

// ── Zero offsets ──────────────────────────────────────────────────────────────
float acs712_zero = 2133.0f;
float zmpt_zero   = 2048.0f;

// ─────────────────────────────────────────────────────────────────────────────
const char* stateName(DeviceState s) {
    switch (s) {
        case ACTIVE: return "ACTIVE";
        case IDLE:   return "IDLE  ";
        case CUT:    return "CUT   ";
    }
    return "?";
}

// ─────────────────────────────────────────────────────────────────────────────
void setRelay(bool on) {
    digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

// ── Button: returns true once per physical press (debounced) ──────────────────
bool buttonWasPressed() {
    bool reading = digitalRead(BUTTON_PIN);   // LOW when pressed (pull-up)

    if (reading != lastButtonRaw) {
        lastDebounceMs = millis();
        lastButtonRaw  = reading;
    }

    if ((millis() - lastDebounceMs) > DEBOUNCE_MS) {
        // Stable reading — fire on falling edge (HIGH→LOW = press)
        if (reading == LOW && !buttonPressed) {
            buttonPressed = true;
            return true;
        }
        if (reading == HIGH) {
            buttonPressed = false;   // Released — ready for next press
        }
    }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
void calibrateZero() {
    const int CAL_SAMPLES = 2000;
    long acs_sum = 0, zmpt_sum = 0;
    for (int i = 0; i < CAL_SAMPLES; i++) {
        acs_sum  += analogRead(ACS712_PIN);
        zmpt_sum += analogRead(ZMPT101B_PIN);
        delayMicroseconds(100);
    }
    acs712_zero = (float)acs_sum  / CAL_SAMPLES;
    zmpt_zero   = (float)zmpt_sum / CAL_SAMPLES;
    Serial.printf("[CAL] ACS712 zero=%.1f  ZMPT zero=%.1f\r\n", acs712_zero, zmpt_zero);
}

// ─────────────────────────────────────────────────────────────────────────────
float measureCurrentRMS() {
    double sum = 0.0;
    for (int i = 0; i < SAMPLES; i++) {
        float raw  = (float)analogRead(ACS712_PIN) - acs712_zero;
        float amps = raw / ACS712_SENSITIVITY;
        sum += (double)amps * amps;
        delayMicroseconds(200);
    }
    float result = sqrtf((float)(sum / SAMPLES));
    return (result < 0.05f) ? 0.0f : result;
}

// ─────────────────────────────────────────────────────────────────────────────
float measureVoltageRMS() {
    double sum = 0.0;
    for (int i = 0; i < SAMPLES; i++) {
        float raw = (float)analogRead(ZMPT101B_PIN) - zmpt_zero;
        sum += (double)raw * raw;
        delayMicroseconds(200);
    }
    return sqrtf((float)(sum / SAMPLES)) * ZMPT_VFACTOR;
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    // Relay first — power stays ON during boot
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(true);   // Close relay immediately — load has power

    // Button with internal pull-up
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.print("\r\n============================================\r\n");
    Serial.print(" Phantom Load Killer — Step 3: Relay      \r\n");
    Serial.print("============================================\r\n");
    Serial.printf(" Active threshold : %.2f A\r\n", ACTIVE_THRESHOLD_A);
    Serial.printf(" Idle timeout     : %d min\r\n", IDLE_TIMEOUT_MIN);
    Serial.print(" Relay            : CLOSED (power on)\r\n");
    Serial.print("\r\nCalibrating (ensure no load, wire connected)...\r\n");
    delay(1000);
    calibrateZero();

    Serial.print("\r\n  State   |  Voltage  |  Current  |  Power   |  Timer / Status\r\n");
    Serial.print("  --------|-----------|-----------|----------|------------------\r\n");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    float V = measureVoltageRMS();
    float I = measureCurrentRMS();
    float W = V * I;

    DeviceState prevState = state;

    // ── Manual button: restore from CUT only ──────────────────────────────
    if (state == CUT && buttonWasPressed()) {
        setRelay(true);
        state = ACTIVE;
        Serial.print("\r\n>>> BUTTON PRESSED — Power restored\r\n\r\n");
    }

    // ── State machine transitions ──────────────────────────────────────────
    switch (state) {

        case ACTIVE:
            if (I < ACTIVE_THRESHOLD_A) {
                state       = IDLE;
                idleStartMs = millis();
            }
            break;

        case IDLE:
            if (I >= ACTIVE_THRESHOLD_A) {
                state = ACTIVE;
            } else {
                unsigned long elapsedMs = millis() - idleStartMs;
                unsigned long timeoutMs = (unsigned long)IDLE_TIMEOUT_MIN * 60UL * 1000UL;
                if (elapsedMs >= timeoutMs) {
                    state = CUT;
                    setRelay(false);   // ← PHYSICALLY CUT POWER
                }
            }
            break;

        case CUT:
            // Relay is open. Stay here until button press (handled above).
            // Guard: if somehow current flows (relay contacts welded?), re-cut.
            if (I >= ACTIVE_THRESHOLD_A) {
                setRelay(false);
            }
            break;
    }

    // ── State change notification ──────────────────────────────────────────
    if (state != prevState) {
        const char* relayStr = (state == CUT) ? "RELAY OPEN — power cut" : "relay closed";
        Serial.printf("\r\n>>> STATE: %s -> %s  [%s]\r\n\r\n",
                      stateName(prevState), stateName(state), relayStr);
    }

    // ── Print live reading ─────────────────────────────────────────────────
    char statusBuf[20] = "               ";
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_MIN * 60UL;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        snprintf(statusBuf, sizeof(statusBuf), "%2lum %02lus left", remaining / 60, remaining % 60);
    } else if (state == CUT) {
        snprintf(statusBuf, sizeof(statusBuf), "btn to restore  ");
    } else {
        snprintf(statusBuf, sizeof(statusBuf), "               ");
    }

    Serial.printf("  %s |  %5.1f V  |  %5.3f A  |  %5.1f W  |  %s\r\n",
                  stateName(state), V, I, W, statusBuf);

    delay(500);
}
