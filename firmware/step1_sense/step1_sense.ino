/*
 * Phantom Load Killer — Step 1: Current & Voltage Sensing
 * =========================================================
 * Hardware:
 *   - ESP32 DevKit V1 (30-pin)
 *   - ACS712-20A  → voltage divider (10kΩ top / 22kΩ bottom) → GPIO34
 *   - ZMPT101B    → directly → GPIO35
 *
 * Both GPIO34 and GPIO35 are input-only, ADC1 channels — safe choices
 * since ADC2 is unusable when WiFi is active.
 *
 * Power: ESP32 via USB adapter. Sensors powered from DevKit 5V/VIN pin.
 *
 * Serial: 115200 baud. Open Serial Monitor after flashing.
 */

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define ACS712_PIN    34    // ADC1_CH6 — after 10kΩ/22kΩ divider
#define ZMPT101B_PIN  35    // ADC1_CH7 — AC voltage sensor direct

// ── ACS712-20A Constants ──────────────────────────────────────────────────────
//
//  Vcc = 5V, sensitivity = 100 mV/A
//  Voltage divider ratio = 22k / (10k + 22k) = 0.6875
//
//  Zero-current voltage at ACS712 output : 2.500 V
//  Zero-current voltage after divider    : 2.500 × 0.6875 = 1.7188 V
//  Zero-current ADC count (12-bit, 3.3V) : 1.7188 / 3.3 × 4095 ≈ 2133
//
//  Sensitivity after divider : 100 mV/A × 0.6875 = 68.75 mV/A
//  Counts per Amp            : 68.75 / 3300 × 4095 ≈ 85.4
//
//  These are theoretical — run calibrateZero() at startup to measure
//  the real zero, then tune ACS712_SENSITIVITY with a known load.
//
const float ACS712_SENSITIVITY = 85.4f;    // ADC counts per Amp — tune this

// ── ZMPT101B Constants ────────────────────────────────────────────────────────
//
//  The ZMPT101B outputs a scaled AC sine wave centred at ~VCC/2.
//  Since the module runs on 5V but feeds into the ESP32 at 3.3V directly,
//  the on-board op-amp limits the output swing to stay within ADC range.
//  The centre (DC bias) sits at roughly ADC mid-scale (~2048).
//
//  ZMPT_VFACTOR converts ADC RMS counts → real Vrms.
//  Start with 0.5 — then hold a known stable voltage (e.g. UPS / variac)
//  measure with a multimeter, and adjust until the Serial output matches.
//
//  Example: Serial shows 340 V, multimeter shows 230 V → factor = 0.5 × (230/340)
//
const float ZMPT_VFACTOR = 0.428f;         // Calibrated: 245V actual / 286V raw × 0.5

// ── Sampling Configuration ────────────────────────────────────────────────────
//
//  At 200 µs per sample → ~5000 samples/sec.
//  1000 samples = 200 ms window = 10 complete 50 Hz cycles. Good RMS accuracy.
//
const int   SAMPLES = 1000;

// ── Runtime zero offsets (set by calibrateZero at startup) ───────────────────
float acs712_zero = 2133.0f;
float zmpt_zero   = 2048.0f;

// ─────────────────────────────────────────────────────────────────────────────
//  calibrateZero()
//  Call once at startup with NO load connected and NO mains on ZMPT101B.
//  Measures actual ADC mid-point for both sensors to remove DC bias error.
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

    Serial.printf("[CAL] ACS712 zero = %.1f  |  ZMPT101B zero = %.1f\r\n",
                  acs712_zero, zmpt_zero);
}

// ─────────────────────────────────────────────────────────────────────────────
//  measureCurrentRMS()
//  Samples ACS712, subtracts DC zero, computes true RMS current in Amps.
//  Noise floor: values below 0.05 A are clamped to 0.
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
    return (result < 0.05f) ? 0.0f : result;   // noise floor clamp
}

// ─────────────────────────────────────────────────────────────────────────────
//  measureVoltageRMS()
//  Samples ZMPT101B, subtracts DC centre, computes Vrms using ZMPT_VFACTOR.
// ─────────────────────────────────────────────────────────────────────────────
float measureVoltageRMS() {
    double sum = 0.0;

    for (int i = 0; i < SAMPLES; i++) {
        float raw = (float)analogRead(ZMPT101B_PIN) - zmpt_zero;
        sum += (double)raw * raw;
        delayMicroseconds(200);
    }

    float rms_counts = sqrtf((float)(sum / SAMPLES));
    return rms_counts * ZMPT_VFACTOR;
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500);

    // 12-bit ADC resolution (0–4095), full 0–3.3V range
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.print("\r\n========================================\r\n");
    Serial.print(" Phantom Load Killer — Step 1: Sensing\r\n");
    Serial.print("========================================\r\n");
    Serial.print("Ensure: no load connected, ZMPT101B NOT on mains yet.\r\n");
    Serial.print("Calibrating zero offsets...\r\n");
    delay(1000);

    calibrateZero();

    Serial.print("\r\nCalibration done.\r\n");
    Serial.print("You can now connect ZMPT101B to mains and attach a load.\r\n\r\n");
    Serial.print("  Voltage (V)  |  Current (A)  |  Power (W)\r\n");
    Serial.print("  -----------  |  -----------  |  ---------\r\n");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    float V = measureVoltageRMS();
    float I = measureCurrentRMS();
    float W = V * I;

    // DEBUG: print raw ADC value so we can verify the sensor is responding
    int raw = analogRead(ACS712_PIN);
    Serial.printf("  %8.1f V  |  %8.3f A  |  %8.1f W  |  raw=%d  zero=%.0f\r\n",
                  V, I, W, raw, acs712_zero);

    delay(500);   // print twice per second
}
