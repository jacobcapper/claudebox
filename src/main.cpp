/*
 * Claude Code Usage Monitor — GeekMagic SmallTV Ultra (ESP8266)
 *
 * Ported from: https://github.com/oauramos/claude-usage-stick
 * Target: ESP8266 + ST7789 240x240 TFT (GeekMagic SmallTV Ultra)
 *
 * Token is AES-256-GCM encrypted using a key derived from the device MAC.
 * No PIN entry (device has no buttons). To reconfigure: GET http://<ip>/reset
 * after connecting, or hard-reset by re-flashing.
 *
 * Setup: on first boot the device creates a WiFi AP named ClaudeMonitor-XXXX.
 * Connect and open http://192.168.4.1 to enter WiFi creds and OAuth token.
 */

#include "hal.h"
#include "config.h"
#include "crypto.h"
#include "provision.h"
#include "api.h"
#include "ui.h"
#include "webserver.h"
#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <time.h>

// Short unique device id for the setup AP name (ESP8266 chip id has no ESP32
// equivalent, so derive from the MAC there).
static uint16_t deviceId() {
#if defined(ESP32)
    return (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
#else
    return (uint16_t)ESP.getChipId();
#endif
}

static StoredConfig cfg;
static char         token[256];
static UsageData    usage;
static unsigned long lastFetch = 0;

// ── WiFi ───────────────────────────────────────────────────
static bool connectWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.ssid, cfg.wifipass);
    int ticks = 0;
    while (WiFi.status() != WL_CONNECTED) {
        ticks++;
        uiConnecting(cfg.ssid, ticks / 2);
        delay(500);
        if (ticks > WIFI_CONNECT_TIMEOUT_S * 2) return false;
    }
    return true;
}

// ── NTP time sync ──────────────────────────────────────────
static void syncTime() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    time_t now = 0;
    int tries = 0;
    while (now < 1000000000UL && tries++ < 20) {
        delay(500);
        now = time(nullptr);
    }
}

// ── Fetch API + redraw ─────────────────────────────────────
static void refresh() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }
    fetchUsage(token, usage);
    lastFetch = millis();
    uiDashboard(usage, lastFetch, WiFi.RSSI());
}

// ── Setup ──────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    halInit();
    uiInit();
    halSetBrightness(DEFAULT_BRIGHTNESS);

    uiBootProgress(10, "Initializing...");
    delay(200);

    uiBootProgress(25, "Checking config...");
    if (!configLoad(cfg)) {
        uiBootProgress(40, "No config — starting setup...");
        delay(400);

        // Generate AP name from chip ID (unique per device)
        char apName[24];
        snprintf(apName, sizeof(apName), "ClaudeMonitor-%04X", deviceId());

        // Open AP — no password needed for the setup portal.
        // Security boundary is the captive portal form itself.
        runProvisioningPortal(apName, nullptr);
        return; // never reached — portal reboots on success
    }

    halSetBrightness((uint8_t)cfg.brightness);
    lcd.setRotation(cfg.rotation & 0x03);

    uiBootProgress(50, "Decrypting token...");
    // MAC is available as soon as WiFi is powered; mode() ensures the driver is up
    WiFi.mode(WIFI_STA);
    if (!decryptToken(cfg.blob, token, sizeof(token))) {
        uiError("DECRYPT FAILED", "Re-flash or re-provision device.");
        return; // halt
    }

    uiBootProgress(65, "Connecting WiFi...");
    if (!connectWiFi()) {
        // Wrong credentials or network unavailable — wipe config and re-provision
        // so the user can correct their WiFi details without needing a reflash.
        uiError("WIFI FAILED", "Returning to setup...");
        delay(3000);
        configClear();
        char apName[24];
        snprintf(apName, sizeof(apName), "ClaudeMonitor-%04X", deviceId());
        runProvisioningPortal(apName, nullptr);
        return;
    }

    uiBootProgress(85, "Syncing time...");
    syncTime();

    uiBootProgress(95, "Fetching usage...");
    refresh();
    webserverBegin(&usage);
}

// ── Loop ───────────────────────────────────────────────────
void loop() {
    webserverHandle();

    // Auto-refresh on interval
    if (millis() - lastFetch >= (unsigned long)cfg.pollSec * 1000UL) {
        refresh();
    }

    // Periodic redraw to update the "Xs ago" counter in the header
    static unsigned long lastRedraw = 0;
    if (millis() - lastRedraw > 10000) {
        uiDashboard(usage, lastFetch, WiFi.RSSI());
        lastRedraw = millis();
    }

    delay(20);
}
