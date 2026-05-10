/**
 * Huonyx Watch – WiFi Manager Implementation
 */
#include "wifi_manager.h"

WiFiManager::WiFiManager()
    : _state(WIFI_MGR_IDLE)
    , _connectStartMs(0)
    , _retryCount(0)
    , _onStateChange(nullptr)
{
    memset(_ssid, 0, sizeof(_ssid));
    memset(_password, 0, sizeof(_password));
}

void WiFiManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);

    /* Load stored credentials */
    _prefs.begin("huonyx-wifi", true); /* read-only */
    String ssid = _prefs.getString("ssid", "");
    String pass = _prefs.getString("pass", "");
    _prefs.end();

    if (ssid.length() > 0) {
        strncpy(_ssid, ssid.c_str(), sizeof(_ssid) - 1);
        strncpy(_password, pass.c_str(), sizeof(_password) - 1);
        Serial.printf("[WIFI] Loaded credentials for '%s'\n", _ssid);
    } else {
        Serial.println("[WIFI] No stored credentials");
    }
}

void WiFiManager::loop() {
    if (_state == WIFI_MGR_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            setState(WIFI_MGR_CONNECTED);
            Serial.printf("[WIFI] Connected! IP: %s RSSI: %d\n",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else if (millis() - _connectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
            _retryCount++;
            if (_retryCount >= MAX_RETRIES) {
                setState(WIFI_MGR_FAILED);
                Serial.println("[WIFI] Connection failed after max retries");
            } else {
                Serial.printf("[WIFI] Retry %d/%d\n", _retryCount, MAX_RETRIES);
                WiFi.disconnect();
                delay(100);
                WiFi.begin(_ssid, _password);
                _connectStartMs = millis();
            }
        }
    } else if (_state == WIFI_MGR_CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WIFI] Connection lost, reconnecting...");
            setState(WIFI_MGR_CONNECTING);
            _retryCount = 0;
            _connectStartMs = millis();
            WiFi.begin(_ssid, _password);
        }
    }
}

bool WiFiManager::connect() {
    if (_ssid[0] == '\0') {
        Serial.println("[WIFI] No credentials to connect with");
        return false;
    }
    return connect(_ssid, _password);
}

bool WiFiManager::connect(const char* ssid, const char* password) {
    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    _ssid[sizeof(_ssid) - 1] = '\0';
    strncpy(_password, password, sizeof(_password) - 1);
    _password[sizeof(_password) - 1] = '\0';

    Serial.printf("[WIFI] Connecting to '%s'...\n", _ssid);
    /* If portal is up (AP_STA), preserve the AP so the phone keeps its TCP
     * connection long enough to receive the success page. Otherwise switch to
     * pure STA. */
    if (WiFi.getMode() != WIFI_AP_STA) {
        WiFi.mode(WIFI_STA);
    }
    WiFi.disconnect(false);  /* don't power off the radio */
    delay(100);
    WiFi.begin(_ssid, _password);

    setState(WIFI_MGR_CONNECTING);
    _connectStartMs = millis();
    _retryCount = 0;
    return true;
}

void WiFiManager::saveCredentials(const char* ssid, const char* password) {
    _prefs.begin("huonyx-wifi", false); /* read-write */
    _prefs.putString("ssid", ssid);
    _prefs.putString("pass", password);
    _prefs.end();

    strncpy(_ssid, ssid, sizeof(_ssid) - 1);
    _ssid[sizeof(_ssid) - 1] = '\0';
    strncpy(_password, password, sizeof(_password) - 1);
    _password[sizeof(_password) - 1] = '\0';
    Serial.printf("[WIFI] Credentials saved for '%s'\n", ssid);
}

void WiFiManager::clearCredentials() {
    _prefs.begin("huonyx-wifi", false);
    _prefs.clear();
    _prefs.end();
    _ssid[0] = '\0';
    _password[0] = '\0';
    Serial.println("[WIFI] Credentials cleared");
}

bool WiFiManager::hasCredentials() const {
    return _ssid[0] != '\0';
}

void WiFiManager::setState(WiFiMgrState s) {
    if (_state == s) return;
    _state = s;
    if (_onStateChange) _onStateChange(s);
}
