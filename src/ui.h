#pragma once
#include "api.h"

void uiInit();
void uiBootProgress(int percent, const char* label);
void uiSetupScreen(const char* apName, const char* apPass);
void uiConnecting(const char* ssid, int attempt = 0);
void uiDashboard(const UsageData& data, unsigned long lastFetchMs, int rssi);
void uiError(const char* title, const char* detail = nullptr);
