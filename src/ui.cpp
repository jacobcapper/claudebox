#include "ui.h"
#include "config.h"
#include "hal.h"
#include <time.h>

// ── Colors (RGB565) ───────────────────────────────────────
#define C_BG      TFT_BLACK
#define C_TEXT    TFT_WHITE
#if defined(BOARD_CYD)
// The CYD panel crushes dark grays and has narrow viewing angles, so lift the
// gray ramp for legibility off-axis.
#define C_DIM     0xC618   // light gray (labels)
#define C_DARK    0x8410   // medium gray (footnotes)
#define C_BAR_BG  0x4208   // bar track
#else
#define C_DIM     0x7BEF   // mid-gray
#define C_DARK    0x4A49   // dark gray
#define C_BAR_BG  0x2104   // very dark gray
#endif
#define C_HEAD    0xEB87   // Claude orange
#define C_ACCENT  0xEB87
#define C_WARN    0xFD20   // amber

// Bar color: white always (matches original aesthetic)
static uint16_t barColor(float /*pct*/) { return C_TEXT; }

// ── Draw a labeled progress bar ────────────────────────────
// x,y = top-left of the label row; bh = bar height in pixels
static void drawBar(int x, int y, int w, int bh, float pct, const char* label) {
    char ps[8];
    snprintf(ps, sizeof(ps), "%.1f%%", pct);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(x, y);
    lcd.print(label);

    // Right-aligned percentage
    lcd.setTextColor(barColor(pct), C_BG);
    lcd.setCursor(x + w - (int)strlen(ps) * 6, y);
    lcd.print(ps);

    // Bar
    int by = y + 12;
    lcd.fillRect(x, by, w, bh, C_BAR_BG);
    int fw = constrain((int)(w * pct / 100.0f), 0, w);
    if (fw > 0) lcd.fillRect(x, by, fw, bh, barColor(pct));
}

// ── Format a seconds-until-epoch countdown ─────────────────
static void fmtCountdown(uint32_t epoch, char* out, size_t len) {
    if (epoch == 0) { strlcpy(out, "--", len); return; }
    time_t now = time(nullptr);
    int32_t diff = (int32_t)epoch - (int32_t)now;
    if (diff <= 0) { strlcpy(out, "now", len); return; }
    int d = diff / 86400;
    int h = (diff % 86400) / 3600;
    int m = (diff % 3600) / 60;
    if (d > 0)      snprintf(out, len, "%dd%dh", d, h);
    else if (h > 0) snprintf(out, len, "%dh%02dm", h, m);
    else            snprintf(out, len, "%dm", m);
}

// ── Public ─────────────────────────────────────────────────

void uiInit() {
    lcd.setRotation(SCREEN_ROT);
    lcd.fillScreen(C_BG);
}

void uiBootProgress(int percent, const char* label) {
    lcd.fillScreen(C_BG);

    lcd.setTextColor(C_ACCENT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(20, 80);
    lcd.print("Claude Usage");

    int bx = 20, by = 112, bw = lcd.width() - 40, bh = 16;
    lcd.fillRect(bx, by, bw, bh, C_BAR_BG);
    int fill = constrain((int)(bw * percent / 100.0f), 0, bw);
    lcd.fillRect(bx, by, fill, bh, C_ACCENT);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(20, by + bh + 8);
    lcd.print(label);
}

void uiSetupScreen(const char* apName, const char* apPass) {
    lcd.fillScreen(C_BG);

    lcd.fillRect(0, 0, lcd.width(), 24, C_ACCENT);
    lcd.setTextColor(C_BG, C_ACCENT);
    lcd.setTextSize(1);
    lcd.setCursor(8, 8);
    lcd.print("SETUP MODE");

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);

    lcd.setCursor(8, 32);
    lcd.print("1. Connect to WiFi:");
    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(8, 44);
    lcd.print(apName);

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(8, 68);
    if (apPass) {
        lcd.print("Password:");
        lcd.setTextColor(C_TEXT, C_BG);
        lcd.setTextSize(2);
        lcd.setCursor(8, 80);
        lcd.print(apPass);
    } else {
        lcd.print("No password (open network)");
    }

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(8, 106);
    lcd.print("2. Open in browser:");
    lcd.setTextColor(C_ACCENT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(8, 118);
    lcd.print("192.168.4.1");

    lcd.setTextColor(C_DARK, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(8, 150);
    lcd.print("To factory reset: GET /reset");
}

void uiConnecting(const char* ssid, int attempt) {
    lcd.fillScreen(C_BG);
    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 88);
    lcd.print("Connecting to WiFi...");

    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 104);
    lcd.print(ssid);

    if (attempt > 0) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 134);
        lcd.printf("Attempt %d", attempt);
    }
}

void uiDashboard(const UsageData& data, unsigned long lastFetchMs, int rssi) {
    lcd.fillScreen(C_BG);
    const int W = lcd.width();

    // ── Header bar ──────────────────────────────────────────
    lcd.fillRect(0, 0, W, 28, C_HEAD);
    lcd.setTextColor(C_BG, C_HEAD);
    lcd.setTextSize(1);
    lcd.setCursor(6, 10);
    lcd.print("CLAUDE USAGE");

    unsigned long ago = (millis() - lastFetchMs) / 1000;
    char hdr[24];
    snprintf(hdr, sizeof(hdr), "%lus %ddBm", ago, rssi);
    lcd.setCursor(W - (int)strlen(hdr) * 6 - 6, 10);
    lcd.print(hdr);

    if (!data.ok) {
        lcd.setTextColor(TFT_RED, C_BG);
        lcd.setTextSize(2);
        lcd.setCursor(10, 60);
        lcd.print("ERROR");
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 90);
        lcd.print(data.error);
        lcd.setCursor(10, 110);
        lcd.print("Will retry automatically.");
        return;
    }

    int barW = W - 20;

    // ── 5-hour section ──────────────────────────────────────
    // drawBar: label at y, bar at y+12, bar bottom at y+12+bh = 36+12+18 = 66
    drawBar(10, 36, barW, 18, data.h5, "5-HOUR WINDOW");

    char h5rst[16];
    fmtCountdown(data.h5ResetEpoch, h5rst, sizeof(h5rst));

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 70);       // below bar bottom (66)
    lcd.print("RESETS IN");

    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(2);          // was 3 (24px); now 2 (16px)
    lcd.setCursor(10, 80);
    lcd.print(h5rst);

    // ── Divider ─────────────────────────────────────────────
    lcd.drawFastHLine(10, 104, W - 20, C_BAR_BG);

    // ── 7-day section ───────────────────────────────────────
    // bar bottom at 112+12+18 = 142
    drawBar(10, 112, barW, 18, data.d7, "7-DAY WINDOW");

    char d7rst[16];
    fmtCountdown(data.d7ResetEpoch, d7rst, sizeof(d7rst));

    lcd.setTextColor(C_DIM, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 146);      // below bar bottom (142)
    lcd.print("RESETS IN");

    lcd.setTextColor(C_TEXT, C_BG);
    lcd.setTextSize(2);          // was 3
    lcd.setCursor(10, 156);
    lcd.print(d7rst);

    // ── Footer ──────────────────────────────────────────────
    lcd.drawFastHLine(10, 188, W - 20, C_BAR_BG);
    lcd.setTextColor(C_DARK, C_BG);
    lcd.setTextSize(1);
    lcd.setCursor(10, 196);
    lcd.printf("5H: %.1f%%  7D: %.1f%%", data.h5, data.d7);
    lcd.setCursor(10, 210);
    lcd.print("USB powered  /reset to reconfigure");
}

void uiError(const char* title, const char* detail) {
    lcd.fillScreen(C_BG);
    lcd.setTextColor(TFT_RED, C_BG);
    lcd.setTextSize(2);
    lcd.setCursor(10, 80);
    lcd.print(title);
    if (detail) {
        lcd.setTextColor(C_DIM, C_BG);
        lcd.setTextSize(1);
        lcd.setCursor(10, 112);
        lcd.print(detail);
    }
}
