/**
 * Huonyx Watch – WiFi Manager
 * Handles WiFi connection with NVS credential storage
 */
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "hw_config.h"

enum WiFiMgrState : uint8_t {
    WIFI_MGR_IDLE = 0,
    WIFI_MGR_CONNECTING,
    WIFI_MGR_CONNECTED,
    WIFI_MGR_FAILED
};

typedef void (*WiFiStateCallback)(WiFiMgrState newState);

class WiFiManager {
public:
    WiFiManager();

    void begin();
    void loop();

    /* Connect to stored credentials */
    bool connect();
    bool connect(const char* ssid, const char* password);

    /* Save credentials to NVS */
    void saveCredentials(const char* ssid, const char* password);
    void clearCredentials();
    bool hasCredentials() const;

    /* State */
    WiFiMgrState getState() const { return _state; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
    const char* getSSID() const { return _ssid; }
    int getRSSI() const { return WiFi.RSSI(); }
    String getIP() const { return WiFi.localIP().toString(); }

    /* Fired on every state transition (called from loop()). */
    void onStateChange(WiFiStateCallback cb) { _onStateChange = cb; }

private:
    Preferences       _prefs;
    WiFiMgrState      _state;
    char              _ssid[WIFI_MAX_SSID_LEN];
    char              _password[WIFI_MAX_PASS_LEN];
    uint32_t          _connectStartMs;
    uint8_t           _retryCount;
    WiFiStateCallback _onStateChange;
    static constexpr uint8_t MAX_RETRIES = 3;

    void setState(WiFiMgrState s);
};

#endif /* WIFI_MANAGER_H */
