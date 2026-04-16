/*
 * Phantom Load Killer — Step 2: Detection State Machine
 * =======================================================
 * Builds on Step 1. Same wiring, same pins.
 * NO RELAY yet — just detects state and prints to Serial.
 *
 * Strategy (revised from Phase 1 findings):
 *   ACS712-20A cannot detect low standby draws (<0.3A / ~70W).
 *   Instead: detect when current drops below ACTIVE_THRESHOLD_A,
 *   start a countdown. If it stays low for IDLE_TIMEOUT_MIN → CUT.
 *   Any spike above threshold resets the timer back to ACTIVE.
 *
 * States:
 *   ACTIVE  — device drawing significant power (above threshold)
 *   IDLE    — power dropped, countdown timer running
 *   CUT     — timer expired, would cut power here (relay in Phase 3)
 *
 * Pins:
 *   GPIO34 — ACS712 output (10kΩ/22kΩ divider)
 *   GPIO35 — ZMPT101B output
 */

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define ACS712_PIN    34
#define ZMPT101B_PIN  35

// ── Calibration (from Phase 1) ────────────────────────────────────────────────
const float ZMPT_VFACTOR      = 0.428f;   // Calibrated to this specific module
const float ACS712_SENSITIVITY = 85.4f;   // ADC counts per Amp

// ── Detection Thresholds ──────────────────────────────────────────────────────
const float ACTIVE_THRESHOLD_A = 0.40f;   // Above this = device is ON (tune for your device)
const int   IDLE_TIMEOUT_MIN   = 2;       // Minutes of low current before CUT (use 2 for testing)

// ── Sampling ──────────────────────────────────────────────────────────────────
const int SAMPLES = 1000;

// ── State Machine ─────────────────────────────────────────────────────────────
enum DeviceState { ACTIVE, IDLE, CUT };
DeviceState state         = ACTIVE;
unsigned long idleStartMs = 0;   // millis() when current first dropped below threshold

// ── Zero offsets (set by calibrateZero) ──────────────────────────────────────
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
    delay(500);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.print("\r\n============================================\r\n");
    Serial.print(" Phantom Load Killer — Step 2: Detection  \r\n");
    Serial.print("============================================\r\n");
    Serial.printf(" Active threshold : %.2f A\r\n", ACTIVE_THRESHOLD_A);
    Serial.printf(" Idle timeout     : %d min\r\n", IDLE_TIMEOUT_MIN);
    Serial.print("\r\nCalibrating (ensure no load, wire connected)...\r\n");
    delay(1000);
    calibrateZero();

    Serial.print("\r\n  State   |  Voltage  |  Current  |  Power   |  Idle timer\r\n");
    Serial.print("  --------|-----------|-----------|----------|------------\r\n");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    float V = measureVoltageRMS();
    float I = measureCurrentRMS();
    float W = V * I;

    DeviceState prevState = state;

    // ── State machine transitions ──────────────────────────────────────────
    switch (state) {

        case ACTIVE:
            if (I < ACTIVE_THRESHOLD_A) {
                // Current dropped — start idle timer
                state       = IDLE;
                idleStartMs = millis();
            }
            break;

        case IDLE:
            if (I >= ACTIVE_THRESHOLD_A) {
                // Current came back — device active again
                state = ACTIVE;
            } else {
                // Still idle — check if timeout expired
                unsigned long elapsedMs  = millis() - idleStartMs;
                unsigned long timeoutMs  = (unsigned long)IDLE_TIMEOUT_MIN * 60UL * 1000UL;
                if (elapsedMs >= timeoutMs) {
                    state = CUT;
                }
            }
            break;

        case CUT:
            // Relay would be open here. In Phase 3 we'll actually cut power.
            // For now just stay in CUT until current is detected (manual reset simulation).
            if (I >= ACTIVE_THRESHOLD_A) {
                // Device somehow drawing power again (manual toggle / phase 3 button)
                state = ACTIVE;
            }
            break;
    }

    // ── Print state change notification ───────────────────────────────────
    if (state != prevState) {
        Serial.printf("\r\n>>> STATE CHANGE: %s -> %s\r\n\r\n",
                      stateName(prevState), stateName(state));
    }

    // ── Print live reading ─────────────────────────────────────────────────
    char timerBuf[16] = "  --      ";
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_MIN * 60UL;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        snprintf(timerBuf, sizeof(timerBuf), "%2lum %02lus left", remaining / 60, remaining % 60);
    } else if (state == CUT) {
        snprintf(timerBuf, sizeof(timerBuf), "  EXPIRED ");
    }

    Serial.printf("  %s |  %5.1f V  |  %5.3f A  |  %5.1f W  |  %s\r\n",
                  stateName(state), V, I, W, timerBuf);

    delay(500);
}
