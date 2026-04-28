/**
 * HuonyxWatch_M5StickCPlus2.ino
 * DIAGNOSTIC BUILD - Minimal boot to identify crash point
 * 
 * This version initializes modules ONE AT A TIME with Serial markers.
 * If the device still crashes, the problem is in the libraries or
 * hardware configuration, not our application code.
 */
#include "build_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <M5Unified.h>
#include <ArduinoJson.h>

#include "hw_config.h"
#include "m5_driver.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "web_portal.h"
#include "ui_manager.h"
#include "audio_streamer.h"
#include "imu_classifier.h"
#include "ir_controller.h"
#include "vision_trigger.h"

/* ── Module instances ─────────────────────────────────────── */
static ConfigManager  configMgr;
static GatewayClient  gateway;
static FlipperBLE     flipper;
static SupabaseBridge supabase;
static WebPortal      webPortal(&configMgr);
static UIManager      ui;
static AudioStreamer  audioStreamer;
static ImuClassifier imuClassifier;
static IRController  irController;
static VisionTrigger visionTrigger;

/* ── LVGL ─────────────────────────────────────────────────── */
static lv_display_t* lvDisplay = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area,
                           uint8_t* px_map) {
    m5_display_flush(area->x1, area->y1, area->x2, area->y2,
                     (const lv_color_t*)px_map);
    lv_display_flush_ready(disp);
}

static uint32_t lvgl_tick_cb() { return millis(); }

/* ══════════════════════════════════════════════════════════
 *  SETUP - DIAGNOSTIC VERSION
 *  Each step is wrapped in Serial markers.
 *  If the device crashes, the LAST printed marker tells us where.
 * ══════════════════════════════════════════════════════════ */
void setup() {
    /* Step 0: Serial - this MUST print or the problem is hardware/bootloader */
    Serial.begin(115200);
    delay(500);  /* Give Serial time to stabilize */
    Serial.println();
    Serial.println("========================================");
    Serial.println("[DIAG] Step 0: Serial OK");
    Serial.println("========================================");
    Serial.flush();

    /* Step 1: M5Unified init */
    Serial.println("[DIAG] Step 1: Starting m5_driver_init...");
    Serial.flush();
    m5_driver_init();
    Serial.println("[DIAG] Step 1: m5_driver_init OK");
    Serial.flush();

    /* Step 2: Show something on display to prove it works */
    Serial.println("[DIAG] Step 2: Display test...");
    Serial.flush();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 50);
    M5.Display.println("HUONYX");
    M5.Display.setCursor(10, 80);
    M5.Display.println("DIAG v1");
    Serial.println("[DIAG] Step 2: Display OK");
    Serial.flush();

    /* Step 3: LVGL init */
    Serial.println("[DIAG] Step 3: LVGL init...");
    Serial.flush();
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);
    lvDisplay = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
    static lv_color_t buf1[LVGL_BUF_SIZE];
    static lv_color_t buf2[LVGL_BUF_SIZE];
    lv_display_set_buffers(lvDisplay, buf1, buf2,
                           sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay, lvgl_flush_cb);
    Serial.println("[DIAG] Step 3: LVGL OK");
    Serial.flush();

    /* Step 4: ConfigManager */
    Serial.println("[DIAG] Step 4: ConfigManager...");
    Serial.flush();
    configMgr.begin();
    Serial.println("[DIAG] Step 4: ConfigManager OK");
    Serial.flush();

    /* Step 5: UI Manager */
    Serial.println("[DIAG] Step 5: UI Manager...");
    Serial.flush();
    ui.begin(&gateway, &configMgr, &flipper, &supabase);
    Serial.println("[DIAG] Step 5: UI Manager OK");
    Serial.flush();

    /* Step 6: Audio Streamer (lazy init - should be instant) */
    Serial.println("[DIAG] Step 6: AudioStreamer...");
    Serial.flush();
    audioStreamer.begin(&gateway);
    Serial.println("[DIAG] Step 6: AudioStreamer OK");
    Serial.flush();

    /* Step 7: IMU Classifier */
    Serial.println("[DIAG] Step 7: IMU Classifier...");
    Serial.flush();
    imuClassifier.begin();
    Serial.println("[DIAG] Step 7: IMU OK");
    Serial.flush();

    /* Step 8: IR Controller */
    Serial.println("[DIAG] Step 8: IR Controller...");
    Serial.flush();
    irController.begin();
    Serial.println("[DIAG] Step 8: IR OK");
    Serial.flush();

    /* Step 9: Web Portal */
    Serial.println("[DIAG] Step 9: Web Portal...");
    Serial.flush();
    webPortal.begin();
    Serial.println("[DIAG] Step 9: Web Portal OK");
    Serial.flush();

    /* Step 10: WiFi */
    Serial.println("[DIAG] Step 10: WiFi...");
    Serial.flush();
    const WatchConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSSID) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifiSSID, cfg.wifiPass);
        Serial.printf("[DIAG] WiFi connecting to: %s\n", cfg.wifiSSID);
    } else {
        Serial.println("[DIAG] No WiFi configured - starting AP");
        webPortal.startAP();
        ui.showScreen(SCREEN_WIFI_SETUP);
    }
    Serial.println("[DIAG] Step 10: WiFi OK");
    Serial.flush();

    /* Step 11: Battery */
    Serial.println("[DIAG] Step 11: Battery...");
    Serial.flush();
    ui.updateBattery(m5_battery_percent(), m5_is_charging());
    Serial.println("[DIAG] Step 11: Battery OK");
    Serial.flush();

    Serial.println("========================================");
    Serial.println("[DIAG] ALL STEPS COMPLETE - BOOT SUCCESS");
    Serial.printf("[DIAG] Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("[DIAG] Min free heap: %d bytes\n", ESP.getMinFreeHeap());
    Serial.printf("[DIAG] Free PSRAM: %d bytes\n", ESP.getFreePsram());
    Serial.println("========================================");
    Serial.flush();

    led_blink(3, 150);
}

/* ══════════════════════════════════════════════════════════
 *  LOOP - DIAGNOSTIC VERSION
 * ══════════════════════════════════════════════════════════ */
void loop() {
    /* Button polling */
    BtnEvent evt = m5_driver_tick();
    if (evt != BTN_NONE) {
        ui.handleButton(evt);
    }

    /* LVGL tick */
    lv_timer_handler();

    /* Network ticks */
    gateway.loop();
    supabase.loop();
    flipper.loop();
    webPortal.loop();

    /* Sensor ticks */
    audioStreamer.loop();
    imuClassifier.loop();

    delay(5);
}
