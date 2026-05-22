#include "crypto.h"
#include "config.h"
#include <SHA256.h>
#include <GCM.h>
#include <AES.h>
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include <string.h>

// Derive a 256-bit key from the device MAC address.
// Using MAC as the only input means this protects against naive flash dumps
// but not against an attacker who knows the device MAC.
static void deriveKey(uint8_t* keyOut32) {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    static const uint8_t CONTEXT[] = "ClaudeMonitor-SmallTV-Ultra-v1";

    SHA256 sha;
    uint8_t hash[32];

    sha.reset();
    sha.update(mac, 6);
    sha.update(CONTEXT, sizeof(CONTEXT) - 1);
    sha.finalize(hash, 32);

    for (int i = 1; i < KDF_ROUNDS; i++) {
        sha.reset();
        sha.update(hash, 32);
        sha.finalize(hash, 32);
    }

    memcpy(keyOut32, hash, 32);
}

bool encryptToken(const char* plaintext, EncryptedBlob& blob) {
    size_t ptLen = strlen(plaintext);
    if (ptLen > sizeof(blob.ciphertext)) return false;

    // Generate a random 96-bit IV using hardware entropy
    for (size_t i = 0; i < sizeof(blob.iv); i++) {
        blob.iv[i] = (uint8_t)(ESP.getCycleCount() ^ micros() ^ random(256));
        delayMicroseconds(1);
    }

    uint8_t key[32];
    deriveKey(key);

    GCM<AES256> gcm;
    gcm.setKey(key, 32);
    gcm.setIV(blob.iv, sizeof(blob.iv));
    gcm.encrypt(blob.ciphertext, (const uint8_t*)plaintext, ptLen);
    gcm.computeTag(blob.tag, sizeof(blob.tag));

    memset(key, 0, sizeof(key));
    blob.len = (uint16_t)ptLen;
    return true;
}

bool decryptToken(const EncryptedBlob& blob, char* plainOut, size_t plainMaxLen) {
    if (blob.len == 0 || blob.len > plainMaxLen - 1) return false;

    uint8_t key[32];
    deriveKey(key);

    GCM<AES256> gcm;
    gcm.setKey(key, 32);
    gcm.setIV(blob.iv, sizeof(blob.iv));
    gcm.decrypt((uint8_t*)plainOut, blob.ciphertext, blob.len);
    bool valid = gcm.checkTag(blob.tag, sizeof(blob.tag));

    memset(key, 0, sizeof(key));

    if (!valid) {
        memset(plainOut, 0, blob.len + 1);
        return false;
    }
    plainOut[blob.len] = '\0';
    return true;
}
