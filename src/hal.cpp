#include "hal.h"
#include <Arduino.h>

TFT_eSPI lcd;

#define BL_PIN 5

void halInit() {
    lcd.init();
    lcd.setRotation(0);
    // Backlight is active LOW: analogWrite(0) = full on, (1023) = off
    pinMode(BL_PIN, OUTPUT);
    analogWrite(BL_PIN, 0);  // full brightness immediately so boot screen is visible
}

// SmallTV Ultra has no user buttons.
void halUpdate() {}
bool halBtnAWasPressed() { return false; }
bool halBtnBWasPressed() { return false; }
bool halBtnAIsPressed()  { return false; }
bool halBtnBIsPressed()  { return false; }

// USB powered — no battery.
int halBatPercent() { return -1; }

void halSetBrightness(uint8_t level) {
    // 0=off, 1=dim, 2=normal, 3=bright  (active-LOW PWM on 0-1023 scale)
    static const uint16_t vals[] = {1023, 700, 300, 0};
    analogWrite(BL_PIN, vals[level < 4 ? level : 2]);
}
