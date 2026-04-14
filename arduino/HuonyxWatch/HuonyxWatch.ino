/**
 * ╔══════════════════════════════════════════════════════════╗
 * ║           HUONYX AI SMARTWATCH FIRMWARE v2.1             ║
 * ║     ESP32-2424S012 (ESP32-C3 + GC9A01 + CST816D)       ║
 * ║                                                          ║
 * ║  A beautiful AI-era smartwatch interface for the         ║
 * ║  Huonyx (House of Claws) autonomous AI platform.        ║
 * ║                                                          ║
 * ║  Features:                                               ║
 * ║  - Digital watch face with battery & status indicators   ║
 * ║  - AI chat interface with quick replies                  ║
 * ║  - Flipper Zero BLE control screen                       ║
 * ║  - Supabase Realtime bridge for remote agent control     ║
 * ║  - WebSocket gateway client for HoC protocol             ║
 * ║  - Web-based configuration portal                        ║
 * ║  - Gesture navigation (swipe between screens)            ║
 * ║                                                          ║
 * ║  Compatible with: LVGL 9.x, ESP32 core 3.x,            ║
 * ║  ArduinoJson 7.x, NimBLE-Arduino 2.x                   ║
 * ╚══════════════════════════════════════════════════════════╝
 */

/* build_config.h MUST be first - defines USER_SETUP_LOADED, LV_CONF_INCLUDE_SIMPLE, NimBLE config */
#include "build_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>      /* Required before WebSockets on ESP32 core 3.x */
#include <time.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "hw_config.h"
#include "touch_driver.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "ui_manager.h"
#include "web_portal.h"

/* ── Global Objects ───────────────────────────────────── */
static TFT_eSPI        tft = TFT_eSPI();
static CST816D_Driver   touch(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST, TOUCH_I2C_ADDR);
static ConfigManager    configMgr;
static GatewayClient    gateway;
static FlipperBLE       flipper;
static SupabaseBridge   bridge;
static UIManager        ui;
static WebPortal        webPortal(&configMgr);

/* ── LVGL 9 Display Buffer ────────────────────────────── */
/* In LVGL 9, buffers are raw uint8_t arrays sized in bytes */
static uint8_t lvgl_buf1[LVGL_BUF_SIZE * 2];  /* RGB565 = 2 bytes per pixel */

/* ── Timing ───────────────────────────────────────────── */
static unsigned long lastTimeUpdate    = 0;
static unsigned long lastWiFiCheck     = 0;
static unsigned long lastBatteryCheck  = 0;
static unsigned long lastStatusBcast   = 0;
static unsigned long lastLvglTick      = 0;
static bool          wifiConnected     = false;
static bool          gatewayStarted    = false;
static bool          flipperStarted    = false;
static bool          bridgeStarted     = false;
static bool          ntpSynced         = false;

/* ── Accumulator for streaming chat deltas ────────────── */
static String currentRunId    = "";
static String accumulatedText = "";

/* ══════════════════════════════════════════════════════════
 *  LVGL 9 CALLBACKS
 * ══════════════════════════════════════════════════════════ */

/**
 * LVGL 9 flush callback.
 * Signature: void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
 * In LVGL 9, the pixel map is a raw byte buffer (RGB565 in our case).
 */
static void lvglFlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)px_map, w * h, true);
    tft.endWrite();

    lv_display_flush_ready(disp);
}

/**
 * LVGL 9 touch read callback.
 * Signature: void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
 */
static void lvglTouchReadCb(lv_indev_t* indev, lv_indev_data_t* data) {
    TouchPoint tp;
    if (touch.read(tp) && tp.pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/**
 * LVGL 9 tick callback - provides millisecond ticks to LVGL.
 */
static uint32_t lvglTickCb(void) {
    return millis();
}

/* ══════════════════════════════════════════════════════════
 *  GATEWAY CALLBACKS
 * ══════════════════════════════════════════════════════════ */

static void onChatDelta(const char* runId, const char* text, bool isFinal) {
    if (currentRunId != runId) {
        currentRunId = runId;
        accumulatedText = "";
    }

    accumulatedText += text;

    if (isFinal) {
        ui.updateAgentTyping(false);

        String displayText = accumulatedText;
        if (displayText.length() > CHAT_MAX_MSG_LEN - 1) {
            displayText = displayText.substring(0, CHAT_MAX_MSG_LEN - 4) + "...";
        }

        ui.addChatMessage(displayText.c_str(), false);
        currentRunId = "";
        accumulatedText = "";
    }
}

static void onGatewayStateChange(GatewayState newState) {
    ui.updateGatewayStatus(newState);
    Serial.printf("[MAIN] Gateway state: %d\n", newState);
}

/**
 * Called when the agent sends a flipper command via the HoC gateway
 */
static void onGatewayFlipperCommand(const char* command, const char* source) {
    Serial.printf("[GW-FLIP] Command from %s: %s\n", source, command);

    char logBuf[160];
    snprintf(logBuf, sizeof(logBuf), "[%s] %s", source, command);
    ui.addFlipperLog(logBuf, true);

    if (flipper.isReady()) {
        uint32_t cmdId = flipper.sendCommand(command);
        if (cmdId == 0) {
            ui.addFlipperLog("Queue full!", false);
        }
    } else {
        ui.addFlipperLog("Flipper not connected", false);
    }
}

/* ══════════════════════════════════════════════════════════
 *  FLIPPER BLE CALLBACKS
 * ══════════════════════════════════════════════════════════ */

/**
 * Called when a Flipper command completes (or times out)
 */
static void onFlipperResponse(uint32_t cmdId, const char* response, bool complete) {
    Serial.printf("[FLIP-CB] Cmd #%u %s: %s\n", cmdId,
                  complete ? "complete" : "partial", response);

    if (complete) {
        /* Show result in Flipper log */
        ui.addFlipperLog(response, false);

        /* Forward result to Supabase bridge for the agent */
        if (bridgeStarted && bridge.isJoined()) {
            bridge.sendFlipperResult(cmdId, response, true);
        }
    }
}

/**
 * Called when Flipper connection state changes
 */
static void onFlipperStateChange(FlipperState newState) {
    ui.updateFlipperStatus(newState);

    const char* stateStr = "idle";
    switch (newState) {
        case FLIP_SCANNING:   stateStr = "scanning"; break;
        case FLIP_CONNECTING: stateStr = "connecting"; break;
        case FLIP_CONNECTED:  stateStr = "connected"; break;
        case FLIP_READY:      stateStr = "ready"; break;
        case FLIP_BUSY:       stateStr = "busy"; break;
        case FLIP_ERROR:      stateStr = "error"; break;
        default: break;
    }

    Serial.printf("[MAIN] Flipper state: %s\n", stateStr);

    /* Notify bridge of Flipper state changes */
    if (bridgeStarted && bridge.isJoined()) {
        bridge.sendFlipperStatus(stateStr, flipper.getDeviceName(), flipper.getRssi());
    }

    /* Update Flipper info on UI */
    if (newState >= FLIP_CONNECTED) {
        ui.updateFlipperInfo(flipper.getDeviceName(), flipper.getRssi());
    } else {
        ui.updateFlipperInfo("---", 0);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SUPABASE BRIDGE CALLBACKS
 * ══════════════════════════════════════════════════════════ */

/**
 * Called when a Flipper command arrives from the remote agent
 * via the Supabase Realtime broadcast channel.
 */
static void onBridgeCommand(const char* command, const char* source) {
    Serial.printf("[BRIDGE-CB] Command from %s: %s\n", source, command);

    /* Show in Flipper log */
    char logBuf[160];
    snprintf(logBuf, sizeof(logBuf), "[%s] %s", source, command);
    ui.addFlipperLog(logBuf, true);

    /* Forward to Flipper if connected */
    if (flipper.isReady()) {
        uint32_t cmdId = flipper.sendCommand(command);
        if (cmdId == 0) {
            /* Queue full */
            ui.addFlipperLog("Queue full!", false);
            if (bridgeStarted && bridge.isJoined()) {
                bridge.sendFlipperResult(0, "Command queue full", false);
            }
        }
    } else {
        /* Flipper not connected */
        const char* errMsg = "Flipper not connected";
        ui.addFlipperLog(errMsg, false);
        if (bridgeStarted && bridge.isJoined()) {
            bridge.sendFlipperResult(0, errMsg, false);
        }
    }
}

/**
 * Called when the bridge connection state changes
 */
static void onBridgeStateChange(BridgeState newState) {
    ui.updateBridgeStatus(newState);
    Serial.printf("[MAIN] Bridge state: %d\n", newState);
}

/**
 * Called when the agent sends a text message via the bridge
 * (separate from Flipper commands - e.g., status updates, notifications)
 */
static void onBridgeAgentMessage(const char* message) {
    Serial.printf("[BRIDGE-CB] Agent message: %s\n", message);
    /* Show in chat as an agent message */
    ui.addChatMessage(message, false);
}

/* ══════════════════════════════════════════════════════════
 *  WIFI MANAGEMENT
 * ══════════════════════════════════════════════════════════ */

static void connectWiFi() {
    if (!configMgr.hasWiFiConfig()) {
        Serial.println("[WIFI] No WiFi config, starting AP mode");
        webPortal.startAP();
        return;
    }

    Serial.printf("[WIFI] Connecting to: %s\n", configMgr.config().wifiSSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(configMgr.config().wifiSSID, configMgr.config().wifiPass);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
        delay(100);
        lv_timer_handler();
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.printf("[WIFI] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

        /* Start web portal in STA mode */
        webPortal.startSTA();

        /* Configure NTP */
        int8_t tz = configMgr.config().timezone;
        configTime(tz * 3600, 0, "pool.ntp.org", "time.nist.gov");
        Serial.printf("[NTP] Configured with UTC%+d\n", tz);
    } else {
        Serial.println("[WIFI] Connection failed, starting AP mode");
        WiFi.disconnect();
        webPortal.startAP();
    }
}

/* ══════════════════════════════════════════════════════════
 *  SERVICE STARTUP
 * ══════════════════════════════════════════════════════════ */

static void startGateway() {
    if (!configMgr.hasGatewayConfig() || !wifiConnected) return;

    gateway.onChatDelta(onChatDelta);
    gateway.onStateChange(onGatewayStateChange);
    gateway.onFlipperCommand(onGatewayFlipperCommand);

    gateway.begin(
        configMgr.config().gwHost,
        configMgr.config().gwPort,
        configMgr.config().gwToken,
        configMgr.config().gwUseSSL
    );

    gatewayStarted = true;
    Serial.printf("[GW] Starting connection to %s:%d\n",
                  configMgr.config().gwHost, configMgr.config().gwPort);
}

static void startFlipper() {
    flipper.onResponse(onFlipperResponse);
    flipper.onStateChange(onFlipperStateChange);
    flipper.begin();
    flipperStarted = true;

    /* Auto-connect if configured */
    if (configMgr.config().flipperAuto) {
        const char* name = configMgr.config().flipperName;
        flipper.startScan(name);
        Serial.printf("[FLIP] Auto-scanning for: %s\n",
                      name[0] ? name : "any Flipper");
    }
}

static void startBridge() {
    if (!configMgr.hasSupabaseConfig() || !wifiConnected) return;

    bridge.onCommand(onBridgeCommand);
    bridge.onStateChange(onBridgeStateChange);
    bridge.onAgentMessage(onBridgeAgentMessage);

    bridge.begin(
        configMgr.config().sbUrl,
        configMgr.config().sbKey
    );

    bridgeStarted = true;
    Serial.printf("[BRIDGE] Starting Supabase bridge to: %s\n",
                  configMgr.config().sbUrl);
}

/* ══════════════════════════════════════════════════════════
 *  BATTERY READING (IP5306)
 * ══════════════════════════════════════════════════════════ */

static int readBatteryLevel() {
    Wire.beginTransmission(IP5306_I2C_ADDR);
    Wire.write(0x78);
    if (Wire.endTransmission() != 0) {
        return -1;
    }

    Wire.requestFrom((uint8_t)IP5306_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return -1;

    uint8_t raw = Wire.read();

    if (raw & 0x08) return 100;
    if (raw & 0x04) return 75;
    if (raw & 0x02) return 50;
    if (raw & 0x01) return 25;
    return 5;
}

static bool isBatteryCharging() {
    Wire.beginTransmission(IP5306_I2C_ADDR);
    Wire.write(0x70);
    if (Wire.endTransmission() != 0) return false;

    Wire.requestFrom((uint8_t)IP5306_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return false;

    uint8_t raw = Wire.read();
    return (raw & 0x08) != 0;
}

/* ══════════════════════════════════════════════════════════
 *  TIME UPDATE
 * ══════════════════════════════════════════════════════════ */

static void updateTimeDisplay() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 100)) {
        if (!ntpSynced) {
            unsigned long sec = millis() / 1000;
            int h = (sec / 3600) % 24;
            int m = (sec / 60) % 60;
            ui.updateTime(h, m, sec % 60);
        }
        return;
    }

    ntpSynced = true;
    ui.updateTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    char dateBuf[32];
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
             days[timeinfo.tm_wday], months[timeinfo.tm_mon], timeinfo.tm_mday);
    ui.updateDate(dateBuf);
}

/* ══════════════════════════════════════════════════════════
 *  SETUP
 * ══════════════════════════════════════════════════════════ */

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n╔══════════════════════════════════════╗");
    Serial.println("║   HUONYX AI SMARTWATCH v" FIRMWARE_VERSION "        ║");
    Serial.println("║   ESP32-2424S012 + Flipper Bridge    ║");
    Serial.println("╚══════════════════════════════════════╝\n");

    /* ── Initialize display ───────────────────────── */
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    /* Backlight PWM - ESP32 core 3.x uses ledcAttach/ledcWrite */
    ledcAttach(PIN_TFT_BL, 5000, 8);  /* pin, freq 5kHz, 8-bit resolution */
    ledcWrite(PIN_TFT_BL, 200);
    Serial.println("[INIT] Display initialized");

    /* ── Initialize touch ─────────────────────────── */
    if (touch.begin()) {
        Serial.println("[INIT] Touch initialized");
    } else {
        Serial.println("[INIT] Touch init FAILED");
    }

    /* ── Initialize LVGL 9 ────────────────────────── */
    lv_init();

    /* Set tick source for LVGL 9 */
    lv_tick_set_cb(lvglTickCb);

    /* Create display (LVGL 9 API - no more driver structs) */
    lv_display_t* disp = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_flush_cb(disp, lvglFlushCb);
    lv_display_set_buffers(disp, lvgl_buf1, NULL,
                           sizeof(lvgl_buf1),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Create input device (LVGL 9 API - no more driver structs) */
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvglTouchReadCb);

    Serial.println("[INIT] LVGL 9 initialized");

    /* ── Load configuration ───────────────────────── */
    configMgr.begin();
    Serial.printf("[INIT] Config loaded\n");
    Serial.printf("  WiFi: %s\n", configMgr.config().wifiSSID);
    Serial.printf("  Gateway: %s:%d\n", configMgr.config().gwHost, configMgr.config().gwPort);
    Serial.printf("  Supabase: %s\n", configMgr.config().sbUrl);
    Serial.printf("  Flipper: %s (auto=%d)\n", configMgr.config().flipperName,
                  configMgr.config().flipperAuto);

    /* Apply saved brightness */
    ledcWrite(PIN_TFT_BL, configMgr.config().brightness);

    /* ── Build UI ─────────────────────────────────── */
    ui.begin(&gateway, &configMgr, &flipper, &bridge);
    ui.updateSettingsValues();
    Serial.println("[INIT] UI built");

    /* ── Connect WiFi ─────────────────────────────── */
    connectWiFi();

    if (wifiConnected) {
        ui.updateWiFiStrength(WiFi.RSSI());
    } else {
        ui.updateWiFiStrength(0);
    }

    /* ── Start Gateway ────────────────────────────── */
    if (wifiConnected && configMgr.hasGatewayConfig()) {
        startGateway();
    }

    /* ── Start Flipper BLE ────────────────────────── */
    startFlipper();

    /* ── Start Supabase Bridge ────────────────────── */
    if (wifiConnected && configMgr.hasSupabaseConfig()) {
        startBridge();
    }

    /* ── Initial battery read ─────────────────────── */
    int batt = readBatteryLevel();
    if (batt >= 0) {
        ui.updateBattery(batt, isBatteryCharging());
    } else {
        ui.updateBattery(100, false);
    }

    Serial.printf("[INIT] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[INIT] Setup complete!\n");
}

/* ══════════════════════════════════════════════════════════
 *  MAIN LOOP
 * ══════════════════════════════════════════════════════════ */

void loop() {
    /* ── LVGL tick ────────────────────────────────── */
    lv_timer_handler();

    /* ── Gateway loop ─────────────────────────────── */
    if (gatewayStarted) {
        gateway.loop();
    }

    /* ── Flipper BLE loop ─────────────────────────── */
    if (flipperStarted) {
        flipper.loop();
    }

    /* ── Supabase bridge loop ─────────────────────── */
    if (bridgeStarted) {
        bridge.loop();
    }

    /* ── Web portal loop ──────────────────────────── */
    webPortal.loop();

    /* ── Time update (every second) ───────────────── */
    if (millis() - lastTimeUpdate >= 1000) {
        lastTimeUpdate = millis();
        updateTimeDisplay();
    }

    /* ── WiFi check (every 30 seconds) ────────────── */
    if (millis() - lastWiFiCheck >= WIFI_RECONNECT_INTERVAL) {
        lastWiFiCheck = millis();

        if (WiFi.status() == WL_CONNECTED) {
            if (!wifiConnected) {
                wifiConnected = true;
                Serial.println("[WIFI] Reconnected");

                /* Start services that need WiFi */
                if (!gatewayStarted && configMgr.hasGatewayConfig()) {
                    startGateway();
                }
                if (!bridgeStarted && configMgr.hasSupabaseConfig()) {
                    startBridge();
                }
            }
            ui.updateWiFiStrength(WiFi.RSSI());
        } else {
            if (wifiConnected) {
                wifiConnected = false;
                Serial.println("[WIFI] Disconnected");
                ui.updateWiFiStrength(0);
            }
            if (configMgr.hasWiFiConfig()) {
                WiFi.reconnect();
            }
        }
    }

    /* ── Battery check (every 60 seconds) ─────────── */
    if (millis() - lastBatteryCheck >= 60000) {
        lastBatteryCheck = millis();
        int batt = readBatteryLevel();
        if (batt >= 0) {
            ui.updateBattery(batt, isBatteryCharging());
        }
    }

    /* ── Periodic status broadcast (every 60 seconds) ── */
    if (bridgeStarted && bridge.isJoined() && millis() - lastStatusBcast >= 60000) {
        lastStatusBcast = millis();
        int batt = readBatteryLevel();
        bridge.sendWatchStatus(
            batt >= 0 ? batt : 100,
            isBatteryCharging(),
            wifiConnected,
            gatewayStarted && gateway.isConnected(),
            flipperStarted && flipper.isConnected()
        );
    }

    /* ── Small delay to prevent watchdog ──────────── */
    delay(5);
}
