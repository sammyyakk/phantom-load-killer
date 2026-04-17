/*
 * Phantom Load Killer — Step 4: OLED Display
 * =============================================================
 * Adds over Step 3:
 *   - 0.96" SSD1306 128×64 I2C OLED showing live V/I/W, state,
 *     countdown timer, and a progress bar during IDLE
 *
 * Wiring (new in this step):
 *   OLED SDA  → GPIO21
 *   OLED SCL  → GPIO22
 *   OLED VCC  → ESP32 3V3  (NOT VIN)
 *   OLED GND  → ESP32 GND
 *
 * All previous wiring unchanged:
 *   GPIO34 — ACS712 output (10kΩ/22kΩ divider)
 *   GPIO35 — ZMPT101B output
 *   GPIO26 — Relay module IN (via BC547 buffer)
 *   GPIO32 — Manual restore button (active LOW, internal pull-up)
 *
 * Libraries required:
 *   Adafruit SSD1306, Adafruit GFX Library
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── OLED Setup ───────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define ACS712_PIN    34
#define ZMPT101B_PIN  35
#define RELAY_PIN     26
#define BUTTON_PIN    32

// ── Relay helpers (BC547 buffer inverts) ─────────────────────────────────────
#define RELAY_ON  HIGH   // GPIO HIGH → BC547 on → relay IN LOW → relay energises
#define RELAY_OFF LOW    // GPIO LOW  → BC547 off → relay IN HIGH → relay off

// ── Calibration ──────────────────────────────────────────────────────────────
const float ZMPT_VFACTOR        = 0.246f;
const float ACS712_SENSITIVITY  = 85.4f;

// ── Detection Thresholds ─────────────────────────────────────────────────────
const float ACTIVE_THRESHOLD_A  = 0.40f;
const int   IDLE_TIMEOUT_SEC    = 10;

// ── Button debounce ──────────────────────────────────────────────────────────
const unsigned long DEBOUNCE_MS = 50;

// ── Sampling ─────────────────────────────────────────────────────────────────
const int SAMPLES = 1000;

// ── State Machine ────────────────────────────────────────────────────────────
enum DeviceState { ACTIVE, IDLE, CUT };
DeviceState state         = ACTIVE;
unsigned long idleStartMs = 0;

// ── Button debounce state ────────────────────────────────────────────────────
bool          lastButtonRaw   = HIGH;
unsigned long lastDebounceMs  = 0;
bool          buttonPressed   = false;

// ── Zero offsets ─────────────────────────────────────────────────────────────
float acs712_zero = 2133.0f;
float zmpt_zero   = 2048.0f;

// ─────────────────────────────────────────────────────────────────────────────
const char* stateName(DeviceState s) {
    switch (s) {
        case ACTIVE: return "ACTIVE";
        case IDLE:   return "IDLE";
        case CUT:    return "CUT";
    }
    return "?";
}

// ─────────────────────────────────────────────────────────────────────────────
void setRelay(bool on) {
    digitalWrite(RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

// ─────────────────────────────────────────────────────────────────────────────
bool buttonWasPressed() {
    bool reading = digitalRead(BUTTON_PIN);

    if (reading != lastButtonRaw) {
        lastDebounceMs = millis();
        lastButtonRaw  = reading;
    }

    if ((millis() - lastDebounceMs) > DEBOUNCE_MS) {
        if (reading == LOW && !buttonPressed) {
            buttonPressed = true;
            return true;
        }
        if (reading == HIGH) {
            buttonPressed = false;
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
    return (result < 0.12f) ? 0.0f : result;
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

// ── OLED update ──────────────────────────────────────────────────────────────
void updateDisplay(float V, float I, float W) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    // Row 0: Voltage / Current
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("%.1fV", V);
    display.setCursor(64, 0);
    display.printf("%.3fA", I);

    // Row 1: Power (larger)
    display.setTextSize(2);
    display.setCursor(0, 14);
    display.printf("%.1fW", W);

    // Row 2: State
    display.setTextSize(1);
    display.setCursor(0, 36);
    display.printf("State: %s", stateName(state));

    // Row 3: Status line
    display.setCursor(0, 48);
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_SEC;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        display.printf("Cut in: %lum %02lus", remaining / 60, remaining % 60);

        // Progress bar
        int barWidth = 120;
        int filled = (int)((float)elapsedSec / (float)totalSec * barWidth);
        if (filled > barWidth) filled = barWidth;
        display.drawRect(4, 58, barWidth, 6, SSD1306_WHITE);
        display.fillRect(4, 58, filled, 6, SSD1306_WHITE);
    } else if (state == CUT) {
        display.print("BTN to restore");
    }

    display.display();
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    // Relay first — power stays ON during boot
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(true);

    // Button with internal pull-up
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // OLED init
    Wire.begin(21, 22);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.print("[OLED] SSD1306 init FAILED\r\n");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10, 20);
        display.print("PHANTOM LOAD KILLER");
        display.setCursor(20, 40);
        display.print("Calibrating...");
        display.display();
    }

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.print("\r\n============================================\r\n");
    Serial.print(" Phantom Load Killer — Step 4: OLED        \r\n");
    Serial.print("============================================\r\n");
    Serial.printf(" Active threshold : %.2f A\r\n", ACTIVE_THRESHOLD_A);
    Serial.printf(" Idle timeout     : %d sec\r\n", IDLE_TIMEOUT_SEC);
    Serial.print(" OLED             : SSD1306 128x64 I2C\r\n");
    Serial.print("\r\nCalibrating (ensure no load)...\r\n");
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
        idleStartMs = millis();
        Serial.print("\r\n>>> BUTTON PRESSED — Power restored\r\n\r\n");
    }

    // ── State machine ────────────────────────────────────────────────────
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
                unsigned long timeoutMs = (unsigned long)IDLE_TIMEOUT_SEC * 1000UL;
                if (elapsedMs >= timeoutMs) {
                    state = CUT;
                    setRelay(false);
                }
            }
            break;

        case CUT:
            break;
    }

    // ── State change notification ────────────────────────────────────────
    if (state != prevState) {
        const char* relayStr = (state == CUT) ? "RELAY OPEN — power cut" : "relay closed";
        Serial.printf("\r\n>>> STATE: %s -> %s  [%s]\r\n\r\n",
                      stateName(prevState), stateName(state), relayStr);
    }

    // ── Serial output ────────────────────────────────────────────────────
    char statusBuf[20] = "               ";
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_SEC;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        snprintf(statusBuf, sizeof(statusBuf), "%2lum %02lus left", remaining / 60, remaining % 60);
    } else if (state == CUT) {
        snprintf(statusBuf, sizeof(statusBuf), "btn to restore  ");
    }

    Serial.printf("  %s |  %5.1f V  |  %5.3f A  |  %5.1f W  |  %s\r\n",
                  stateName(state), V, I, W, statusBuf);

    // ── OLED output ──────────────────────────────────────────────────────
    updateDisplay(V, I, W);

    delay(500);
}
