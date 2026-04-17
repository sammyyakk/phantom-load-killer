/*
 * Phantom Load Killer — Final Firmware
 * =============================================================
 * ESP32-based smart AC power switch. Monitors real-time voltage,
 * current, and wattage. Automatically cuts relay when device goes
 * idle (phantom/standby load detection). Includes WiFi AP dashboard,
 * OLED display, RGB status LED, manual button restore.
 *
 * WiFi AP:
 *   SSID     : PhantomKiller
 *   Password : phantom123
 *   Dashboard: http://192.168.4.1
 *
 * Wiring:
 *   GPIO34 — ACS712 output (10k/22k divider)
 *   GPIO35 — ZMPT101B output
 *   GPIO26 — Relay module IN (via BC547 buffer)
 *   GPIO32 — Manual restore button (active LOW, internal pull-up)
 *   GPIO21 — OLED SDA
 *   GPIO22 — OLED SCL
 *   GPIO23 --[330R]--> RGB LED Red pin
 *   GPIO18 --[330R]--> RGB LED Green pin
 *   GPIO19 --[330R]--> RGB LED Blue pin
 *
 * Libraries required:
 *   Adafruit SSD1306, Adafruit GFX Library
 *   WiFi, WebServer (built-in with ESP32 core)
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ── WiFi AP Config ───────────────────────────────────────────────────────────
const char* AP_SSID = "PhantomKiller";
const char* AP_PASS = "phantom123";

WebServer server(80);

// ── OLED Setup ───────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR    0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── RGB LED Pins ─────────────────────────────────────────────────────────────
#define LED_RED_PIN   23
#define LED_GREEN_PIN 18
#define LED_BLUE_PIN  19

#define LED_MAX_BRIGHT 80

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define ACS712_PIN    34
#define ZMPT101B_PIN  35
#define RELAY_PIN     26
#define BUTTON_PIN    32

// ── Relay helpers ────────────────────────────────────────────────────────────
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// ── Calibration ──────────────────────────────────────────────────────────────
const float ZMPT_VFACTOR        = 0.222f;   // 240V reading: 186V → 240V needs 1.29x
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

// ── Dashboard control ────────────────────────────────────────────────────────
bool autoMode       = true;   // Auto idle detection ON by default
bool relayState     = true;   // Relay closed (power ON)
bool manualOverride = false;  // Set when dashboard forces relay state

// ── Button debounce state ────────────────────────────────────────────────────
bool          lastButtonRaw   = HIGH;
unsigned long lastDebounceMs  = 0;
bool          buttonPressed   = false;

// ── Zero offsets ─────────────────────────────────────────────────────────────
float acs712_zero = 2133.0f;
float zmpt_zero   = 2048.0f;

// ── Live readings (shared with web handlers) ─────────────────────────────────
float liveV = 0.0f, liveI = 0.0f, liveW = 0.0f;

// ── Power saved tracking ─────────────────────────────────────────────────────
float        powerAtCut      = 0.0f;   // watts when CUT triggered
float        whSaved         = 0.0f;   // watt-hours saved total
unsigned long cutStartMs     = 0;      // when CUT state began
unsigned long totalCutMs     = 0;      // total ms in CUT
int           cutCount        = 0;      // how many times auto-cut fired

// ─────────────────────────────────────────────────────────────────────────────
const char* stateName(DeviceState s) {
    switch (s) {
        case ACTIVE: return "ACTIVE";
        case IDLE:   return "IDLE";
        case CUT:    return "CUT";
    }
    return "?";
}

// ── RGB LED helper ───────────────────────────────────────────────────────────
void setRGB(uint8_t r, uint8_t g, uint8_t b) {
    ledcWrite(LED_RED_PIN,   r);
    ledcWrite(LED_GREEN_PIN, g);
    ledcWrite(LED_BLUE_PIN,  b);
}

// ─────────────────────────────────────────────────────────────────────────────
void setRelay(bool on) {
    relayState = on;
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

// ── LED update ───────────────────────────────────────────────────────────────
void updateLED() {
    switch (state) {
        case ACTIVE:
            setRGB(0, LED_MAX_BRIGHT, 0);
            break;
        case IDLE: {
            unsigned long elapsedMs = millis() - idleStartMs;
            float phase = (float)(elapsedMs % 2000UL) / 2000.0f * 6.2832f;
            uint8_t breath = (uint8_t)((sinf(phase) * 0.5f + 0.5f) * LED_MAX_BRIGHT);
            setRGB(breath, breath / 2, 0);
            break;
        }
        case CUT:
            setRGB(LED_MAX_BRIGHT, 0, 0);
            break;
    }
}

// ── OLED update ──────────────────────────────────────────────────────────────
void updateDisplay(float V, float I, float W) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("%.1fV", V);
    display.setCursor(64, 0);
    display.printf("%.3fA", I);

    display.setTextSize(2);
    display.setCursor(0, 14);
    display.printf("%.1fW", W);

    display.setTextSize(1);
    display.setCursor(0, 36);
    display.printf("State: %s", stateName(state));

    display.setCursor(0, 48);
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_SEC;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        display.printf("Cut in: %lum %02lus", remaining / 60, remaining % 60);
        int barWidth = 120;
        int filled = (int)((float)elapsedSec / (float)totalSec * barWidth);
        if (filled > barWidth) filled = barWidth;
        display.drawRect(4, 58, barWidth, 6, SSD1306_WHITE);
        display.fillRect(4, 58, filled, 6, SSD1306_WHITE);
    } else if (state == CUT) {
        display.print("BTN/Web restore");
    }

    display.display();
}

// ══════════════════════════════════════════════════════════════════════════════
//  WEB DASHBOARD
// ══════════════════════════════════════════════════════════════════════════════

// ── HTML Page (served once, then JS fetches /api/data) ───────────────────────
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Phantom Load Killer</title>
<style>
  *{margin:0;padding:0;box-sizing:border-box}

  :root{
    --bg:#0e0e10;--bg2:#111114;--border:#1c1c22;--text:#c0c0c0;--text2:#e8e8e8;
    --muted:#444;--muted2:#383840;--divider:#1e1e24;--bar-bg:#1a1a22;
    --head:#f0f0f0;--sub:#44444c;--up:#333;--srow-b:#16161e;
    --foot:#2a2a2a;
    --chip-on-c:#3a9e6e;--chip-on-bg:#0a2016;--chip-on-b:#1a3d2c;
    --chip-off-c:#b84040;--chip-off-bg:#200a0a;--chip-off-b:#3d1a1a;
    --tgl-bg:#1a1a22;--tgl-b:#2a2a34;--tgl-k:#444;
    --btn-on-c:#3a9e6e;--btn-on-b:#1a3d2c;--btn-on-h:#0a2016;
    --btn-off-c:#c05858;--btn-off-b:#3d1a1a;--btn-off-h:#200a0a;
    --auto-r:#181820;--auto-t:#a0a0a8;
  }
  body.light{
    --bg:#f4f5f7;--bg2:#ffffff;--border:#dde0e4;--text:#3a3a40;--text2:#1a1a1e;
    --muted:#8a8a94;--muted2:#9a9aa4;--divider:#d0d2d8;--bar-bg:#e0e2e6;
    --head:#1a1a1e;--sub:#8a8a94;--up:#999;--srow-b:#eaeaee;
    --foot:#b0b0b8;
    --chip-on-c:#1a7a48;--chip-on-bg:#e6f5ec;--chip-on-b:#b0d8c0;
    --chip-off-c:#b03030;--chip-off-bg:#fce8e8;--chip-off-b:#e0b0b0;
    --tgl-bg:#e0e2e6;--tgl-b:#c0c2c8;--tgl-k:#999;
    --btn-on-c:#1a7a48;--btn-on-b:#b0d8c0;--btn-on-h:#e6f5ec;
    --btn-off-c:#b03030;--btn-off-b:#e0b0b0;--btn-off-h:#fce8e8;
    --auto-r:#e0e2e6;--auto-t:#4a4a54;
  }

  body{
    font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
    background:var(--bg);color:var(--text);
    padding:18px 16px 36px;max-width:440px;margin:0 auto;
  }
  .mono{font-family:'Courier New',Consolas,monospace}

  /* theme toggle */
  .theme-btn{background:none;border:1px solid var(--border);color:var(--muted);
    font-size:16px;width:30px;height:30px;cursor:pointer;display:flex;
    align-items:center;justify-content:center;flex-shrink:0}
  .theme-btn:active{opacity:0.6}

  /* header */
  .hdr{display:flex;align-items:flex-start;justify-content:space-between;
    padding-bottom:14px;margin-bottom:18px;border-bottom:1px solid var(--divider)}
  .hdr-title{font-size:15px;font-weight:700;color:var(--head);letter-spacing:-0.3px}
  .hdr-sub{font-size:11px;color:var(--sub);margin-top:3px;font-weight:400}
  .hdr-right{display:flex;align-items:flex-start;gap:8px}
  .hdr-up{font-size:11px;color:var(--up);font-family:'Courier New',monospace;text-align:right;padding-top:2px}

  /* meters */
  .meters{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:12px}
  .meter{background:var(--bg2);border:1px solid var(--border);padding:14px 8px 12px;text-align:center;position:relative}
  .meter::after{content:'';position:absolute;top:0;left:0;right:0;height:2px}
  .meter.v::after{background:#2e7d57}
  .meter.a::after{background:#7d6a2e}
  .meter.w::after{background:#2e4e7d}
  .meter-val{font-family:'Courier New',Consolas,monospace;font-size:26px;font-weight:700;
    color:var(--text2);line-height:1;letter-spacing:-1px}
  .meter-unit{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:2px;margin-top:5px}

  /* status */
  .status{background:var(--bg2);border:1px solid var(--border);padding:10px 14px;
    margin-bottom:12px;display:flex;align-items:center;justify-content:space-between}
  .status-left{display:flex;align-items:center;gap:8px}
  .dot{width:7px;height:7px;border-radius:50%;flex-shrink:0}
  .dot.active{background:#2e7d57}
  .dot.idle{background:#c9a030;animation:blink 1s steps(1) infinite}
  .dot.cut{background:#b03030}
  @keyframes blink{50%{opacity:0}}
  .st{font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:1.5px}
  .st.active{color:#2e7d57}.st.idle{color:#c9a030}.st.cut{color:#b03030}
  .rchip{font-size:11px;font-weight:700;padding:3px 9px;border:1px solid;letter-spacing:0.5px}
  .rchip.on{color:var(--chip-on-c);border-color:var(--chip-on-b);background:var(--chip-on-bg)}
  .rchip.off{color:var(--chip-off-c);border-color:var(--chip-off-b);background:var(--chip-off-bg)}

  /* countdown */
  .cd{background:var(--bg2);border:1px solid var(--border);border-left:2px solid #c9a030;
    padding:10px 14px;margin-bottom:12px;display:none}
  .cd.show{display:block}
  .cd-lbl{font-size:10px;color:var(--muted);text-transform:uppercase;letter-spacing:2px;margin-bottom:7px}
  .bar-t{width:100%;height:3px;background:var(--bar-bg);margin-bottom:6px}
  .bar-f{height:100%;background:#c9a030;transition:width 0.8s linear}
  .cd-time{font-family:'Courier New',monospace;font-size:13px;color:#c9a030;font-weight:600}

  /* panel */
  .panel{background:var(--bg2);border:1px solid var(--border);padding:14px;margin-bottom:12px}
  .plabel{font-size:10px;color:var(--muted2);text-transform:uppercase;letter-spacing:2px;margin-bottom:12px}
  .btn-row{display:flex;gap:8px;margin-bottom:14px}
  .btn{flex:1;padding:11px 0;font-family:inherit;font-size:13px;font-weight:600;
    letter-spacing:0.3px;cursor:pointer;border:1px solid;background:transparent;transition:background 0.1s}
  .btn:active{opacity:0.6}
  .btn-on{color:var(--btn-on-c);border-color:var(--btn-on-b)}.btn-on:hover{background:var(--btn-on-h)}
  .btn-off{color:var(--btn-off-c);border-color:var(--btn-off-b)}.btn-off:hover{background:var(--btn-off-h)}
  .auto-row{display:flex;align-items:center;justify-content:space-between;
    padding-top:12px;border-top:1px solid var(--auto-r)}
  .auto-t{font-size:13px;color:var(--auto-t);font-weight:500}
  .auto-d{font-size:11px;color:var(--muted2);margin-top:2px}
  .tgl{position:relative;width:38px;height:20px;cursor:pointer}
  .tgl input{display:none}
  .tgl-t{position:absolute;inset:0;background:var(--tgl-bg);border:1px solid var(--tgl-b);border-radius:10px;transition:0.2s}
  .tgl-k{position:absolute;width:14px;height:14px;top:2px;left:2px;
    background:var(--tgl-k);border-radius:50%;transition:0.2s}
  .tgl input:checked~.tgl-t{background:#0a2016;border-color:#2e7d57}
  .tgl input:checked~.tgl-k{left:20px;background:#3a9e6e}

  /* savings */
  .savings{background:var(--bg2);border:1px solid var(--border);border-top:2px solid #2e4e7d;
    padding:14px;margin-bottom:12px}
  .srow{display:flex;justify-content:space-between;align-items:baseline;
    padding:6px 0;border-bottom:1px solid var(--srow-b)}
  .srow:last-child{border-bottom:none}
  .sk{font-size:12px;color:var(--muted)}
  .sv{font-family:'Courier New',monospace;font-size:13px;font-weight:700;color:var(--text2)}

  /* footer */
  .foot{display:flex;justify-content:space-between;font-size:11px;color:var(--foot);padding-top:6px}

  @media(max-width:340px){.meters{grid-template-columns:1fr}.meter-val{font-size:22px}}
</style>
</head>
<body>

<div class="hdr">
  <div>
    <div class="hdr-title">Phantom Load Killer</div>
    <div class="hdr-sub">ESP32 Smart AC Monitor</div>
  </div>
  <div class="hdr-right">
    <div class="hdr-up mono" id="uptime">--</div>
    <button class="theme-btn" id="themeBtn" onclick="toggleTheme()" title="Toggle light/dark mode">&#9789;</button>
  </div>
</div>

<div class="meters">
  <div class="meter v"><div class="meter-val" id="v">--</div><div class="meter-unit">Volts</div></div>
  <div class="meter a"><div class="meter-val" id="i">--</div><div class="meter-unit">Amps</div></div>
  <div class="meter w"><div class="meter-val" id="w">--</div><div class="meter-unit">Watts</div></div>
</div>

<div class="status">
  <div class="status-left">
    <span class="dot" id="dot"></span>
    <span class="st" id="badge">--</span>
  </div>
  <span class="rchip off" id="relay">--</span>
</div>

<div class="cd" id="countdown">
  <div class="cd-lbl">Idle &mdash; cutting in</div>
  <div class="bar-t"><div class="bar-f" id="progress" style="width:0%"></div></div>
  <div class="cd-time" id="timerText">--</div>
</div>

<div class="panel">
  <div class="plabel">Relay Control</div>
  <div class="btn-row">
    <button class="btn btn-on" onclick="cmd('on')">POWER ON</button>
    <button class="btn btn-off" onclick="cmd('off')">POWER OFF</button>
  </div>
  <div class="auto-row">
    <div>
      <div class="auto-t">Auto-detection</div>
      <div class="auto-d">Cut relay on idle load</div>
    </div>
    <label class="tgl">
      <input type="checkbox" id="autoChk" checked onchange="toggleAuto()">
      <span class="tgl-t"></span>
      <span class="tgl-k"></span>
    </label>
  </div>
</div>

<div class="savings">
  <div class="plabel">Power Saved</div>
  <div class="srow"><span class="sk">Energy saved</span><span class="sv" id="whs">0.00 Wh</span></div>
  <div class="srow"><span class="sk">Times cut</span><span class="sv" id="cuts">0</span></div>
  <div class="srow"><span class="sk">Total time off</span><span class="sv" id="cuttime">0m 00s</span></div>
</div>

<div class="foot">
  <span>192.168.4.1 &mdash; 2s refresh</span>
  <span id="watt-live">-- W live</span>
</div>

<script>
var E=function(id){return document.getElementById(id)};
function cmd(a){fetch('/api/relay?action='+a).then(function(r){return r.json()}).then(update)}
function toggleAuto(){
  var o=E('autoChk').checked;
  fetch('/api/auto?mode='+(o?'1':'0')).then(function(r){return r.json()}).then(update);
}
function update(d){
  E('v').textContent=d.v.toFixed(1);
  E('i').textContent=d.i.toFixed(3);
  E('w').textContent=d.w.toFixed(1);
  E('watt-live').textContent=d.w.toFixed(1)+' W';
  var s=d.state.toLowerCase();
  E('dot').className='dot '+s;
  E('badge').className='st '+s;
  E('badge').textContent=d.state.toUpperCase();
  var rl=E('relay');rl.textContent=d.relay?'ON':'OFF';rl.className='rchip '+(d.relay?'on':'off');
  E('autoChk').checked=d.auto;
  var cd=E('countdown');
  if(s==='idle'){
    cd.className='cd show';
    E('progress').style.width=(d.idleElapsed/d.idleTimeout*100).toFixed(1)+'%';
    var rem=d.idleTimeout-d.idleElapsed;if(rem<0)rem=0;
    E('timerText').textContent=Math.floor(rem/60)+'m '+(rem%60<10?'0':'')+(rem%60)+'s';
  }else{cd.className='cd'}
  E('whs').textContent=d.whSaved.toFixed(2)+' Wh';
  E('cuts').textContent=d.cutCount;
  var cs=d.cutSec;
  E('cuttime').textContent=Math.floor(cs/60)+'m '+(cs%60<10?'0':'')+(cs%60)+'s';
  var ut=d.uptime;
  E('uptime').textContent=Math.floor(ut/3600)+'h '+Math.floor((ut%3600)/60)+'m '+ut%60+'s';
}
function toggleTheme(){
  var b=document.body;
  var light=b.classList.toggle('light');
  try{localStorage.setItem('plk-theme',light?'light':'dark')}catch(e){}
  E('themeBtn').innerHTML=light?'&#9788;':'&#9789;';
}
(function(){try{if(localStorage.getItem('plk-theme')==='light'){document.body.classList.add('light');E('themeBtn').innerHTML='&#9788;'}}catch(e){}})();
function poll(){fetch('/api/data').then(function(r){return r.json()}).then(update).catch(function(){})}
setInterval(poll,2000);poll();
</script>
</body>
</html>
)rawliteral";

// ── API: GET /api/data — JSON with live readings ─────────────────────────────
void handleApiData() {
    unsigned long uptimeSec = millis() / 1000UL;
    unsigned long idleElapsed = 0;
    if (state == IDLE) {
        idleElapsed = (millis() - idleStartMs) / 1000UL;
    }

    // Calculate live wh saved (add running CUT time if currently in CUT)
    float currentWhSaved = whSaved;
    unsigned long currentCutMs = totalCutMs;
    if (state == CUT && cutStartMs > 0) {
        unsigned long runningCutMs = millis() - cutStartMs;
        currentWhSaved += powerAtCut * (float)runningCutMs / 3600000.0f;
        currentCutMs += runningCutMs;
    }

    char json[384];
    snprintf(json, sizeof(json),
        "{\"v\":%.1f,\"i\":%.3f,\"w\":%.1f,"
        "\"state\":\"%s\",\"relay\":%s,\"auto\":%s,"
        "\"idleElapsed\":%lu,\"idleTimeout\":%d,\"uptime\":%lu,"
        "\"whSaved\":%.2f,\"cutCount\":%d,\"cutSec\":%lu}",
        liveV, liveI, liveW,
        stateName(state),
        relayState ? "true" : "false",
        autoMode ? "true" : "false",
        idleElapsed,
        IDLE_TIMEOUT_SEC,
        uptimeSec,
        currentWhSaved,
        cutCount,
        currentCutMs / 1000UL
    );

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

// ── API: GET /api/relay?action=on|off — manual relay control ─────────────────
void handleApiRelay() {
    if (!server.hasArg("action")) {
        server.send(400, "application/json", "{\"error\":\"missing action\"}");
        return;
    }

    String action = server.arg("action");

    if (action == "on") {
        // Accumulate saved energy before restoring
        if (state == CUT && cutStartMs > 0) {
            unsigned long cutDuration = millis() - cutStartMs;
            whSaved += powerAtCut * (float)cutDuration / 3600000.0f;
            totalCutMs += cutDuration;
        }
        setRelay(true);
        state = ACTIVE;
        manualOverride = false;
        cutStartMs = 0;
        Serial.print("\r\n>>> DASHBOARD: Relay ON — power restored\r\n\r\n");
    } else if (action == "off") {
        setRelay(false);
        state = CUT;
        powerAtCut = liveW;
        cutStartMs = millis();
        manualOverride = true;
        Serial.print("\r\n>>> DASHBOARD: Relay OFF — power cut\r\n\r\n");
    } else {
        server.send(400, "application/json", "{\"error\":\"invalid action\"}");
        return;
    }

    handleApiData();
}

// ── API: GET /api/auto?mode=1|0 — toggle auto detection ─────────────────────
void handleApiAuto() {
    if (!server.hasArg("mode")) {
        server.send(400, "application/json", "{\"error\":\"missing mode\"}");
        return;
    }

    String mode = server.arg("mode");
    autoMode = (mode == "1");
    manualOverride = false;

    if (autoMode && state == CUT && relayState) {
        state = ACTIVE;
    }

    Serial.printf("\r\n>>> DASHBOARD: Auto mode %s\r\n\r\n", autoMode ? "ON" : "OFF");
    handleApiData();
}

// ── Serve dashboard HTML ─────────────────────────────────────────────────────
void handleRoot() {
    server.send(200, "text/html", DASHBOARD_HTML);
}

// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(300);

    // Relay first — power stays ON during boot
    pinMode(RELAY_PIN, OUTPUT);
    setRelay(true);

    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // RGB LED
    ledcAttach(LED_RED_PIN,   5000, 8);
    ledcAttach(LED_GREEN_PIN, 5000, 8);
    ledcAttach(LED_BLUE_PIN,  5000, 8);
    setRGB(0, 0, 0);

    // OLED
    Wire.begin(21, 22);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        Serial.print("[OLED] SSD1306 init FAILED\r\n");
    } else {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(10, 20);
        display.print("PHANTOM LOAD KILLER");
        display.setCursor(30, 40);
        display.print("Starting WiFi...");
        display.display();
    }

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    // ── Start WiFi AP ────────────────────────────────────────────────────
    WiFi.mode(WIFI_AP);
    delay(100);
    bool apOk = WiFi.softAP(AP_SSID, AP_PASS, 6, 0, 4);
    // channel 6, not hidden, max 4 connections
    delay(500);
    IPAddress ip = WiFi.softAPIP();

    Serial.print("\r\n============================================\r\n");
    Serial.print(" Phantom Load Killer — Step 6: Dashboard    \r\n");
    Serial.print("============================================\r\n");
    Serial.printf(" WiFi AP start : %s\r\n", apOk ? "OK" : "FAILED");
    Serial.printf(" WiFi AP SSID  : %s\r\n", AP_SSID);
    Serial.printf(" WiFi AP Pass  : %s\r\n", AP_PASS);
    Serial.printf(" WiFi Channel  : 6\r\n");
    Serial.printf(" Dashboard     : http://%s\r\n", ip.toString().c_str());
    Serial.printf(" Active thresh : %.2f A\r\n", ACTIVE_THRESHOLD_A);
    Serial.printf(" Idle timeout  : %d sec\r\n", IDLE_TIMEOUT_SEC);
    Serial.print("============================================\r\n");

    // ── Web server routes ────────────────────────────────────────────────
    server.on("/",           HTTP_GET, handleRoot);
    server.on("/api/data",   HTTP_GET, handleApiData);
    server.on("/api/relay",  HTTP_GET, handleApiRelay);
    server.on("/api/auto",   HTTP_GET, handleApiAuto);
    server.begin();
    Serial.print("[WEB] Server started\r\n");

    // ── Calibrate ────────────────────────────────────────────────────────
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print("Calibrating...");
    display.display();

    Serial.print("\r\nCalibrating (ensure no load)...\r\n");
    delay(1000);
    calibrateZero();

    Serial.print("\r\n  State   |  Voltage  |  Current  |  Power   |  Timer / Status\r\n");
    Serial.print("  --------|-----------|-----------|----------|------------------\r\n");
}

// ─────────────────────────────────────────────────────────────────────────────
void loop() {
    // Handle web requests (non-blocking)
    server.handleClient();

    // Measure
    liveV = measureVoltageRMS();
    liveI = measureCurrentRMS();
    liveW = liveV * liveI;

    DeviceState prevState = state;

    // ── Manual button: restore from CUT ──────────────────────────────────
    if (state == CUT && buttonWasPressed()) {
        // Accumulate saved energy before restoring
        if (cutStartMs > 0) {
            unsigned long cutDuration = millis() - cutStartMs;
            whSaved += powerAtCut * (float)cutDuration / 3600000.0f;
            totalCutMs += cutDuration;
        }
        setRelay(true);
        state = ACTIVE;
        manualOverride = false;
        cutStartMs = 0;
        idleStartMs = millis();
        Serial.print("\r\n>>> BUTTON PRESSED — Power restored\r\n\r\n");
    }

    // ── State machine (only runs in auto mode, not in manual override) ───
    if (autoMode && !manualOverride) {
        switch (state) {
            case ACTIVE:
                if (liveI < ACTIVE_THRESHOLD_A) {
                    state       = IDLE;
                    idleStartMs = millis();
                }
                break;

            case IDLE:
                if (liveI >= ACTIVE_THRESHOLD_A) {
                    state = ACTIVE;
                } else {
                    unsigned long elapsedMs = millis() - idleStartMs;
                    unsigned long timeoutMs = (unsigned long)IDLE_TIMEOUT_SEC * 1000UL;
                    if (elapsedMs >= timeoutMs) {
                        state = CUT;
                        powerAtCut = liveW;  // remember what we're saving
                        cutStartMs = millis();
                        cutCount++;
                        setRelay(false);
                    }
                }
                break;

            case CUT:
                break;
        }
    }

    // ── State change notification ────────────────────────────────────────
    if (state != prevState) {
        const char* relayStr = (state == CUT) ? "RELAY OPEN" : "relay closed";
        Serial.printf("\r\n>>> STATE: %s -> %s  [%s]\r\n\r\n",
                      stateName(prevState), stateName(state), relayStr);
    }

    // ── Serial output ────────────────────────────────────────────────────
    char statusBuf[24] = "               ";
    if (state == IDLE) {
        unsigned long elapsedSec = (millis() - idleStartMs) / 1000UL;
        unsigned long totalSec   = (unsigned long)IDLE_TIMEOUT_SEC;
        unsigned long remaining  = (totalSec > elapsedSec) ? (totalSec - elapsedSec) : 0;
        snprintf(statusBuf, sizeof(statusBuf), "%2lum %02lus left", remaining / 60, remaining % 60);
    } else if (state == CUT) {
        snprintf(statusBuf, sizeof(statusBuf), "btn/web restore ");
    }

    Serial.printf("  %s |  %5.1f V  |  %5.3f A  |  %5.1f W  |  %s\r\n",
                  stateName(state), liveV, liveI, liveW, statusBuf);

    // ── OLED + LED ───────────────────────────────────────────────────────
    updateDisplay(liveV, liveI, liveW);
    updateLED();

    delay(500);
}
