/**
 * Configuration Manager Implementation
 * Huonyx AI Smartwatch
 *
 * NVS write strategy: each setter writes ONLY its own key(s), not all 14.
 * The full save() is used only during web portal commit (all fields at once)
 * and factory reset. This prevents excessive NVS wear from frequent calls.
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
    if (_prefs.isKey(NVS_KEY_GW_FINGERPRINT)) {
        _prefs.getString(NVS_KEY_GW_FINGERPRINT, _cfg.gwFingerprint, sizeof(_cfg.gwFingerprint));
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
    if (_prefs.isKey(NVS_KEY_SB_FINGERPRINT)) {
        _prefs.getString(NVS_KEY_SB_FINGERPRINT, _cfg.sbFingerprint, sizeof(_cfg.sbFingerprint));
    }

    /* Flipper */
    if (_prefs.isKey(NVS_KEY_FLIP_NAME)) {
        _prefs.getString(NVS_KEY_FLIP_NAME, _cfg.flipperName, sizeof(_cfg.flipperName));
    }
    if (_prefs.isKey(NVS_KEY_FLIP_AUTO)) {
        _cfg.flipperAuto = _prefs.getBool(NVS_KEY_FLIP_AUTO, false);
    }
}

/* Full atomic save — used by web portal commit and factory reset only. */
void ConfigManager::save() {
    _prefs.putString(NVS_KEY_WIFI_SSID, _cfg.wifiSSID);
    _prefs.putString(NVS_KEY_WIFI_PASS, _cfg.wifiPass);
    _prefs.putString(NVS_KEY_GW_HOST, _cfg.gwHost);
    _prefs.putUShort(NVS_KEY_GW_PORT, _cfg.gwPort);
    _prefs.putString(NVS_KEY_GW_TOKEN, _cfg.gwToken);
    _prefs.putBool(NVS_KEY_GW_USE_SSL, _cfg.gwUseSSL);
    _prefs.putString(NVS_KEY_GW_FINGERPRINT, _cfg.gwFingerprint);
    _prefs.putUChar(NVS_KEY_BRIGHTNESS, _cfg.brightness);
    _prefs.putChar(NVS_KEY_TIMEZONE, _cfg.timezone);
    _prefs.putString(NVS_KEY_SB_URL, _cfg.sbUrl);
    _prefs.putString(NVS_KEY_SB_KEY, _cfg.sbKey);
    _prefs.putString(NVS_KEY_SB_FINGERPRINT, _cfg.sbFingerprint);
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
    /* save() not needed — clear() already wiped NVS, defaults will load correctly */
}

/* ── Setters: write ONLY their own NVS keys (not all 14) ──── */

void ConfigManager::setWiFi(const char* ssid, const char* pass) {
    strncpy(_cfg.wifiSSID, ssid ? ssid : "", sizeof(_cfg.wifiSSID) - 1);
    strncpy(_cfg.wifiPass, pass ? pass : "", sizeof(_cfg.wifiPass) - 1);
    _prefs.putString(NVS_KEY_WIFI_SSID, _cfg.wifiSSID);
    _prefs.putString(NVS_KEY_WIFI_PASS, _cfg.wifiPass);
}

void ConfigManager::setGateway(const char* host, uint16_t port,
                               const char* token, bool ssl) {
    strncpy(_cfg.gwHost,  host  ? host  : "", sizeof(_cfg.gwHost)  - 1);
    strncpy(_cfg.gwToken, token ? token : "", sizeof(_cfg.gwToken) - 1);
    _cfg.gwPort   = port;
    _cfg.gwUseSSL = ssl;
    _prefs.putString(NVS_KEY_GW_HOST,    _cfg.gwHost);
    _prefs.putUShort(NVS_KEY_GW_PORT,    _cfg.gwPort);
    _prefs.putString(NVS_KEY_GW_TOKEN,   _cfg.gwToken);
    _prefs.putBool(NVS_KEY_GW_USE_SSL,   _cfg.gwUseSSL);
}

void ConfigManager::setGwFingerprint(const char* fingerprint) {
    strncpy(_cfg.gwFingerprint, fingerprint ? fingerprint : "",
            sizeof(_cfg.gwFingerprint) - 1);
    _prefs.putString(NVS_KEY_GW_FINGERPRINT, _cfg.gwFingerprint);
}

void ConfigManager::setSupabase(const char* url, const char* key) {
    strncpy(_cfg.sbUrl, url ? url : "", sizeof(_cfg.sbUrl) - 1);
    strncpy(_cfg.sbKey, key ? key : "", sizeof(_cfg.sbKey) - 1);
    _prefs.putString(NVS_KEY_SB_URL, _cfg.sbUrl);
    _prefs.putString(NVS_KEY_SB_KEY, _cfg.sbKey);
}

void ConfigManager::setSbFingerprint(const char* fingerprint) {
    strncpy(_cfg.sbFingerprint, fingerprint ? fingerprint : "",
            sizeof(_cfg.sbFingerprint) - 1);
    _prefs.putString(NVS_KEY_SB_FINGERPRINT, _cfg.sbFingerprint);
}

void ConfigManager::setFlipper(const char* name, bool autoConnect) {
    strncpy(_cfg.flipperName, name ? name : "", sizeof(_cfg.flipperName) - 1);
    _cfg.flipperAuto = autoConnect;
    _prefs.putString(NVS_KEY_FLIP_NAME, _cfg.flipperName);
    _prefs.putBool(NVS_KEY_FLIP_AUTO,   _cfg.flipperAuto);
}

void ConfigManager::setBrightness(uint8_t val) {
    _cfg.brightness = val;
    _prefs.putUChar(NVS_KEY_BRIGHTNESS, val);
}

void ConfigManager::setTimezone(int8_t tz) {
    _cfg.timezone = tz;
    _prefs.putChar(NVS_KEY_TIMEZONE, tz);
}

/* ── Validators ────────────────────────────────────────────── */

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
    return true;  /* Works without a specific name - scans for any Flipper */
}
