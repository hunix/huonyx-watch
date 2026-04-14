/**
 * Web Configuration Portal
 * Allows users to configure WiFi and Gateway settings via browser
 * Huonyx AI Smartwatch
 */

#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "config_manager.h"

class WebPortal {
public:
    WebPortal(ConfigManager* cfg);

    void begin();
    void loop();
    void stop();
    bool isRunning() const { return _running; }

    /* Start AP mode for initial setup */
    void startAP(const char* ssid = "HuonyxWatch", const char* pass = nullptr);
    void stopAP();

    /* Start in STA mode (alongside normal WiFi) */
    void startSTA();

private:
    WebServer   _server;
    ConfigManager* _cfg;
    bool        _running;
    bool        _apMode;

    void handleRoot();
    void handleSetup();
    void handleSave();
    void handleStatus();
    void handleNotFound();

    String generateHTML();
    String generateSetupHTML();
};

#endif /* WEB_PORTAL_H */
