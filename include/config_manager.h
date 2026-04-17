/**
 * Configuration Manager - NVS Storage
 * Huonyx AI Smartwatch
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "hw_config.h"

struct WatchConfig {
    /* WiFi */
    char     wifiSSID[33];
    char     wifiPass[65];

    /* HoC Gateway */
    char     gwHost[64];
    uint16_t gwPort;
    char     gwToken[128];
    bool     gwUseSSL;
    char     gwFingerprint[60];  /* SHA-1 TLS fingerprint ("AA:BB:CC:...") or empty */

    /* Display */
    uint8_t  brightness;    /* 0-255 */
    int8_t   timezone;      /* UTC offset in hours */

    /* Supabase Realtime Bridge */
    char     sbUrl[128];    /* Project URL (e.g., "abcdef.supabase.co") */
    char     sbKey[128];    /* Anon or service key */
    char     sbFingerprint[60];  /* SHA-1 TLS fingerprint for Supabase or empty */

    /* Flipper Zero */
    char     flipperName[32];  /* Target device name (empty = any Flipper) */
    bool     flipperAuto;      /* Auto-connect on boot */
};

class ConfigManager {
public:
    ConfigManager();

    void begin();
    void load();
    void save();
    void reset();

    /* Getters */
    WatchConfig& config() { return _cfg; }
    const WatchConfig& config() const { return _cfg; }

    /* Convenience setters */
    void setWiFi(const char* ssid, const char* pass);
    void setGateway(const char* host, uint16_t port, const char* token, bool ssl);
    void setGwFingerprint(const char* fingerprint);   /* Optional TLS fingerprint */
    void setBrightness(uint8_t val);
    void setTimezone(int8_t tz);
    void setSupabase(const char* url, const char* key);
    void setSbFingerprint(const char* fingerprint);   /* Optional TLS fingerprint */
    void setFlipper(const char* name, bool autoConnect);

    /* Validation */
    bool hasWiFiConfig() const;
    bool hasGatewayConfig() const;
    bool hasSupabaseConfig() const;
    bool hasFlipperConfig() const;

private:
    Preferences _prefs;
    WatchConfig _cfg;
};

#endif /* CONFIG_MANAGER_H */
