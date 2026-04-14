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
    char     wifiSSID[33];
    char     wifiPass[65];
    char     gwHost[64];
    uint16_t gwPort;
    char     gwToken[128];
    bool     gwUseSSL;
    uint8_t  brightness;    /* 0-255 */
    int8_t   timezone;      /* UTC offset in hours */
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
    void setBrightness(uint8_t val);
    void setTimezone(int8_t tz);

    /* Validation */
    bool hasWiFiConfig() const;
    bool hasGatewayConfig() const;

private:
    Preferences _prefs;
    WatchConfig _cfg;
};

#endif /* CONFIG_MANAGER_H */
