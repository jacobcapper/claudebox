#include "api.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

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
    BearSSL::WiFiClientSecure client;
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

    String body = "{\"model\":\"" PROBE_MODEL "\","
                  "\"max_tokens\":1,"
                  "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";

    Serial.printf("[API] POST %s\n", MESSAGES_ENDPOINT);
    int code = https.POST(body);
    Serial.printf("[API] HTTP %d\n", code);

    if (code <= 0) {
        snprintf(out.error, sizeof(out.error), "http_%d", code);
        out.ok = false;
        https.end();
        return false;
    }

    String h5u = https.header("anthropic-ratelimit-unified-5h-utilization");
    String h5r = https.header("anthropic-ratelimit-unified-5h-reset");
    String d7u = https.header("anthropic-ratelimit-unified-7d-utilization");
    String d7r = https.header("anthropic-ratelimit-unified-7d-reset");

    Serial.printf("[API] 5h: %s  7d: %s\n", h5u.c_str(), d7u.c_str());
    Serial.printf("[API] 5h_reset: %s  7d_reset: %s\n", h5r.c_str(), d7r.c_str());

    https.end();

    if (h5u.length() == 0 && d7u.length() == 0) {
        if (code == 401) {
            strlcpy(out.error, "auth_failed", sizeof(out.error));
        } else {
            snprintf(out.error, sizeof(out.error), "no_headers_%d", code);
        }
        out.ok = false;
        return false;
    }

    out.h5 = h5u.toFloat() * 100.0f;
    out.d7 = d7u.toFloat() * 100.0f;
    out.h5ResetEpoch = (uint32_t)h5r.toInt();
    out.d7ResetEpoch = (uint32_t)d7r.toInt();
    out.ok = true;
    return true;
}
