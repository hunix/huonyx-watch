/**
 * HuonyxWatch_M5StickCPlus2.ino
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * Main sketch: initializes hardware, UI (M5.Display direct),
 * networking, and runs the main loop.
 *
 * NO LVGL DEPENDENCY — uses M5.Display (LovyanGFX) directly.
 * This eliminates the LVGL vs M5GFX header conflict entirely.
 */

/* Build configuration */
#include "build_config.h"
#include "hw_config.h"

/* Hardware & Libraries */
#include <M5Unified.h>
#include <WiFi.h>

/* Project modules */
#include "m5_driver.h"
#include "ui_manager.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "audio_streamer.h"
#include "imu_classifier.h"
#include "ir_controller.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "web_portal.h"

/* Global objects */
UIManager       ui;
ConfigManager   configMgr;
GatewayClient   gateway;
AudioStreamer   audioStreamer;
ImuClassifier   imuClassifier;
IRController    irController;
FlipperBLE      flipper;
SupabaseBridge  supabase;
WebPortal       webPortal;

/* Timers */
static uint32_t _lastInteraction = 0;
static uint32_t _lastTimeUpdate  = 0;
static uint32_t _lastBattUpdate  = 0;
static uint32_t _lastWifiCheck   = 0;
static bool     _ntpSynced       = false;

/* Buzzer helper */
static void buzzer_tone(uint16_t freq, uint16_t durationMs) {
    tone(PIN_BUZZER, freq, durationMs);
}

/* ========================================================
 *  SETUP
 * ======================================================== */
void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("========================================");
    Serial.println("[BOOT] Huonyx Watch M5StickC Plus2");
    Serial.println("[BOOT] No-LVGL build (M5.Display direct)");
    Serial.printf("[BOOT] Free heap: %d\n", ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.flush();

    /* Step 1: Hardware init via M5Unified */
    Serial.println("[BOOT] m5_driver_init...");
    Serial.flush();
    m5_driver_init();
    Serial.printf("[BOOT] m5_driver_init OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 2: ConfigManager (NVS) */
    Serial.println("[BOOT] ConfigManager...");
    Serial.flush();
    configMgr.begin();
    Serial.printf("[BOOT] ConfigManager OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 3: UIManager (M5.Display direct — no LVGL) */
    Serial.println("[BOOT] UIManager...");
    Serial.flush();
    ui.begin(&gateway, &configMgr, &flipper, &supabase);
    Serial.printf("[BOOT] UIManager OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 4: AudioStreamer (lazy init) */
    Serial.println("[BOOT] AudioStreamer...");
    Serial.flush();
    audioStreamer.begin(&gateway);
    Serial.printf("[BOOT] AudioStreamer OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 5: IMU */
    Serial.println("[BOOT] IMU...");
    Serial.flush();
    imuClassifier.begin();
    Serial.printf("[BOOT] IMU OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 6: IR */
    Serial.println("[BOOT] IR...");
    Serial.flush();
    irController.begin();
    Serial.printf("[BOOT] IR OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 7: WebPortal */
    Serial.println("[BOOT] WebPortal...");
    Serial.flush();
    webPortal.begin();
    Serial.printf("[BOOT] WebPortal OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 8: WiFi */
    Serial.println("[BOOT] WiFi...");
    Serial.flush();
    const WatchConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSSID) > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(cfg.wifiSSID, cfg.wifiPass);
        Serial.printf("[BOOT] WiFi connecting: %s\n", cfg.wifiSSID);
    } else {
        Serial.println("[BOOT] No WiFi configured - starting AP");
        webPortal.startAP();
    }
    Serial.printf("[BOOT] WiFi OK | Heap: %d\n", ESP.getFreeHeap());
    Serial.flush();

    /* Step 9: Callbacks */
    Serial.println("[BOOT] Callbacks...");
    Serial.flush();

    gateway.onStateChange([](GatewayState s) {
        ui.updateGatewayStatus(s);
        Serial.printf("[GW] State: %d\n", (int)s);
    });

    gateway.onChatDelta([](const char* rid, const char* txt, bool fin) {
        if (fin) {
            ui.addChatMessage(txt, false);
            ui.updateAgentTyping(false);
        } else {
            ui.updateAgentTyping(true);
        }
    });

    flipper.onStateChange([](FlipperState s) {
        ui.updateFlipperStatus(s);
        Serial.printf("[FLIP] State: %d\n", (int)s);
    });

    supabase.onStateChange([](BridgeState s) {
        ui.updateBridgeStatus(s);
        Serial.printf("[SB] State: %d\n", (int)s);
    });

    ui.updateBattery(m5_battery_percent(), m5_is_charging());
    _lastInteraction = millis();

    Serial.println("========================================");
    Serial.println("[BOOT] ALL STEPS COMPLETE - BOOT SUCCESS");
    Serial.printf("[BOOT] Free heap: %d\n", ESP.getFreeHeap());
    Serial.printf("[BOOT] Min free heap: %d\n", ESP.getMinFreeHeap());
    Serial.println("========================================");
    Serial.flush();

    led_blink(3, 150);
}

/* ========================================================
 *  LOOP
 * ======================================================== */
void loop() {
    uint32_t now = millis();

    /* Button polling */
    BtnEvent evt = m5_driver_tick();
    if (evt != BTN_NONE) {
        _lastInteraction = now;
        ui.handleButton(evt);
    }

    /* UI tick (handles dirty redraws and sleep timer) */
    ui.tick();

    /* Network ticks */
    gateway.loop();
    supabase.loop();
    flipper.loop();
    webPortal.loop();

    /* Audio & IMU */
    audioStreamer.loop();
    imuClassifier.loop();

    /* Time update every 500ms */
    if (now - _lastTimeUpdate > 500) {
        _lastTimeUpdate = now;
        RtcTime t;
        if (rtc_get_time(&t)) {
            ui.updateTime(t.hour, t.minute, t.second);
            static const char* dayNames[] = {
                "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
            };
            static const char* monthNames[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            char dateBuf[24];
            snprintf(dateBuf, sizeof(dateBuf), "%s, %s %d",
                     dayNames[t.weekday % 7],
                     monthNames[(t.month - 1) % 12],
                     t.day);
            ui.updateDate(dateBuf);
        }
    }

    /* Battery update every 10s */
    if (now - _lastBattUpdate > 10000) {
        _lastBattUpdate = now;
        ui.updateBattery(m5_battery_percent(), m5_is_charging());
    }

    /* WiFi RSSI update every 5s */
    if (now - _lastWifiCheck > 5000) {
        _lastWifiCheck = now;
        if (WiFi.status() == WL_CONNECTED) {
            ui.updateWiFiStrength(WiFi.RSSI());

            /* NTP sync once after WiFi connects */
            if (!_ntpSynced) {
                _ntpSynced = true;
                const WatchConfig& cfg2 = configMgr.config();
                rtc_sync_from_ntp("pool.ntp.org", cfg2.timezone * 3600L);
                Serial.println("[NTP] Synced");
            }

            /* Connect gateway if not connected */
            const WatchConfig& cfg2 = configMgr.config();
            if (gateway.state() == GW_DISCONNECTED && strlen(cfg2.gwHost) > 0) {
                gateway.connect(cfg2.gwHost, cfg2.gwPort, cfg2.gwToken, cfg2.gwUseSSL);
            }
            if (supabase.state() == BRIDGE_IDLE && strlen(cfg2.sbUrl) > 0) {
                supabase.connect(cfg2.sbUrl, cfg2.sbKey);
            }
        } else {
            ui.updateWiFiStrength(0);
        }
    }

    delay(5);
}
