#include "api.h"
#include "config.h"
#if defined(ESP32)
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#endif

static const char* RL_HEADERS[] = {
    "anthropic-ratelimit-unified-5h-utilization",
    "anthropic-ratelimit-unified-5h-reset",
    "anthropic-ratelimit-unified-7d-utilization",
    "anthropic-ratelimit-unified-7d-reset",
};
static const int RL_HEADER_COUNT = 4;

bool fetchUsage(const char* token, UsageData& out) {
    // setInsecure() skips server cert validation.
    // Full CA chain verification would need ~30 KB of extra heap, which is
    // tight on ESP8266. The connection is still TLS-encrypted.
#if defined(ESP32)
    WiFiClientSecure client;
#else
    BearSSL::WiFiClientSecure client;
#endif
    client.setInsecure();

    HTTPClient https;
    https.setTimeout(API_TIMEOUT_MS);

    if (!https.begin(client, MESSAGES_ENDPOINT)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        out.ok = false;
        return false;
    }

    https.addHeader("Authorization", String("Bearer ") + token);
    https.addHeader("anthropic-version", ANTHROPIC_VERSION);
    https.addHeader("anthropic-beta", "oauth-2025-04-20");
    https.addHeader("content-type", "application/json");
    https.addHeader("User-Agent", "claude-code/2.1.5");
    https.collectHeaders(RL_HEADERS, RL_HEADER_COUNT);

    // Claude Code OAuth tokens are only accepted when the request is shaped like
    // Claude Code — the system prompt MUST start with this exact string, or the
    // API returns 401 even for a valid token.
    String body = "{\"model\":\"" PROBE_MODEL "\","
                  "\"max_tokens\":1,"
                  "\"system\":\"You are Claude Code, Anthropic's official CLI for Claude.\","
                  "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";

    Serial.printf("[API] POST %s\n", MESSAGES_ENDPOINT);
    int code = https.POST(body);
    Serial.printf("[API] HTTP %d  (token len=%u prefix=%.13s)\n",
                  code, (unsigned)strlen(token), token);

    if (code <= 0) {
        snprintf(out.error, sizeof(out.error), "net_%d", code);
        out.ok = false;
        https.end();
        return false;
    }

    String h5u = https.header("anthropic-ratelimit-unified-5h-utilization");
    String h5r = https.header("anthropic-ratelimit-unified-5h-reset");
    String d7u = https.header("anthropic-ratelimit-unified-7d-utilization");
    String d7r = https.header("anthropic-ratelimit-unified-7d-reset");

    // Non-200 or no rate-limit headers: capture the API error body so the real
    // reason is visible on screen and serial, instead of a vague "auth_failed".
    if (code != 200 || (h5u.length() == 0 && d7u.length() == 0)) {
        String resp = https.getString();
        https.end();
        Serial.printf("[API] error %d body: %s\n", code, resp.c_str());
        snprintf(out.error, sizeof(out.error), "%d:%.55s", code, resp.c_str());
        out.ok = false;
        return false;
    }

    Serial.printf("[API] 5h: %s  7d: %s\n", h5u.c_str(), d7u.c_str());
    Serial.printf("[API] 5h_reset: %s  7d_reset: %s\n", h5r.c_str(), d7r.c_str());
    https.end();

    out.h5 = h5u.toFloat() * 100.0f;
    out.d7 = d7u.toFloat() * 100.0f;
    out.h5ResetEpoch = (uint32_t)h5r.toInt();
    out.d7ResetEpoch = (uint32_t)d7r.toInt();
    out.ok = true;
    return true;
}
