/*
 * I2C Scanner — finds all devices on the bus
 * Flash this, open serial monitor at 115200
 */

#include <Wire.h>

void setup() {
    Serial.begin(115200);
    delay(500);
    Wire.begin(21, 22);
    Serial.print("\r\nI2C Scanner starting...\r\n");
}

void loop() {
    int found = 0;
    for (byte addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.printf("  Found device at 0x%02X\r\n", addr);
            found++;
        }
    }
    if (found == 0) {
        Serial.print("  No I2C devices found — check wiring (SDA=21, SCL=22)\r\n");
    }
    Serial.printf("  Scan done. %d device(s).\r\n\r\n", found);
    delay(3000);
}
