#include "hal.h"
#include "config.h"
#include <Arduino.h>

TFT_eSPI lcd;

#if defined(BOARD_CYD)
// ── ESP32 Cheap Yellow Display: backlight on GPIO21, active HIGH ──
#define BL_PIN     21
#define BL_CHANNEL 0

void halInit() {
    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    ledcSetup(BL_CHANNEL, 5000, 8);   // 5 kHz PWM, 8-bit
    ledcAttachPin(BL_PIN, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 255);       // full brightness so boot screen is visible
}

void halSetBrightness(uint8_t level) {
    // 0=off 1=dim 2=normal 3=bright (active HIGH, 0-255 duty)
    static const uint8_t vals[] = {0, 60, 160, 255};
    ledcWrite(BL_CHANNEL, vals[level < 4 ? level : 2]);
}

#else
// ── GeekMagic SmallTV Ultra: backlight on GPIO5, active LOW ──
#define BL_PIN 5

void halInit() {
    lcd.init();
    lcd.setRotation(SCREEN_ROT);
    // Backlight is active LOW: analogWrite(0) = full on, (1023) = off
    pinMode(BL_PIN, OUTPUT);
    analogWrite(BL_PIN, 0);  // full brightness immediately so boot screen is visible
}

void halSetBrightness(uint8_t level) {
    // 0=off, 1=dim, 2=normal, 3=bright  (active-LOW PWM on 0-1023 scale)
    static const uint16_t vals[] = {1023, 700, 300, 0};
    analogWrite(BL_PIN, vals[level < 4 ? level : 2]);
}
#endif

// Neither board has user buttons.
void halUpdate() {}
bool halBtnAWasPressed() { return false; }
bool halBtnBWasPressed() { return false; }
bool halBtnAIsPressed()  { return false; }
bool halBtnBIsPressed()  { return false; }

// USB powered — no battery.
int halBatPercent() { return -1; }
