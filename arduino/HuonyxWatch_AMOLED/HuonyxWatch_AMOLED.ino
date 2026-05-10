/**
 * ╔══════════════════════════════════════════════════════════╗
 * ║  HUONYX WATCH – AMOLED Edition                          ║
 * ║  Board: Waveshare ESP32-S3-Touch-AMOLED-2.06            ║
 * ║  Display: CO5300 412x412 AMOLED (QSPI)                 ║
 * ║  Touch: FT3168 capacitive                               ║
 * ║  RTC: PCF85063                                          ║
 * ║  Gateway: HoC via Tailscale funnel (WSS)                ║
 * ╚══════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <time.h>
#include <sys/time.h>

#include "hw_config.h"
#include "display_driver.h"
#include "touch_driver.h"
#include "wifi_manager.h"
#include "wifi_portal.h"
#include "gateway_client.h"
#include "rtc_driver.h"
#include "ui_manager.h"

/* ── Global Objects ───────────────────────────────────── */
TouchDriver   touch;
WiFiManager   wifi;
WiFiPortal    portal;
GatewayClient gateway;
RTCDriver     rtc;
UIManager     ui;

/* ── Timing ───────────────────────────────────────────── */
static uint32_t lastTimeUpdateMs   = 0;
static uint32_t lastRtcWritebackMs = 0;
static uint32_t lastPortalCheckMs  = 0;
static uint32_t bootTimeMs         = 0;

/* ── Time state ───────────────────────────────────────── */
static bool ntpSyncedThisSession = false;
static bool tzApplied = false;

/* ── Forward Declarations ─────────────────────────────── */
void onGatewayStateChange(GatewayState state);
void onChatDelta(const char* runId, const char* text, bool isFinal);
void onSessionList(JsonArrayConst sessions);
void onWifiStateChange(WiFiMgrState state);
void onPortalSave(const char* ssid, const char* pass);
void updateTime();
void applyStoredTimezone();
void startNtpSync();
void writeNtpToRtc();
void handleSerialCommand(String &cmd);

/* ══════════════════════════════════════════════════════════
 *  SETUP
 * ══════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    /* Long delay so USB CDC fully enumerates with the host BEFORE we touch
     * anything that could spike current (display, WiFi, softAP). If the watch
     * resets while CDC is mid-enumeration the host marks the port unusable
     * and you have to BOOT-button-recover. */
    delay(1500);
    Serial.println();
    Serial.println("========================================");
    Serial.println("  HUONYX WATCH – AMOLED Edition v" FIRMWARE_VERSION);
    Serial.println("  ESP32-S3 + CO5300 412x412");
    Serial.println("========================================");
    Serial.flush();

    bootTimeMs = millis();

    /* ── Display ──────────────────────────────────────── */
    Serial.println("[BOOT] Initializing display...");
    Serial.flush();
    if (!display_init()) {
        Serial.println("[BOOT] FATAL: Display init failed!");
        while (1) delay(1000);
    }
    Serial.println("[BOOT] Display OK (412x412 AMOLED)");
    Serial.flush();
    /* Let the AMOLED boost converter settle before any further current draw. */
    delay(200);

    /* Boot splash */
    gfx->setTextSize(3);
    gfx->setTextColor(0x07FF);
    gfx->setCursor(SCREEN_CENTER_X - 72, SCREEN_CENTER_Y - 30);
    gfx->print("HUONYX");
    gfx->setTextSize(1);
    gfx->setTextColor(0x7BEF);
    gfx->setCursor(SCREEN_CENTER_X - 48, SCREEN_CENTER_Y + 10);
    gfx->print("Initializing...");

    /* ── Touch (also initializes shared I2C bus) ──────── */
    Serial.println("[BOOT] Initializing touch...");
    if (!touch.begin()) {
        Serial.println("[BOOT] WARNING: Touch init failed (continuing)");
    } else {
        Serial.println("[BOOT] Touch OK (FT3168)");
    }

    /* ── RTC (shares the I2C bus touch just brought up) ─ */
    Serial.println("[BOOT] Initializing RTC...");
    if (rtc.begin()) {
        Serial.println("[BOOT] RTC OK (PCF85063)");
    } else {
        Serial.println("[BOOT] RTC not found (continuing)");
    }

    /* ── Apply stored timezone before any time-related calls ── */
    applyStoredTimezone();

    /* If RTC has valid time, seed the system clock now so the watchface
     * shows real time even before WiFi/NTP. */
    if (rtc.isPresent() && rtc.isValid()) {
        rtc.seedSystemClock();
    }

    /* ── WiFi ─────────────────────────────────────────── */
    Serial.println("[BOOT] Initializing WiFi...");
    Serial.flush();
    /* Settle before bringing WiFi up — avoids a current spike on top of the
     * AMOLED's own startup load. */
    delay(200);
    wifi.onStateChange(onWifiStateChange);
    wifi.begin();
    if (wifi.hasCredentials()) {
        wifi.connect();
    } else {
        Serial.println("[BOOT] No WiFi credentials stored");
    }
    Serial.flush();

    /* ── Captive Portal hooks ─────────────────────────── */
    portal.onSave(onPortalSave);

    /* ── Gateway ──────────────────────────────────────── */
    Serial.println("[BOOT] Setting up gateway client...");
    gateway.onStateChange(onGatewayStateChange);
    gateway.onChatDelta(onChatDelta);
    gateway.onSessionList(onSessionList);

    /* ── UI ───────────────────────────────────────────── */
    Serial.println("[BOOT] Initializing UI...");
    ui.begin(gfx, &touch);
    ui.setGateway(&gateway);
    ui.setWifi(&wifi);
    ui.setPortal(&portal);
    ui.setWifiConnected(wifi.isConnected());
    ui.setGatewayState(gateway.getState());
    Serial.println("[BOOT] UI OK");

    /* ── Boot Complete ────────────────────────────────── */
    Serial.println("========================================");
    Serial.printf("[BOOT] COMPLETE in %lu ms\n", millis() - bootTimeMs);
    Serial.println("========================================");
    Serial.flush();

    /* IMPORTANT: do NOT call portal.start() here. softAP power spike during
     * boot can brown-out the regulator on USB-only power, which crashes the
     * device before USB CDC has finished enumerating with the host — leaving
     * the watch unflashable until BOOT-button recovery. The user can launch
     * the portal manually from Settings → WiFi → Phone setup once the watch
     * is in steady state. */
    if (wifi.hasCredentials()) {
        ui.showNotification("Huonyx Ready", CLR_SUCCESS);
    } else {
        ui.showNotification("WiFi: open Settings", CLR_WARNING);
    }
}

/* ══════════════════════════════════════════════════════════
 *  LOOP
 * ══════════════════════════════════════════════════════════ */
void loop() {
    uint32_t now = millis();

    wifi.loop();

    if (portal.isActive()) {
        portal.loop();
    }

    /* Gateway WebSocket — only when connected to real WiFi (not portal AP). */
    if (wifi.isConnected() && !portal.isActive()) {
        if (gateway.getState() == GW_DISCONNECTED) {
            gateway.begin(GATEWAY_HOST, GATEWAY_PORT, GATEWAY_PASSWORD, GATEWAY_USE_SSL);
        }
        gateway.loop();
    }

    ui.setWifiConnected(wifi.isConnected());
    ui.setGatewayState(gateway.getState());

    /* Tick time every second */
    if (now - lastTimeUpdateMs >= 1000) {
        lastTimeUpdateMs = now;
        updateTime();
    }

    /* Persist NTP-corrected time back to the RTC every 10 minutes. */
    if (ntpSyncedThisSession && rtc.isPresent() &&
        now - lastRtcWritebackMs >= 600000) {
        lastRtcWritebackMs = now;
        writeNtpToRtc();
    }

    ui.update();

    /* Serial command interface */
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        handleSerialCommand(cmd);
    }

    delay(1);
}

/* ══════════════════════════════════════════════════════════
 *  TIME
 * ══════════════════════════════════════════════════════════ */
void applyStoredTimezone() {
    Preferences p;
    p.begin("huonyx-time", true);
    String tz = p.getString("tz", "UTC0");
    p.end();
    setenv("TZ", tz.c_str(), 1);
    tzset();
    tzApplied = true;
    Serial.printf("[TIME] TZ applied: %s\n", tz.c_str());
}

void startNtpSync() {
    /* Use TZ env var (already set) — pass empty offsets so configTime doesn't
     * synthesize its own TZ string and overwrite ours. */
    configTzTime(getenv("TZ") ? getenv("TZ") : "UTC0",
                 "pool.ntp.org", "time.nist.gov");
    Serial.println("[TIME] NTP sync requested");
    /* Mark synced once we get a real time. updateTime() will check periodically. */
}

void writeNtpToRtc() {
    time_t now;
    time(&now);
    if (now < 1700000000) return;  /* Sanity: 2023-11-14 or later */
    struct tm t;
    localtime_r(&now, &t);
    if (rtc.writeTime(&t)) {
        Serial.println("[TIME] RTC updated from system clock");
    }
}

void updateTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 0)) {
        /* If this is the first valid time-of-year we've seen post-NTP, mark synced
         * and write back to RTC. */
        if (!ntpSyncedThisSession && timeinfo.tm_year + 1900 >= 2024) {
            ntpSyncedThisSession = true;
            writeNtpToRtc();
            lastRtcWritebackMs = millis();
        }
        ui.setTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        ui.setDate(timeinfo.tm_mday, timeinfo.tm_mon + 1,
                   timeinfo.tm_year + 1900, timeinfo.tm_wday);
    }
}

/* ══════════════════════════════════════════════════════════
 *  CALLBACKS
 * ══════════════════════════════════════════════════════════ */
void onGatewayStateChange(GatewayState state) {
    const char* stateNames[] = {"DISCONNECTED","CONNECTING","CONNECTED","AUTHENTICATED","ERROR"};
    Serial.printf("[GW] State: %s\n", stateNames[state]);

    if (state == GW_AUTHENTICATED) {
        ui.showNotification("Gateway Connected", CLR_SUCCESS);
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

void onWifiStateChange(WiFiMgrState state) {
    if (state == WIFI_MGR_CONNECTED) {
        Serial.println("[WIFI->NTP] WiFi up — kicking NTP");
        ntpSyncedThisSession = false;  /* re-sync each connect */
        startNtpSync();
        if (portal.isActive()) {
            /* Successful connect via portal — tear it down. */
            Serial.println("[PORTAL] WiFi up, stopping AP");
            portal.stop();
            ui.navigateTo(SCREEN_WATCHFACE);
            ui.showNotification("WiFi Connected", CLR_SUCCESS);
        }
    } else if (state == WIFI_MGR_FAILED) {
        /* Don't auto-launch the portal — softAP power spike can brown-out the
         * regulator. User can manually start the portal from Settings → WiFi
         * → Phone setup. */
        Serial.println("[WIFI] Connect failed");
        ui.showNotification("WiFi failed", CLR_ERROR);
    }
}

void onPortalSave(const char* ssid, const char* pass) {
    Serial.printf("[PORTAL] Save: %s\n", ssid);
    wifi.saveCredentials(ssid, pass);
    /* Try to connect on the STA interface. The portal AP stays up briefly so
     * the phone can see the success page; loop will tear it down on success. */
    wifi.connect(ssid, pass);
}

/* ══════════════════════════════════════════════════════════
 *  SERIAL COMMAND INTERFACE
 * ══════════════════════════════════════════════════════════ */
void handleSerialCommand(String &cmd) {
    if (cmd.startsWith("WIFI_SET ")) {
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
    } else if (cmd == "WIFI_FORGET") {
        wifi.clearCredentials();
        Serial.println("[CMD] WiFi credentials cleared");
    } else if (cmd == "PORTAL") {
        portal.start("HuonyxWatch-Setup");
        ui.navigateTo(SCREEN_PORTAL);
        Serial.println("[CMD] Portal started");
    } else if (cmd.startsWith("TZ ")) {
        String tz = cmd.substring(3);
        Preferences p;
        p.begin("huonyx-time", false);
        p.putString("tz", tz);
        p.end();
        setenv("TZ", tz.c_str(), 1);
        tzset();
        Serial.printf("[CMD] TZ set to: %s\n", tz.c_str());
    } else if (cmd == "TIME") {
        time_t now;
        time(&now);
        struct tm t;
        localtime_r(&now, &t);
        Serial.printf("[TIME] %04d-%02d-%02d %02d:%02d:%02d  TZ=%s  RTC=%s\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec,
                      getenv("TZ") ? getenv("TZ") : "?",
                      rtc.isValid() ? "valid" : "invalid");
    } else if (cmd == "NTP") {
        startNtpSync();
        Serial.println("[CMD] NTP sync requested");
    } else if (cmd.startsWith("CHAT ")) {
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
        Serial.printf("Portal: %s\n", portal.isActive() ? "active" : "off");
        Serial.printf("Gateway: %s\n", gateway.isConnected() ? "Authenticated" : "Not connected");
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
        Serial.println("WIFI_SET <ssid> <pass>  - Set WiFi credentials");
        Serial.println("WIFI_FORGET             - Clear WiFi credentials");
        Serial.println("PORTAL                  - Start captive portal");
        Serial.println("TZ <posix-string>       - Set timezone (e.g. 'TRT-3')");
        Serial.println("TIME                    - Show current time");
        Serial.println("NTP                     - Force NTP resync");
        Serial.println("CHAT <message>          - Send chat message");
        Serial.println("STATUS                  - Show system status");
        Serial.println("SESSIONS                - List gateway sessions");
        Serial.println("SESSION <key>           - Switch session");
        Serial.println("HELP                    - Show this help");
    } else {
        Serial.printf("[CMD] Unknown: '%s' (type HELP)\n", cmd.c_str());
    }
}
