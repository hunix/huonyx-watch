/**
 * Huonyx Watch – WiFi Manager Implementation
 */
#include "wifi_manager.h"

WiFiManager::WiFiManager()
    : _state(WIFI_MGR_IDLE)
    , _connectStartMs(0)
    , _retryCount(0)
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
            _state = WIFI_MGR_CONNECTED;
            Serial.printf("[WIFI] Connected! IP: %s RSSI: %d\n",
                         WiFi.localIP().toString().c_str(), WiFi.RSSI());
        } else if (millis() - _connectStartMs > WIFI_CONNECT_TIMEOUT_MS) {
            _retryCount++;
            if (_retryCount >= MAX_RETRIES) {
                _state = WIFI_MGR_FAILED;
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
            _state = WIFI_MGR_CONNECTING;
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
    strncpy(_password, password, sizeof(_password) - 1);

    Serial.printf("[WIFI] Connecting to '%s'...\n", _ssid);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(_ssid, _password);

    _state = WIFI_MGR_CONNECTING;
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
    strncpy(_password, password, sizeof(_password) - 1);
    Serial.printf("[WIFI] Credentials saved for '%s'\n", ssid);
}

bool WiFiManager::hasCredentials() const {
    return _ssid[0] != '\0';
}
