#pragma once

// ── Polling ──────────────────────────────────────────────
#define DEFAULT_POLL_SEC        120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC            300

// ── Security ─────────────────────────────────────────────
// Key is derived from device MAC (no PIN — these devices have no buttons).
// ESP8266 is ~10x slower than ESP32 at SHA-256, so it uses fewer rounds.
#if defined(ESP32)
#define KDF_ROUNDS              10000
#else
#define KDF_ROUNDS              1000
#endif

// ── Display ──────────────────────────────────────────────
#if defined(BOARD_CYD)
#define SCREEN_W                240   // ST7789 240x320, portrait
#define SCREEN_H                320
#define SCREEN_ROT              0
#else
#define SCREEN_W                240   // SmallTV Ultra ST7789 240x240
#define SCREEN_H                240
#define SCREEN_ROT              0
#endif
#define DEFAULT_BRIGHTNESS      2   // 0=off 1=dim 2=normal 3=bright
#define DEFAULT_ROTATION        SCREEN_ROT   // TFT_eSPI rotation 0-3

// ── Network ──────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_S  20
#define API_TIMEOUT_MS          15000
#define MESSAGES_ENDPOINT       "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION       "2023-06-01"
#define PROBE_MODEL             "claude-haiku-4-5-20251001"

// ── Storage ──────────────────────────────────────────────
#define EEPROM_SIZE             1024
#define EEPROM_MAGIC            0xCAFEBEEFUL
