/**
 * ╔══════════════════════════════════════════════════════════╗
 * ║           HUONYX AI SMARTWATCH FIRMWARE                  ║
 * ║     ESP32-2424S012 (ESP32-C3 + GC9A01 + CST816D)       ║
 * ║                                                          ║
 * ║  A beautiful AI-era smartwatch interface for the         ║
 * ║  Huonyx (House of Claws) autonomous AI platform.        ║
 * ║                                                          ║
 * ║  Features:                                               ║
 * ║  - Digital watch face with battery & status indicators   ║
 * ║  - AI chat interface with quick replies                  ║
 * ║  - WebSocket gateway client for HoC protocol             ║
 * ║  - Web-based configuration portal                        ║
 * ║  - Gesture navigation (swipe between screens)            ║
 * ╚══════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#include "hw_config.h"
#include "touch_driver.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "ui_manager.h"
#include "web_portal.h"

/* ── Global Objects ───────────────────────────────────── */
static TFT_eSPI       tft = TFT_eSPI();
static CST816D_Driver  touch(PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST, TOUCH_I2C_ADDR);
static ConfigManager   configMgr;
static GatewayClient   gateway;
static UIManager       ui;
static WebPortal       webPortal(&configMgr);

/* ── LVGL Display Buffer ──────────────────────────────── */
static lv_disp_draw_buf_t drawBuf;
static lv_color_t         buf1[LVGL_BUF_SIZE];

/* ── LVGL Display Driver ──────────────────────────────── */
static lv_disp_drv_t  dispDrv;
static lv_indev_drv_t indevDrv;

/* ── Timing ───────────────────────────────────────────── */
static unsigned long lastTimeUpdate   = 0;
static unsigned long lastWiFiCheck    = 0;
static unsigned long lastBatteryCheck = 0;
static unsigned long lastGwCheck      = 0;
static bool          wifiConnected    = false;
static bool          gatewayStarted   = false;
static bool          ntpSynced        = false;

/* ── Accumulator for streaming chat deltas ────────────── */
static String currentRunId = "";
static String accumulatedText = "";

/* ══════════════════════════════════════════════════════════
 *  LVGL CALLBACKS
 * ══════════════════════════════════════════════════════════ */

/**
 * Display flush callback - sends pixel data to GC9A01 via TFT_eSPI
 */
static void lvglFlushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t*)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(drv);
}

/**
 * Touch input read callback - reads CST816D touch data
 */
static void lvglTouchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    TouchPoint tp;
    if (touch.read(tp) && tp.pressed) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = tp.x;
        data->point.y = tp.y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ══════════════════════════════════════════════════════════
 *  GATEWAY CALLBACKS
 * ══════════════════════════════════════════════════════════ */

/**
 * Called when chat delta/final events arrive from the gateway
 */
static void onChatDelta(const char* runId, const char* text, bool isFinal) {
    if (currentRunId != runId) {
        /* New run - reset accumulator */
        currentRunId = runId;
        accumulatedText = "";
    }

    accumulatedText += text;

    if (isFinal) {
        /* Final message - display the complete response */
        ui.updateAgentTyping(false);

        /* Truncate if too long for display */
        String displayText = accumulatedText;
        if (displayText.length() > CHAT_MAX_MSG_LEN - 1) {
            displayText = displayText.substring(0, CHAT_MAX_MSG_LEN - 4) + "...";
        }

        ui.addChatMessage(displayText.c_str(), false);
        currentRunId = "";
        accumulatedText = "";
    }
}

/**
 * Called when gateway connection state changes
 */
static void onGatewayStateChange(GatewayState newState) {
    ui.updateGatewayStatus(newState);
    Serial.printf("[MAIN] Gateway state: %d\n", newState);
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

        /* Start web portal in STA mode for configuration */
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

static void startGateway() {
    if (!configMgr.hasGatewayConfig() || !wifiConnected) return;

    gateway.onChatDelta(onChatDelta);
    gateway.onStateChange(onGatewayStateChange);

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

/* ══════════════════════════════════════════════════════════
 *  BATTERY READING (IP5306)
 * ══════════════════════════════════════════════════════════ */

static int readBatteryLevel() {
    /* IP5306 battery level register */
    Wire.beginTransmission(IP5306_I2C_ADDR);
    Wire.write(0x78);
    if (Wire.endTransmission() != 0) {
        return -1;  /* IC not responding */
    }

    Wire.requestFrom((uint8_t)IP5306_I2C_ADDR, (uint8_t)1);
    if (!Wire.available()) return -1;

    uint8_t raw = Wire.read();

    /* IP5306 reports battery in 4 levels via bits */
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
        /* NTP not synced yet - show placeholder */
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

    /* Format date string */
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
    Serial.println("\n╔══════════════════════════════════╗");
    Serial.println("║   HUONYX AI SMARTWATCH v" FIRMWARE_VERSION "    ║");
    Serial.println("║   ESP32-2424S012                 ║");
    Serial.println("╚══════════════════════════════════╝\n");

    /* ── Initialize display ───────────────────────── */
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    analogWrite(PIN_TFT_BL, 200);
    Serial.println("[INIT] Display initialized");

    /* ── Initialize touch ─────────────────────────── */
    if (touch.begin()) {
        Serial.println("[INIT] Touch initialized");
    } else {
        Serial.println("[INIT] Touch init FAILED");
    }

    /* ── Initialize LVGL ──────────────────────────── */
    lv_init();

    /* Display buffer */
    lv_disp_draw_buf_init(&drawBuf, buf1, NULL, LVGL_BUF_SIZE);

    /* Display driver */
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res  = TFT_WIDTH;
    dispDrv.ver_res  = TFT_HEIGHT;
    dispDrv.flush_cb = lvglFlushCb;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    /* Input driver (touch) */
    lv_indev_drv_init(&indevDrv);
    indevDrv.type    = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = lvglTouchReadCb;
    lv_indev_drv_register(&indevDrv);

    Serial.println("[INIT] LVGL initialized");

    /* ── Load configuration ───────────────────────── */
    configMgr.begin();
    Serial.printf("[INIT] Config loaded - WiFi: %s, Gateway: %s:%d\n",
                  configMgr.config().wifiSSID,
                  configMgr.config().gwHost,
                  configMgr.config().gwPort);

    /* Apply saved brightness */
    analogWrite(PIN_TFT_BL, configMgr.config().brightness);

    /* ── Build UI ─────────────────────────────────── */
    ui.begin(&gateway, &configMgr);
    ui.updateSettingsValues();
    Serial.println("[INIT] UI built");

    /* ── Connect WiFi ─────────────────────────────── */
    connectWiFi();

    /* Update WiFi status on UI */
    if (wifiConnected) {
        ui.updateWiFiStrength(WiFi.RSSI());
    } else {
        ui.updateWiFiStrength(0);
    }

    /* ── Start Gateway ────────────────────────────── */
    if (wifiConnected && configMgr.hasGatewayConfig()) {
        startGateway();
    }

    /* ── Initial battery read ─────────────────────── */
    int batt = readBatteryLevel();
    if (batt >= 0) {
        ui.updateBattery(batt, isBatteryCharging());
    } else {
        ui.updateBattery(100, false);  /* Assume USB powered */
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
                if (!gatewayStarted && configMgr.hasGatewayConfig()) {
                    startGateway();
                }
            }
            ui.updateWiFiStrength(WiFi.RSSI());
        } else {
            if (wifiConnected) {
                wifiConnected = false;
                Serial.println("[WIFI] Disconnected");
                ui.updateWiFiStrength(0);
            }
            /* Attempt reconnect */
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

    /* ── Small delay to prevent watchdog ──────────── */
    delay(5);
}
