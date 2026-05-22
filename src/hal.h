#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

extern TFT_eSPI lcd;

void halInit();
void halUpdate();
bool halBtnAWasPressed();
bool halBtnBWasPressed();
bool halBtnAIsPressed();
bool halBtnBIsPressed();
int  halBatPercent();
void halSetBrightness(uint8_t level);
