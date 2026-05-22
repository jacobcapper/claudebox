#pragma once
#include <stdint.h>
#include <stddef.h>

// AES-256-GCM encrypted token blob.
// Key is derived from device MAC (no user PIN — SmallTV Ultra has no buttons).
struct EncryptedBlob {
    uint8_t  iv[12];
    uint8_t  tag[16];
    uint16_t len;
    uint8_t  ciphertext[256];  // OAuth tokens are ~150 chars max
};

bool encryptToken(const char* plaintext, EncryptedBlob& blob);
bool decryptToken(const EncryptedBlob& blob, char* plainOut, size_t plainMaxLen);
