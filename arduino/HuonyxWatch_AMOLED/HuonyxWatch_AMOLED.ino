/**
 * ╔══════════════════════════════════════════════════════════╗
 * ║  HUONYX WATCH – AMOLED Edition                          ║
 * ║  Board: Waveshare ESP32-S3-Touch-AMOLED-2.06            ║
 * ║  Display: CO5300 412x412 AMOLED (QSPI)                 ║
 * ║  Touch: FT3168 capacitive                               ║
 * ║  Gateway: HoC via Tailscale funnel (WSS)                ║
 * ╚══════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>

#include "hw_config.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "wifi_manager.h"
#include "gateway_client.h"
#include "ui_manager.h"

/* ── USB Serial for ESP32-S3 ──────────────────────────── */
/* ESP32-S3 uses native USB CDC - Serial works via USB */

/* ── Global Objects ───────────────────────────────────── */
TouchDriver   touch;
WiFiManager   wifi;
GatewayClient gateway;
UIManager     ui;

/* ── Timing ───────────────────────────────────────────── */
static uint32_t lastTimeUpdateMs = 0;
static uint32_t lastGwCheckMs = 0;
static uint32_t bootTimeMs = 0;

/* ── Forward Declarations ─────────────────────────────── */
void onGatewayStateChange(GatewayState state);
void onChatDelta(const char* runId, const char* text, bool isFinal);
void onSessionList(JsonArrayConst sessions);
void updateTime();
void handleSerialCommand(String &cmd);

/* ══════════════════════════════════════════════════════════
 *  SETUP
 * ══════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    delay(500); /* Let USB CDC stabilize */
    Serial.println();
    Serial.println("========================================");
    Serial.println("  HUONYX WATCH – AMOLED Edition v" FIRMWARE_VERSION);
    Serial.println("  ESP32-S3 + CO5300 412x412");
    Serial.println("========================================");

    bootTimeMs = millis();

    /* ── Step 1: Display ──────────────────────────────── */
    Serial.println("[BOOT] Initializing display...");
    if (!display_init()) {
        Serial.println("[BOOT] FATAL: Display init failed!");
        while (1) delay(1000);
    }
    Serial.println("[BOOT] Display OK (412x412 AMOLED)");

    /* Show boot splash */
    gfx->setTextSize(3);
    gfx->setTextColor(0x07FF); /* Cyan */
    gfx->setCursor(SCREEN_CENTER_X - 72, SCREEN_CENTER_Y - 30);
    gfx->print("HUONYX");
    gfx->setTextSize(1);
    gfx->setTextColor(0x7BEF); /* Grey */
    gfx->setCursor(SCREEN_CENTER_X - 48, SCREEN_CENTER_Y + 10);
    gfx->print("Initializing...");

    /* ── Step 2: Touch ────────────────────────────────── */
    Serial.println("[BOOT] Initializing touch...");
    if (!touch.begin()) {
        Serial.println("[BOOT] WARNING: Touch init failed (continuing)");
    } else {
        Serial.println("[BOOT] Touch OK (FT3168)");
    }

    /* ── Step 3: WiFi ─────────────────────────────────── */
    Serial.println("[BOOT] Initializing WiFi...");
    wifi.begin();
    if (wifi.hasCredentials()) {
        wifi.connect();
        /* Wait briefly for connection */
        uint32_t wifiStart = millis();
        while (!wifi.isConnected() && millis() - wifiStart < 8000) {
            wifi.loop();
            delay(100);
        }
        if (wifi.isConnected()) {
            Serial.printf("[BOOT] WiFi connected: %s\n", wifi.getIP().c_str());
            /* Sync NTP time */
            configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
            Serial.println("[BOOT] NTP time sync started (GMT+3)");
        } else {
            Serial.println("[BOOT] WiFi not connected yet (will retry in background)");
        }
    } else {
        Serial.println("[BOOT] No WiFi credentials stored");
        Serial.println("[BOOT] Use Serial to configure: WIFI_SET ssid password");
    }

    /* ── Step 4: Gateway ──────────────────────────────── */
    Serial.println("[BOOT] Setting up gateway client...");
    gateway.onStateChange(onGatewayStateChange);
    gateway.onChatDelta(onChatDelta);
    gateway.onSessionList(onSessionList);

    if (wifi.isConnected()) {
        gateway.begin(GATEWAY_HOST, GATEWAY_PORT, GATEWAY_PASSWORD, GATEWAY_USE_SSL);
    }
    Serial.println("[BOOT] Gateway client ready");

    /* ── Step 5: UI ───────────────────────────────────── */
    Serial.println("[BOOT] Initializing UI...");
    ui.begin(gfx, &touch);
    ui.setGateway(&gateway);
    ui.setWifiConnected(wifi.isConnected());
    ui.setGatewayState(gateway.getState());
    Serial.println("[BOOT] UI OK");

    /* ── Boot Complete ────────────────────────────────── */
    Serial.println("========================================");
    Serial.printf("[BOOT] COMPLETE in %lu ms\n", millis() - bootTimeMs);
    Serial.println("[BOOT] Swipe left/right to navigate");
    Serial.println("[BOOT] Long-press in chat for quick replies");
    Serial.println("========================================");

    ui.showNotification("Huonyx Ready", 0x07E0); /* Green */
}

/* ══════════════════════════════════════════════════════════
 *  LOOP
 * ══════════════════════════════════════════════════════════ */
void loop() {
    uint32_t now = millis();

    /* WiFi management */
    wifi.loop();

    /* Gateway WebSocket */
    if (wifi.isConnected()) {
        /* Start gateway if not yet started */
        if (gateway.getState() == GW_DISCONNECTED) {
            gateway.begin(GATEWAY_HOST, GATEWAY_PORT, GATEWAY_PASSWORD, GATEWAY_USE_SSL);
        }
        gateway.loop();
    }

    /* Update UI state */
    ui.setWifiConnected(wifi.isConnected());
    ui.setGatewayState(gateway.getState());

    /* Update time (simple uptime-based for now, RTC can be added later) */
    if (now - lastTimeUpdateMs >= 1000) {
        lastTimeUpdateMs = now;
        updateTime();
    }

    /* UI render */
    ui.update();

    /* Serial command interface */
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        handleSerialCommand(cmd);
    }

    /* Small yield for system tasks */
    delay(1);
}

/* ══════════════════════════════════════════════════════════
 *  CALLBACKS
 * ══════════════════════════════════════════════════════════ */
void onGatewayStateChange(GatewayState state) {
    const char* stateNames[] = {"DISCONNECTED", "CONNECTING", "CONNECTED", "AUTHENTICATED", "ERROR"};
    Serial.printf("[GW] State: %s\n", stateNames[state]);

    if (state == GW_AUTHENTICATED) {
        ui.showNotification("Gateway Connected", CLR_SUCCESS);
        /* Request session list */
        gateway.requestSessionList(5);
    } else if (state == GW_ERROR) {
        ui.showNotification("Gateway Error", CLR_ERROR);
    }
}

void onChatDelta(const char* runId, const char* text, bool isFinal) {
    if (isFinal) {
        ui.addChatMessage(text, false);
        ui.setTypingIndicator(false);
        Serial.printf("[CHAT] Agent: %s\n", text);
    } else {
        ui.setTypingIndicator(true);
    }
}

void onSessionList(JsonArrayConst sessions) {
    Serial.printf("[GW] Got %d sessions\n", sessions.size());
    for (JsonObjectConst s : sessions) {
        const char* key = s["key"] | "?";
        const char* title = s["title"] | s["derivedTitle"] | key;
        Serial.printf("  - %s: %s\n", key, title);
    }
}

/* ══════════════════════════════════════════════════════════
 *  TIME (simple uptime-based, RTC integration TODO)
 * ══════════════════════════════════════════════════════════ */
void updateTime() {
    /* Use NTP time if WiFi connected, otherwise uptime */
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        ui.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        ui.setDate(timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
    } else {
        /* Fallback: uptime */
        uint32_t upSec = millis() / 1000;
        ui.setTime((upSec / 3600) % 24, (upSec / 60) % 60, upSec % 60);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SERIAL COMMAND INTERFACE
 * ══════════════════════════════════════════════════════════ */
void handleSerialCommand(String &cmd) {
    if (cmd.startsWith("WIFI_SET ")) {
        /* WIFI_SET ssid password */
        int spaceIdx = cmd.indexOf(' ', 9);
        if (spaceIdx > 0) {
            String ssid = cmd.substring(9, spaceIdx);
            String pass = cmd.substring(spaceIdx + 1);
            Serial.printf("[CMD] Setting WiFi: '%s'\n", ssid.c_str());
            wifi.saveCredentials(ssid.c_str(), pass.c_str());
            wifi.connect(ssid.c_str(), pass.c_str());
        } else {
            Serial.println("[CMD] Usage: WIFI_SET <ssid> <password>");
        }
    } else if (cmd.startsWith("CHAT ")) {
        /* CHAT message text */
        String msg = cmd.substring(5);
        if (gateway.isConnected()) {
            gateway.sendMessage(gateway.getSessionKey(), msg.c_str());
            ui.addChatMessage(msg.c_str(), true);
            Serial.printf("[CMD] Sent: %s\n", msg.c_str());
        } else {
            Serial.println("[CMD] Gateway not connected");
        }
    } else if (cmd == "STATUS") {
        Serial.println("=== STATUS ===");
        Serial.printf("WiFi: %s (RSSI: %d)\n",
                     wifi.isConnected() ? "Connected" : "Disconnected", wifi.getRSSI());
        Serial.printf("Gateway: %s\n",
                     gateway.isConnected() ? "Authenticated" : "Not connected");
        Serial.printf("Session: %s\n", gateway.getSessionKey());
        Serial.printf("Heap: %d bytes free\n", ESP.getFreeHeap());
        Serial.printf("Uptime: %lu sec\n", millis() / 1000);
    } else if (cmd == "SESSIONS") {
        gateway.requestSessionList(10);
    } else if (cmd.startsWith("SESSION ")) {
        String key = cmd.substring(8);
        gateway.setSessionKey(key.c_str());
        Serial.printf("[CMD] Session set to: %s\n", key.c_str());
    } else if (cmd == "HELP") {
        Serial.println("=== COMMANDS ===");
        Serial.println("WIFI_SET <ssid> <password>  - Set WiFi credentials");
        Serial.println("CHAT <message>              - Send chat message");
        Serial.println("STATUS                      - Show system status");
        Serial.println("SESSIONS                    - List gateway sessions");
        Serial.println("SESSION <key>               - Switch session");
        Serial.println("HELP                        - Show this help");
    } else {
        Serial.printf("[CMD] Unknown: '%s' (type HELP)\n", cmd.c_str());
    }
}
