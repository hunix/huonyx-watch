/**
 * HuonyxWatch_M5StickCPlus2.ino
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * A fully-featured port of the Huonyx Watch firmware to the
 * M5StickC Plus2 (ESP32-PICO-V3-02, ST7789V2 135x240 display).
 *
 * Features:
 *   - Huonyx Gateway WebSocket client (AI chat)
 *   - Supabase Realtime bridge (Flipper Zero control)
 *   - Flipper Zero BLE client
 *   - LVGL 9 UI adapted for 135x240 portrait display
 *   - Button-driven navigation (3 physical buttons)
 *   - Hardware RTC (BM8563) with NTP sync
 *   - IMU wake-on-wrist-raise (MPU6886)
 *   - Buzzer notifications
 *   - Web portal for configuration
 *   - NVS persistent configuration
 *
 * Navigation:
 *   Button A short  → Next screen / Back
 *   Button A long   → Settings
 *   Button B short  → Scroll / Cycle
 *   Button B long   → Select / Quick Reply / Scan
 *
 * Hardware: M5StickC Plus2 (SKU: K016-P2)
 * Framework: Arduino ESP32 3.x
 * Display: ST7789V2 via M5GFX
 * UI: LVGL 9.x
 */

#include "build_config.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <lvgl.h>
#include <M5GFX.h>

#include "hw_config.h"
#include "m5_driver.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "web_portal.h"
#include "ui_manager.h"

/* ── Module instances ─────────────────────────────────── */
static ConfigManager  configMgr;
static GatewayClient  gateway;
static FlipperBLE     flipper;
static SupabaseBridge supabase;
static WebPortal      webPortal;
static UIManager      ui;

/* ── LVGL display flush callback ──────────────────────── */
static lv_display_t* lvDisplay = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area,
                           uint8_t* px_map) {
    m5_display_flush(area->x1, area->y1, area->x2, area->y2,
                     (const lv_color_t*)px_map);
    lv_display_flush_ready(disp);
}

/* ── LVGL tick callback ───────────────────────────────── */
static uint32_t lvgl_tick_cb() {
    return millis();
}

/* ── WiFi management ──────────────────────────────────── */
static bool     _wifiConnected  = false;
static uint32_t _wifiRetryMs    = 0;
static bool     _ntpSynced      = false;

static void wifi_connect() {
    const AppConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSsid) == 0) return;

    Serial.printf("[WiFi] Connecting to %s...\n", cfg.wifiSsid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(cfg.wifiSsid, cfg.wifiPassword);
}

static void wifi_tick() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!_wifiConnected) {
            _wifiConnected = true;
            Serial.printf("[WiFi] Connected, IP: %s\n",
                          WiFi.localIP().toString().c_str());
            ui.updateWiFiStrength(WiFi.RSSI());

            /* Sync RTC from NTP on first connection */
            if (!_ntpSynced) {
                const AppConfig& cfg = configMgr.config();
                if (rtc_sync_from_ntp("pool.ntp.org", cfg.gmtOffset)) {
                    _ntpSynced = true;
                    Serial.println("[RTC] NTP sync successful");
                }
            }

            /* Connect gateway */
            const AppConfig& cfg = configMgr.config();
            if (strlen(cfg.gatewayHost) > 0) {
                gateway.begin(cfg.gatewayHost, cfg.gatewayPort,
                              cfg.gatewayToken, cfg.gatewayUseSsl);
            }

            /* Connect Supabase bridge */
            if (strlen(cfg.supabaseUrl) > 0) {
                supabase.begin(cfg.supabaseUrl, cfg.supabaseKey);
            }
        } else {
            /* Periodic RSSI update */
            static uint32_t rssiTimer = 0;
            if (millis() - rssiTimer > 10000) {
                rssiTimer = millis();
                ui.updateWiFiStrength(WiFi.RSSI());
            }
        }
    } else {
        if (_wifiConnected) {
            _wifiConnected = false;
            Serial.println("[WiFi] Disconnected");
            ui.updateWiFiStrength(0);
        }
        /* Retry every 30 seconds */
        if (millis() - _wifiRetryMs > WIFI_RECONNECT_INTERVAL) {
            _wifiRetryMs = millis();
            wifi_connect();
        }
    }
}

/* ── RTC / Clock tick ─────────────────────────────────── */
static void clock_tick() {
    static uint32_t clockTimer = 0;
    if (millis() - clockTimer < 1000) return;
    clockTimer = millis();

    RtcTime t;
    if (rtc_get_time(&t)) {
        ui.updateTime(t.hour, t.minute, t.second);

        /* Update date every minute */
        static uint8_t lastMin = 255;
        if (t.minute != lastMin) {
            lastMin = t.minute;
            static const char* DAYS[]   = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
            static const char* MONTHS[] = {"Jan","Feb","Mar","Apr","May","Jun",
                                           "Jul","Aug","Sep","Oct","Nov","Dec"};
            char dateBuf[20];
            snprintf(dateBuf, sizeof(dateBuf), "%s, %d %s %d",
                     DAYS[t.weekday % 7], t.day,
                     MONTHS[(t.month - 1) % 12], t.year);
            ui.updateDate(dateBuf);
        }
    }
}

/* ── Battery tick ─────────────────────────────────────── */
static void battery_tick() {
    static uint32_t batTimer = 0;
    if (millis() - batTimer < 30000) return;  /* Every 30s */
    batTimer = millis();

    /* M5StickC Plus2 has no PMIC — estimate from ADC */
    /* Battery voltage on ADC pin (approximate) */
    uint32_t raw = analogRead(38);  /* Battery ADC */
    float voltage = raw * 3.3f / 4095.0f * 2.0f;  /* Voltage divider */
    int percent = constrain((int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f), 0, 100);
    bool charging = (voltage > 4.15f);
    ui.updateBattery(percent, charging);
}

/* ── IMU wake detection ───────────────────────────────── */
static void imu_tick() {
    static uint32_t imuTimer = 0;
    if (millis() - imuTimer < 500) return;  /* Check every 500ms */
    imuTimer = millis();

    if (ui.isSleeping() && imu_detect_wrist_raise()) {
        ui.wakeDisplay();
    }
}

/* ── Sleep tick ───────────────────────────────────────── */
static void sleep_tick() {
    extern uint32_t _lastInteraction;
    /* Handled inside UIManager — just check from main loop */
}

/* ── Gateway callbacks ────────────────────────────────── */
static void onGatewayStateChange(GatewayState state) {
    ui.updateGatewayStatus(state);
    if (state == GW_AUTHENTICATED) {
        Serial.println("[GW] Authenticated");
        ui.showNotification("Huonyx", "Gateway connected");
    }
}

static void onChatDelta(const char* runId, const char* text, bool isFinal) {
    if (isFinal && strlen(text) > 0) {
        ui.addChatMessage(text, false);
        ui.updateAgentTyping(false);
    } else if (!isFinal) {
        ui.updateAgentTyping(true);
    }
}

/* ── Flipper callbacks ────────────────────────────────── */
static void onFlipperStateChange(FlipperState state) {
    ui.updateFlipperStatus(state);
    if (state == FLIPPER_CONNECTED) {
        ui.showNotification("Flipper", "Connected");
        ui.addFlipperLog("Flipper Zero connected", false);
    }
}

static void onFlipperData(const char* data) {
    ui.addFlipperLog(data, false);
    supabase.sendFlipperResult(0, data, true);
}

/* ── Supabase callbacks ───────────────────────────────── */
static void onBridgeStateChange(BridgeState state) {
    ui.updateBridgeStatus(state);
}

static void onBridgeCommand(const char* command, const char* source) {
    ui.addFlipperLog(command, true);
    if (flipper.isConnected()) {
        flipper.sendCommand(command);
    }
}

/* ══════════════════════════════════════════════════════════
 *  SETUP
 * ══════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    Serial.println("\n[Huonyx] M5StickC Plus2 starting...");

    /* ── Hardware init ────────────────────────────────── */
    m5_driver_init();

    /* ── LVGL init ────────────────────────────────────── */
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    /* Create display */
    lvDisplay = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);

    /* Allocate draw buffers (double-buffered) */
    static lv_color_t buf1[LVGL_BUF_SIZE];
    static lv_color_t buf2[LVGL_BUF_SIZE];
    lv_display_set_buffers(lvDisplay, buf1, buf2,
                           sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay, lvgl_flush_cb);

    /* ── Config ───────────────────────────────────────── */
    configMgr.begin();

    /* ── UI ───────────────────────────────────────────── */
    ui.begin(&gateway, &configMgr, &flipper, &supabase);

    /* ── Gateway callbacks ────────────────────────────── */
    gateway.onStateChange(onGatewayStateChange);
    gateway.onChatDelta(onChatDelta);

    /* ── Flipper callbacks ────────────────────────────── */
    flipper.onStateChange(onFlipperStateChange);
    flipper.onData(onFlipperData);

    /* ── Supabase callbacks ───────────────────────────── */
    supabase.onStateChange(onBridgeStateChange);
    supabase.onCommand(onBridgeCommand);

    /* ── Web portal ───────────────────────────────────── */
    webPortal.begin(&configMgr);

    /* ── WiFi ─────────────────────────────────────────── */
    const AppConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSsid) > 0) {
        wifi_connect();
    } else {
        /* No WiFi configured — start AP mode for setup */
        webPortal.startAP();
        ui.showScreen(SCREEN_WIFI_SETUP);
    }

    /* ── Auto-connect Flipper ─────────────────────────── */
    if (cfg.flipperAutoConnect && strlen(cfg.flipperDeviceName) > 0) {
        flipper.begin(cfg.flipperDeviceName);
    }

    /* Initial battery read */
    ui.updateBattery(100, false);

    Serial.println("[Huonyx] Setup complete");
    led_blink(2, 200);
}

/* ══════════════════════════════════════════════════════════
 *  LOOP
 * ══════════════════════════════════════════════════════════ */
void loop() {
    /* ── Button polling ───────────────────────────────── */
    BtnEvent evt = m5_driver_tick();
    if (evt != BTN_NONE) {
        ui.handleButton(evt);
    }

    /* ── LVGL tick ────────────────────────────────────── */
    lv_timer_handler();

    /* ── Network ticks ────────────────────────────────── */
    wifi_tick();
    gateway.loop();
    supabase.loop();
    flipper.loop();
    webPortal.loop();

    /* ── Sensor ticks ─────────────────────────────────── */
    clock_tick();
    battery_tick();
    imu_tick();

    /* ── Sleep management ─────────────────────────────── */
    static uint32_t _lastInteraction = millis();
    if (!ui.isSleeping() &&
        (millis() - _lastInteraction > SLEEP_TIMEOUT_MS)) {
        ui.sleepDisplay();
    }

    /* Small yield to prevent watchdog */
    delay(5);
}
