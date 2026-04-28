/**
 * HuonyxWatch_M5StickCPlus2.ino
 * DIAGNOSTIC BUILD v2 - Incremental module testing
 * 
 * Change DIAG_LEVEL to test modules one at a time:
 *   2 = M5 + Display only (known working)
 *   3 = + LVGL init
 *   4 = + ConfigManager
 *   5 = + UIManager
 *   6 = + AudioStreamer
 *   7 = + IMU
 *   8 = + IR
 *   9 = + WebPortal
 *  10 = + WiFi connect
 *  11 = + Full loop (gateway, supabase, flipper)
 *
 * Currently set to 11 (all modules) with watchdog-safe loop.
 */
#define DIAG_LEVEL  11

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

/* -- Module instances -- */
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

/* -- LVGL -- */
static lv_display_t* lvDisplay = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area,
                           uint8_t* px_map) {
    m5_display_flush(area->x1, area->y1, area->x2, area->y2,
                     (const lv_color_t*)px_map);
    lv_display_flush_ready(disp);
}

static uint32_t lvgl_tick_cb() { return millis(); }

/* -- State -- */
static uint32_t _lastInteraction = 0;
static bool _wifiConnected = false;

/* ========================================================
 *  SETUP
 * ======================================================== */
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("========================================");
    Serial.printf("[DIAG] Level %d | Free heap: %d\n", DIAG_LEVEL, ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.flush();

    /* Step 1: M5 hardware */
    Serial.println("[DIAG] Step 1: m5_driver_init...");
    Serial.flush();
    m5_driver_init();
    Serial.printf("[DIAG] Step 1 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 2: Display test */
    Serial.println("[DIAG] Step 2: Display...");
    Serial.flush();
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_CYAN, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(10, 30);
    M5.Display.printf("DIAG L%d", DIAG_LEVEL);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(10, 60);
    M5.Display.printf("Heap:%d", ESP.getFreeHeap());
    Serial.printf("[DIAG] Step 2 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 3
    M5.Display.setCursor(10, 80);
    M5.Display.println("STOPPED at L2");
    Serial.println("[DIAG] Halted at level 2");
    return;
#endif

    /* Step 3: LVGL */
    Serial.println("[DIAG] Step 3: LVGL...");
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
    Serial.printf("[DIAG] Step 3 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 4
    Serial.println("[DIAG] Halted at level 3");
    return;
#endif

    /* Step 4: ConfigManager */
    Serial.println("[DIAG] Step 4: ConfigManager...");
    Serial.flush();
    configMgr.begin();
    Serial.printf("[DIAG] Step 4 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 5
    Serial.println("[DIAG] Halted at level 4");
    return;
#endif

    /* Step 5: UIManager */
    Serial.println("[DIAG] Step 5: UIManager...");
    Serial.flush();
    ui.begin(&gateway, &configMgr, &flipper, &supabase);
    Serial.printf("[DIAG] Step 5 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 6
    Serial.println("[DIAG] Halted at level 5");
    return;
#endif

    /* Step 6: AudioStreamer */
    Serial.println("[DIAG] Step 6: AudioStreamer...");
    Serial.flush();
    audioStreamer.begin(&gateway);
    Serial.printf("[DIAG] Step 6 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 7
    Serial.println("[DIAG] Halted at level 6");
    return;
#endif

    /* Step 7: IMU */
    Serial.println("[DIAG] Step 7: IMU...");
    Serial.flush();
    imuClassifier.begin();
    Serial.printf("[DIAG] Step 7 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 8
    Serial.println("[DIAG] Halted at level 7");
    return;
#endif

    /* Step 8: IR */
    Serial.println("[DIAG] Step 8: IR...");
    Serial.flush();
    irController.begin();
    Serial.printf("[DIAG] Step 8 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 9
    Serial.println("[DIAG] Halted at level 8");
    return;
#endif

    /* Step 9: WebPortal */
    Serial.println("[DIAG] Step 9: WebPortal...");
    Serial.flush();
    webPortal.begin();
    Serial.printf("[DIAG] Step 9 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 10
    Serial.println("[DIAG] Halted at level 9");
    return;
#endif

    /* Step 10: WiFi */
    Serial.println("[DIAG] Step 10: WiFi...");
    Serial.flush();
    const WatchConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSSID) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifiSSID, cfg.wifiPass);
        Serial.printf("[DIAG] WiFi: %s\n", cfg.wifiSSID);
    } else {
        Serial.println("[DIAG] No WiFi - starting AP");
        webPortal.startAP();
    }
    Serial.printf("[DIAG] Step 10 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

#if DIAG_LEVEL < 11
    Serial.println("[DIAG] Halted at level 10");
    return;
#endif

    /* Step 11: Callbacks + battery */
    Serial.println("[DIAG] Step 11: Callbacks...");
    Serial.flush();
    gateway.onStateChange([](GatewayState s) {
        Serial.printf("[GW] State: %d\n", s);
    });
    gateway.onChatDelta([](const char* rid, const char* txt, bool fin) {
        Serial.printf("[GW] Chat: %s\n", txt);
    });
    flipper.onStateChange([](FlipperState s) {
        Serial.printf("[FLIP] State: %d\n", s);
    });
    supabase.onStateChange([](BridgeState s) {
        Serial.printf("[SB] State: %d\n", s);
    });
    ui.updateBattery(m5_battery_percent(), m5_is_charging());
    _lastInteraction = millis();
    Serial.printf("[DIAG] Step 11 OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    Serial.println("========================================");
    Serial.println("[DIAG] ALL STEPS COMPLETE - BOOT SUCCESS");
    Serial.printf("[DIAG] Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("[DIAG] Min free heap: %d\n", ESP.getMinFreeHeap());
    Serial.println("========================================");
    Serial.flush();
    led_blink(3, 150);
}

/* ========================================================
 *  LOOP
 * ======================================================== */
void loop() {
#if DIAG_LEVEL < 3
    /* No LVGL - just blink to show we're alive */
    static uint32_t lastBlink = 0;
    if (millis() - lastBlink > 2000) {
        lastBlink = millis();
        Serial.printf("[DIAG] Alive | Heap: %d\n", ESP.getFreeHeap());
    }
    delay(100);
    return;
#endif

    /* Button polling */
    BtnEvent evt = m5_driver_tick();
    if (evt != BTN_NONE) {
#if DIAG_LEVEL >= 5
        ui.handleButton(evt);
#endif
    }

    /* LVGL tick */
    lv_timer_handler();

#if DIAG_LEVEL >= 11
    /* Network ticks */
    gateway.loop();
    supabase.loop();
    flipper.loop();
#endif

#if DIAG_LEVEL >= 9
    webPortal.loop();
#endif

#if DIAG_LEVEL >= 6
    audioStreamer.loop();
#endif

#if DIAG_LEVEL >= 7
    imuClassifier.loop();
#endif

    delay(5);
}
