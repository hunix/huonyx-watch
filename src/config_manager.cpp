/**
 * Configuration Manager Implementation
 * Huonyx AI Smartwatch
 */

#include "config_manager.h"

ConfigManager::ConfigManager() {
    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.gwPort = GATEWAY_DEFAULT_PORT;
    _cfg.brightness = 200;
    _cfg.timezone = 3;  /* Default UTC+3 */
}

void ConfigManager::begin() {
    _prefs.begin(NVS_NAMESPACE, false);
    load();
}

void ConfigManager::load() {
    if (_prefs.isKey(NVS_KEY_WIFI_SSID)) {
        _prefs.getString(NVS_KEY_WIFI_SSID, _cfg.wifiSSID, sizeof(_cfg.wifiSSID));
    }
    if (_prefs.isKey(NVS_KEY_WIFI_PASS)) {
        _prefs.getString(NVS_KEY_WIFI_PASS, _cfg.wifiPass, sizeof(_cfg.wifiPass));
    }
    if (_prefs.isKey(NVS_KEY_GW_HOST)) {
        _prefs.getString(NVS_KEY_GW_HOST, _cfg.gwHost, sizeof(_cfg.gwHost));
    }
    if (_prefs.isKey(NVS_KEY_GW_PORT)) {
        _cfg.gwPort = _prefs.getUShort(NVS_KEY_GW_PORT, GATEWAY_DEFAULT_PORT);
    }
    if (_prefs.isKey(NVS_KEY_GW_TOKEN)) {
        _prefs.getString(NVS_KEY_GW_TOKEN, _cfg.gwToken, sizeof(_cfg.gwToken));
    }
    if (_prefs.isKey(NVS_KEY_GW_USE_SSL)) {
        _cfg.gwUseSSL = _prefs.getBool(NVS_KEY_GW_USE_SSL, false);
    }
    if (_prefs.isKey(NVS_KEY_BRIGHTNESS)) {
        _cfg.brightness = _prefs.getUChar(NVS_KEY_BRIGHTNESS, 200);
    }
    if (_prefs.isKey(NVS_KEY_TIMEZONE)) {
        _cfg.timezone = _prefs.getChar(NVS_KEY_TIMEZONE, 3);
    }
}

void ConfigManager::save() {
    _prefs.putString(NVS_KEY_WIFI_SSID, _cfg.wifiSSID);
    _prefs.putString(NVS_KEY_WIFI_PASS, _cfg.wifiPass);
    _prefs.putString(NVS_KEY_GW_HOST, _cfg.gwHost);
    _prefs.putUShort(NVS_KEY_GW_PORT, _cfg.gwPort);
    _prefs.putString(NVS_KEY_GW_TOKEN, _cfg.gwToken);
    _prefs.putBool(NVS_KEY_GW_USE_SSL, _cfg.gwUseSSL);
    _prefs.putUChar(NVS_KEY_BRIGHTNESS, _cfg.brightness);
    _prefs.putChar(NVS_KEY_TIMEZONE, _cfg.timezone);
}

void ConfigManager::reset() {
    _prefs.clear();
    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.gwPort = GATEWAY_DEFAULT_PORT;
    _cfg.brightness = 200;
    _cfg.timezone = 3;
    save();
}

void ConfigManager::setWiFi(const char* ssid, const char* pass) {
    strncpy(_cfg.wifiSSID, ssid, sizeof(_cfg.wifiSSID) - 1);
    strncpy(_cfg.wifiPass, pass, sizeof(_cfg.wifiPass) - 1);
    save();
}

void ConfigManager::setGateway(const char* host, uint16_t port, const char* token, bool ssl) {
    strncpy(_cfg.gwHost, host, sizeof(_cfg.gwHost) - 1);
    _cfg.gwPort = port;
    strncpy(_cfg.gwToken, token, sizeof(_cfg.gwToken) - 1);
    _cfg.gwUseSSL = ssl;
    save();
}

void ConfigManager::setBrightness(uint8_t val) {
    _cfg.brightness = val;
    _prefs.putUChar(NVS_KEY_BRIGHTNESS, val);
}

void ConfigManager::setTimezone(int8_t tz) {
    _cfg.timezone = tz;
    _prefs.putChar(NVS_KEY_TIMEZONE, tz);
}

bool ConfigManager::hasWiFiConfig() const {
    return _cfg.wifiSSID[0] != '\0';
}

bool ConfigManager::hasGatewayConfig() const {
    return _cfg.gwHost[0] != '\0' && _cfg.gwToken[0] != '\0';
}
