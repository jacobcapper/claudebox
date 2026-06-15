#pragma once
#include "crypto.h"
#include <stdint.h>

// Persistent config stored in EEPROM.
struct StoredConfig {
    uint32_t     magic;        // EEPROM_MAGIC when valid
    char         ssid[33];
    char         wifipass[65];
    EncryptedBlob blob;
    int32_t      pollSec;
    int32_t      brightness;
    char         devName[33];
    uint8_t      provisioned;
    uint8_t      rotation;      // TFT_eSPI setRotation value (0-3)
};

bool configLoad(StoredConfig& cfg);
void configSave(const StoredConfig& cfg);
void configClear();

// Starts a WiFi AP, serves captive portal at 192.168.4.1.
// Blocks until user submits config. Encrypts token, saves to EEPROM, reboots.
void runProvisioningPortal(const char* apName, const char* apPass);
