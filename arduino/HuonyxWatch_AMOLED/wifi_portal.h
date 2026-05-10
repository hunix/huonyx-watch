/**
 * Huonyx Watch – WiFi Captive Portal
 * SoftAP + WebServer + DNS catch-all so a phone can configure WiFi
 * without USB serial.
 */
#ifndef WIFI_PORTAL_H
#define WIFI_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

typedef void (*PortalSaveCallback)(const char* ssid, const char* pass);

class WiFiPortal {
public:
    WiFiPortal();

    /* Start AP + web server. apPass="" means open network. */
    void start(const char* apSsid, const char* apPass = "");
    void stop();
    void loop();

    bool        isActive() const { return _active; }
    const char* getApIp() const  { return _apIp; }
    const char* getApSsid() const { return _apSsid; }

    /* Fired after the user submits the form. */
    void onSave(PortalSaveCallback cb) { _onSave = cb; }

private:
    WebServer          _server;
    DNSServer          _dns;
    bool               _active;
    char               _apSsid[33];
    char               _apIp[16];
    PortalSaveCallback _onSave;

    static WiFiPortal* _instance;
    static void handleRoot();
    static void handleScan();
    static void handleSave();
    static void handleNotFound();

    String buildIndexHtml();
    String escapeHtml(const String& s);
};

#endif /* WIFI_PORTAL_H */
