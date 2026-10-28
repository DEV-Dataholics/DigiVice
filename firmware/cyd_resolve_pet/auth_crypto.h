#pragma once
#include <Arduino.h>
#include <time.h>
#include <esp_system.h>
#include "mbedtls/md.h"

// ============================================================================
// Silicon Hardware-Bound Cryptography & Anti-Replay Engine
// Uses ESP32 Factory eFuse MAC + mbedtls HMAC-SHA256
// ============================================================================

class HardwareAuth {
public:
  // Reads immutable factory-fused 48-bit MAC address from silicon eFuses
  static String getSiliconMac() {
    uint64_t chipid = ESP.getEfuseMac();
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(chipid >> 40),
             (uint8_t)(chipid >> 32),
             (uint8_t)(chipid >> 24),
             (uint8_t)(chipid >> 16),
             (uint8_t)(chipid >> 8),
             (uint8_t)(chipid));
    return String(macStr);
  }

  // Generates 16-character random hex nonce from ESP32 hardware TRNG (thermal noise)
  static String generateNonce() {
    char nonce[17];
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    snprintf(nonce, sizeof(nonce), "%08x%08x", r1, r2);
    return String(nonce);
  }

  // Synchronizes system time with NTP servers (UTC epoch)
  static bool syncNtp(unsigned long timeoutMs = 8000) {
    configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
    unsigned long start = millis();
    time_t now = time(nullptr);
    while (now < 1700000000 && (millis() - start) < timeoutMs) {
      delay(200);
      now = time(nullptr);
    }
    return (now >= 1700000000);
  }

  // Computes HMAC-SHA256 signature using mbedtls hardware acceleration
  static String computeHmacSha256(const String &secretKey, const String &payload) {
    uint8_t hmacResult[32];
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char*)secretKey.c_str(), secretKey.length());
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload.c_str(), payload.length());
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    char hexBuffer[65];
    for (int i = 0; i < 32; i++) {
      snprintf(&hexBuffer[i * 2], 3, "%02x", hmacResult[i]);
    }
    return String(hexBuffer);
  }
};
