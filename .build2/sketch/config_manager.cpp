#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\config_manager.cpp"
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
    _cfg.flipperAuto = false;
}

void ConfigManager::begin() {
    _prefs.begin(NVS_NAMESPACE, false);
    load();
}

void ConfigManager::load() {
    /* WiFi */
    if (_prefs.isKey(NVS_KEY_WIFI_SSID)) {
        _prefs.getString(NVS_KEY_WIFI_SSID, _cfg.wifiSSID, sizeof(_cfg.wifiSSID));
    }
    if (_prefs.isKey(NVS_KEY_WIFI_PASS)) {
        _prefs.getString(NVS_KEY_WIFI_PASS, _cfg.wifiPass, sizeof(_cfg.wifiPass));
    }

    /* Gateway */
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

    /* Display */
    if (_prefs.isKey(NVS_KEY_BRIGHTNESS)) {
        _cfg.brightness = _prefs.getUChar(NVS_KEY_BRIGHTNESS, 200);
    }
    if (_prefs.isKey(NVS_KEY_TIMEZONE)) {
        _cfg.timezone = _prefs.getChar(NVS_KEY_TIMEZONE, 3);
    }

    /* Supabase */
    if (_prefs.isKey(NVS_KEY_SB_URL)) {
        _prefs.getString(NVS_KEY_SB_URL, _cfg.sbUrl, sizeof(_cfg.sbUrl));
    }
    if (_prefs.isKey(NVS_KEY_SB_KEY)) {
        _prefs.getString(NVS_KEY_SB_KEY, _cfg.sbKey, sizeof(_cfg.sbKey));
    }

    /* Flipper */
    if (_prefs.isKey(NVS_KEY_FLIP_NAME)) {
        _prefs.getString(NVS_KEY_FLIP_NAME, _cfg.flipperName, sizeof(_cfg.flipperName));
    }
    if (_prefs.isKey(NVS_KEY_FLIP_AUTO)) {
        _cfg.flipperAuto = _prefs.getBool(NVS_KEY_FLIP_AUTO, false);
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
    _prefs.putString(NVS_KEY_SB_URL, _cfg.sbUrl);
    _prefs.putString(NVS_KEY_SB_KEY, _cfg.sbKey);
    _prefs.putString(NVS_KEY_FLIP_NAME, _cfg.flipperName);
    _prefs.putBool(NVS_KEY_FLIP_AUTO, _cfg.flipperAuto);
}

void ConfigManager::reset() {
    _prefs.clear();
    memset(&_cfg, 0, sizeof(_cfg));
    _cfg.gwPort = GATEWAY_DEFAULT_PORT;
    _cfg.brightness = 200;
    _cfg.timezone = 3;
    _cfg.flipperAuto = false;
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

void ConfigManager::setSupabase(const char* url, const char* key) {
    strncpy(_cfg.sbUrl, url, sizeof(_cfg.sbUrl) - 1);
    strncpy(_cfg.sbKey, key, sizeof(_cfg.sbKey) - 1);
    save();
}

void ConfigManager::setFlipper(const char* name, bool autoConnect) {
    strncpy(_cfg.flipperName, name, sizeof(_cfg.flipperName) - 1);
    _cfg.flipperAuto = autoConnect;
    save();
}

bool ConfigManager::hasWiFiConfig() const {
    return _cfg.wifiSSID[0] != '\0';
}

bool ConfigManager::hasGatewayConfig() const {
    return _cfg.gwHost[0] != '\0' && _cfg.gwToken[0] != '\0';
}

bool ConfigManager::hasSupabaseConfig() const {
    return _cfg.sbUrl[0] != '\0' && _cfg.sbKey[0] != '\0';
}

bool ConfigManager::hasFlipperConfig() const {
    return true;  /* Flipper can work without a specific name (scans for any) */
}
