# 02 — ESP32 Toolchain: Setup Issues on Arch Linux

## Arduino IDE 2.x — Failed

### Symptoms
- Installed via AUR (`arduino-ide-bin`)
- Binary launches but window never appears
- Running from terminal shows Electron crash:
  ```
  Uncaught Exception:
  Error: write EPIPE
    at afterWriteDispatched (node:internal/stream_base_commons:161:15)
    ...
    at console.<computed> [as info] (/opt/arduino-ide/resources/app/lib/backend/electron-main.js:2:1228830)
  ```
- EPIPE = broken pipe between Electron main process and renderer. Known issue with Arduino IDE 2.x on Wayland compositors.

### Environment
- OS: Arch Linux
- Display server: Wayland (`WAYLAND_DISPLAY=wayland-1`, `XDG_SESSION_TYPE=wayland`)
- Window manager: Hyprland (inferred from workspace)

### Workarounds Attempted
- `--enable-features=UseOzonePlatform --ozone-platform=wayland` flag — not tested (switched to CLI instead)
- `GDK_BACKEND=x11` XWayland fallback — not tested

### Resolution
**Switched to `arduino-cli`** — fully terminal-based, no GUI. Works perfectly. Recommended for all Arch/Wayland setups.

---

## arduino-cli Setup

### Installation
```fish
sudo pacman -S arduino-cli
```

### One-Time ESP32 Core Setup
```fish
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32
```
Downloads ~300MB of toolchain. Takes 3–5 minutes.

### FQBN
`esp32:esp32:esp32` — standard ESP32 Dev Module

---

## Upload Issues

### Problem 1: Permission Denied on /dev/ttyUSB0
```
[Errno 13] Permission denied: '/dev/ttyUSB0'
```

**Cause:** User not in `uucp` group (Arch Linux uses `uucp`, not `dialout` like Ubuntu).

**Fix:**
```fish
sudo usermod -aG uucp $USER
# then log out and back in
```

**Workaround without logout:**
```fish
sudo chmod 666 /dev/ttyUSB0
```
Resets on replug — only use while debugging.

### Problem 2: Upload Crash at High Baud Rate
```
esptool v5.2.0
Connected to ESP32 on /dev/ttyUSB0
Warning: Detected crystal freq 15.55 MHz is quite different to normalized freq 26 MHz.
...
StopIteration
A fatal error occurred: The chip stopped responding.
```

**Cause:** Clone ESP32 board (non-genuine) cannot reliably handle esptool's default 921600 baud rate for flashing. Crystal frequency warning confirms it's a clone/third-party board.

**Fix:** Lower upload speed via FQBN parameter:
```fish
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 --port /dev/ttyUSB0 sketch.ino
```

115200 is slower (~30–60 seconds per upload) but 100% reliable on clone boards.

---

## Serial Monitor

### arduino-cli monitor raw mode
`arduino-cli monitor` runs in raw terminal mode. The ESP32's `Serial.println()` sends `\n` only (LF), not `\r\n` (CRLF). In raw mode, LF moves to the next line but does **not** return the cursor to column 0 — causing each new line to continue from where the previous ended, producing garbled output.

**Fix:** Replace all `Serial.println()` and `\n` in format strings with `\r\n`:
```cpp
// Wrong (garbled in raw mode):
Serial.println("hello");
Serial.printf("value = %d\n", x);

// Correct:
Serial.print("hello\r\n");
Serial.printf("value = %d\r\n", x);
```

### Boot Button for Upload
Some clone ESP32 boards require manually holding the BOOT button during upload initiation. Symptom: upload hangs on `Connecting......____`. Hold BOOT, release after first `Writing at 0x...` appears.

---

## Quick Reference: Flash Command

```fish
# Compile
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 firmware/stepX/stepX.ino

# Upload
arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200 --port /dev/ttyUSB0 firmware/stepX/stepX.ino

# Monitor
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200

# All in one
arduino-cli compile --fqbn esp32:esp32:esp32:UploadSpeed=115200 firmware/stepX/stepX.ino && \
arduino-cli upload  --fqbn esp32:esp32:esp32:UploadSpeed=115200 --port /dev/ttyUSB0 firmware/stepX/stepX.ino && \
arduino-cli monitor --port /dev/ttyUSB0 --config baudrate=115200
```
