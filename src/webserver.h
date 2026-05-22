#pragma once
#include "api.h"

// Starts a web server on port 80 for runtime use (dashboard mode).
// Handles: GET /          → status page
//          GET /reset     → factory reset
//          POST /update   → firmware OTA upload
void webserverBegin(const UsageData* usagePtr);
void webserverHandle();
