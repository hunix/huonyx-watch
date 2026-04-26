/**
 * HuonyxWatch_M5StickCPlus2.ino
 * Huonyx AI Smartwatch — M5StickC Plus2 Port (v2.0 — Sensor Edition)
 *
 * A fully-featured port of the Huonyx Watch firmware to the
 * M5StickC Plus2 (ESP32-PICO-V3-02, ST7789V2 135x240 display).
 *
 * ── Core Features ──────────────────────────────────────────
 *   - Huonyx Gateway WebSocket client (AI chat + agent control)
 *   - Supabase Realtime bridge (Flipper Zero control)
 *   - Flipper Zero BLE client
 *   - LVGL 9 UI adapted for 135x240 portrait display
 *   - Button-driven navigation (3 physical buttons)
 *   - Hardware RTC (BM8563) with NTP sync
 *   - Web portal for configuration
 *   - NVS persistent configuration
 *
 * ── v2.0 Sensor Additions ──────────────────────────────────
 *   - VOICE-TO-HUONYX: Real-time I2S mic streaming to Whisper
 *     (dual-core FreeRTOS: Core 0 = sampling, Core 1 = WiFi)
 *   - VISUAL INTELLIGENCE: Trigger camera capture + vision LLM
 *     analysis from the wrist (camera connects via WiFi/Grove)
 *   - IMU CONTEXT CLASSIFIER: Activity detection (still/walking/
 *     lying/driving) injected as metadata into every agent message
 *   - GESTURE CONTROL: Shake=stop task, tilt=scroll, wrist-raise=wake
 *   - AGENTIC IR REMOTE: Agent sends IR commands (NEC/Sony/Samsung/
 *     LG/Panasonic) via WebSocket → M5StickC fires IR at appliances
 *   - BUZZER FEEDBACK: Distinct tones for recording, response, error
 *
 * ── Button Navigation ──────────────────────────────────────
 *   Button A short   → Next screen / Back
 *   Button A long    → Settings / Start voice recording
 *   Button A hold    → Continuous voice recording (longform mode)
 *   Button B short   → Scroll / Cycle
 *   Button B long    → Select / Quick Reply / Trigger camera vision
 *   Button A+B combo → Toggle silent mode
 *
 * ── Gesture Navigation ─────────────────────────────────────
 *   Wrist raise      → Wake display
 *   Shake            → Stop current agent task
 *   Tilt left        → Scroll up
 *   Tilt right       → Scroll down
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
#include <ArduinoJson.h>

#include "hw_config.h"
#include "m5_driver.h"
#include "config_manager.h"
#include "gateway_client.h"
#include "flipper_ble.h"
#include "supabase_bridge.h"
#include "web_portal.h"
#include "ui_manager.h"

/* ── v2.0 Sensor modules ──────────────────────────────────── */
#include "audio_streamer.h"
#include "imu_classifier.h"
#include "ir_controller.h"
#include "vision_trigger.h"

/* ── Module instances ─────────────────────────────────────── */
static ConfigManager  configMgr;
static GatewayClient  gateway;
static FlipperBLE     flipper;
static SupabaseBridge supabase;
static WebPortal      webPortal;
static UIManager      ui;

/* ── v2.0 Sensor instances ────────────────────────────────── */
static AudioStreamer  audioStreamer;
static ImuClassifier imuClassifier;
static IrController  irController;
static VisionTrigger visionTrigger;

/* ── Voice recording state ────────────────────────────────── */
static bool _btnAHeld         = false;
static uint32_t _btnAHeldMs   = 0;
static bool _silentMode       = false;
static bool _btnABCombo       = false;

/* ── LVGL display flush callback ──────────────────────────── */
static lv_display_t* lvDisplay = nullptr;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area,
                           uint8_t* px_map) {
    m5_display_flush(area->x1, area->y1, area->x2, area->y2,
                     (const lv_color_t*)px_map);
    lv_display_flush_ready(disp);
}

static uint32_t lvgl_tick_cb() { return millis(); }

/* ── WiFi management ──────────────────────────────────────── */
static bool     _wifiConnected = false;
static uint32_t _wifiRetryMs   = 0;
static bool     _ntpSynced     = false;

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

            if (!_ntpSynced) {
                const AppConfig& cfg = configMgr.config();
                if (rtc_sync_from_ntp("pool.ntp.org", cfg.gmtOffset)) {
                    _ntpSynced = true;
                    Serial.println("[RTC] NTP sync successful");
                }
            }

            const AppConfig& cfg = configMgr.config();
            if (strlen(cfg.gatewayHost) > 0) {
                gateway.begin(cfg.gatewayHost, cfg.gatewayPort,
                              cfg.gatewayToken, cfg.gatewayUseSsl);
            }
            if (strlen(cfg.supabaseUrl) > 0) {
                supabase.begin(cfg.supabaseUrl, cfg.supabaseKey);
            }
        } else {
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
        if (millis() - _wifiRetryMs > WIFI_RECONNECT_INTERVAL) {
            _wifiRetryMs = millis();
            wifi_connect();
        }
    }
}

/* ── RTC / Clock tick ─────────────────────────────────────── */
static void clock_tick() {
    static uint32_t clockTimer = 0;
    if (millis() - clockTimer < 1000) return;
    clockTimer = millis();

    RtcTime t;
    if (rtc_get_time(&t)) {
        ui.updateTime(t.hour, t.minute, t.second);
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

/* ── Battery tick ─────────────────────────────────────────── */
static void battery_tick() {
    static uint32_t batTimer = 0;
    if (millis() - batTimer < 30000) return;
    batTimer = millis();
    uint32_t raw = analogRead(38);
    float voltage = raw * 3.3f / 4095.0f * 2.0f;
    int percent = constrain((int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f), 0, 100);
    bool charging = (voltage > 4.15f);
    ui.updateBattery(percent, charging);
}

/* ── Buzzer helpers ───────────────────────────────────────── */
static void buzzer_tone(uint16_t freq, uint16_t durationMs) {
    if (_silentMode) return;
    ledcSetup(0, freq, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 128);
    delay(durationMs);
    ledcWrite(0, 0);
    ledcDetachPin(BUZZER_PIN);
}

static void buzzer_recording_start() { buzzer_tone(880, 80); }
static void buzzer_recording_stop()  { buzzer_tone(660, 80); }
static void buzzer_response()        { buzzer_tone(1047, 60); delay(30); buzzer_tone(1319, 60); }
static void buzzer_error()           { buzzer_tone(300, 200); }

/* ══════════════════════════════════════════════════════════
 *  AUDIO STREAMING CALLBACKS
 * ══════════════════════════════════════════════════════════ */
static void onAudioChunk(const uint8_t* data, size_t len) {
    /* Forward raw PCM bytes to gateway as binary WebSocket frame */
    gateway.sendBinary(data, len);
}

static void onAudioStateChange(StreamerState state) {
    switch (state) {
        case STREAMER_RECORDING:
            buzzer_recording_start();
            ui.setRecordingOverlay(true, false);
            break;
        case STREAMER_SENDING:
            ui.setRecordingOverlay(false, true);
            break;
        case STREAMER_IDLE:
            buzzer_recording_stop();
            ui.setRecordingOverlay(false, false);
            break;
        case STREAMER_ERROR:
            buzzer_error();
            ui.setRecordingOverlay(false, false);
            break;
    }
}

/* ══════════════════════════════════════════════════════════
 *  IMU CLASSIFIER CALLBACKS
 * ══════════════════════════════════════════════════════════ */
static void onGesture(GestureEvent gesture) {
    switch (gesture) {
        case GESTURE_WRIST_RAISE:
            if (ui.isSleeping()) ui.wakeDisplay();
            break;

        case GESTURE_SHAKE:
            /* Send stop command to Huonyx agent */
            if (gateway.isConnected()) {
                gateway.sendTextMessage("{\"type\":\"stop_task\"}");
                ui.showNotification("Huonyx", "Task stopped");
                buzzer_tone(440, 100);
            }
            break;

        case GESTURE_TILT_LEFT:
            ui.scrollUp();
            break;

        case GESTURE_TILT_RIGHT:
            ui.scrollDown();
            break;

        case GESTURE_DOUBLE_TAP:
            /* Repeat last agent response via buzzer (Morse) — future */
            break;

        default:
            break;
    }
}

static void onActivity(ActivityState state) {
    /* Activity change logged — context will be injected on next message */
    Serial.printf("[IMU] Activity changed: %s\n", imuClassifier.activityName());
}

/* ══════════════════════════════════════════════════════════
 *  IR CONTROLLER CALLBACKS
 * ══════════════════════════════════════════════════════════ */
static void onIrResult(IrResult result, const char* protocol) {
    if (result == IR_OK) {
        char buf[48];
        snprintf(buf, sizeof(buf), "IR %s sent", protocol);
        ui.showNotification("IR Remote", buf);
    } else {
        ui.showNotification("IR Remote", "Command failed");
        buzzer_error();
    }
}

/* ══════════════════════════════════════════════════════════
 *  VISION TRIGGER CALLBACKS
 * ══════════════════════════════════════════════════════════ */
static void onVisionResult(const char* analysis) {
    ui.addChatMessage(analysis, false);
    ui.showScreen(SCREEN_CHAT);
    buzzer_response();
}

static void onVisionStateChange(VisionState state) {
    if (state == VISION_WAITING) {
        ui.showNotification("Vision", "Analyzing...");
    }
}

/* ══════════════════════════════════════════════════════════
 *  GATEWAY CALLBACKS
 * ══════════════════════════════════════════════════════════ */
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
        buzzer_response();
    } else if (!isFinal) {
        ui.updateAgentTyping(true);
    }
}

/* Handle special message types from gateway (IR commands, vision responses) */
static void onGatewayJson(const char* jsonStr) {
    StaticJsonDocument<512> doc;
    if (deserializeJson(doc, jsonStr) != DeserializationError::Ok) return;

    const char* type = doc["type"] | "";

    if (strcmp(type, "ir_command") == 0) {
        /* Agent wants to fire an IR command */
        irController.handleCommand(doc.as<JsonObject>());

    } else if (strcmp(type, "vision_response") == 0) {
        /* Server returning vision analysis result */
        const char* analysis = doc["analysis"] | "";
        visionTrigger.onVisionResponse(analysis);

    } else if (strcmp(type, "voice_transcript") == 0) {
        /* Server returning Whisper transcription */
        const char* transcript = doc["text"] | "";
        if (strlen(transcript) > 0) {
            /* Inject IMU context into the message before sending to agent */
            char contextMsg[512];
            snprintf(contextMsg, sizeof(contextMsg), "%s\n%s",
                     transcript, imuClassifier.contextString());
            gateway.sendTextMessage(contextMsg);
        }
    }
}

/* ── Flipper callbacks ────────────────────────────────────── */
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

/* ── Supabase callbacks ───────────────────────────────────── */
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
 *  BUTTON HANDLING (extended for voice + vision)
 * ══════════════════════════════════════════════════════════ */
static uint32_t _lastInteraction = 0;

static void handle_button_extended(BtnEvent evt) {
    _lastInteraction = millis();

    /* ── Button A+B combo → toggle silent mode ──────────── */
    if (evt == BTN_A_B_COMBO) {
        _silentMode = !_silentMode;
        char buf[32];
        snprintf(buf, sizeof(buf), "Silent mode %s", _silentMode ? "ON" : "OFF");
        ui.showNotification("Settings", buf);
        if (!_silentMode) buzzer_tone(880, 60);
        return;
    }

    /* ── Button A long → start voice recording ──────────── */
    if (evt == BTN_A_LONG) {
        if (!audioStreamer.isRecording()) {
            /* Determine mode: short long-press = standard, very long = longform */
            RecordMode mode = (millis() - _btnAHeldMs > 3000) ? REC_LONGFORM : REC_STANDARD;
            audioStreamer.startRecording(mode);
            ui.showScreen(SCREEN_CHAT);
        } else {
            audioStreamer.stopRecording();
        }
        return;
    }

    /* ── Button B long → trigger vision capture ─────────── */
    if (evt == BTN_B_LONG) {
        if (!visionTrigger.isWaiting()) {
            /* Build and send vision command via gateway */
            String cmd = visionTrigger.buildCommand(VISION_DESCRIBE);
            gateway.sendTextMessage(cmd.c_str());
            visionTrigger.capture(VISION_DESCRIBE);
        }
        return;
    }

    /* ── All other buttons → normal UI handling ─────────── */
    ui.handleButton(evt);
}

/* ══════════════════════════════════════════════════════════
 *  SETUP
 * ══════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);
    Serial.println("\n[Huonyx] M5StickC Plus2 v2.0 (Sensor Edition) starting...");

    /* ── Hardware init ────────────────────────────────────── */
    m5_driver_init();

    /* ── LVGL init ────────────────────────────────────────── */
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);

    lvDisplay = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);

    static lv_color_t buf1[LVGL_BUF_SIZE];
    static lv_color_t buf2[LVGL_BUF_SIZE];
    lv_display_set_buffers(lvDisplay, buf1, buf2,
                           sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(lvDisplay, lvgl_flush_cb);

    /* ── Config ───────────────────────────────────────────── */
    configMgr.begin();

    /* ── UI ───────────────────────────────────────────────── */
    ui.begin(&gateway, &configMgr, &flipper, &supabase);

    /* ── v2.0: Audio Streamer ─────────────────────────────── */
    audioStreamer.onChunk(onAudioChunk);
    audioStreamer.onStateChange(onAudioStateChange);
    audioStreamer.begin();

    /* ── v2.0: IMU Classifier ─────────────────────────────── */
    imuClassifier.onGesture(onGesture);
    imuClassifier.onActivity(onActivity);
    imuClassifier.begin();

    /* ── v2.0: IR Controller ──────────────────────────────── */
    irController.onResult(onIrResult);
    irController.begin();

    /* ── v2.0: Vision Trigger ─────────────────────────────── */
    visionTrigger.onResult(onVisionResult);
    visionTrigger.onStateChange(onVisionStateChange);
    visionTrigger.begin("default");  /* Camera ID configured in Huonyx server */

    /* ── Gateway callbacks ────────────────────────────────── */
    gateway.onStateChange(onGatewayStateChange);
    gateway.onChatDelta(onChatDelta);
    gateway.onJsonMessage(onGatewayJson);   /* New: handles IR/vision/transcript */

    /* ── Flipper callbacks ────────────────────────────────── */
    flipper.onStateChange(onFlipperStateChange);
    flipper.onData(onFlipperData);

    /* ── Supabase callbacks ───────────────────────────────── */
    supabase.onStateChange(onBridgeStateChange);
    supabase.onCommand(onBridgeCommand);

    /* ── Web portal ───────────────────────────────────────── */
    webPortal.begin(&configMgr);

    /* ── WiFi ─────────────────────────────────────────────── */
    const AppConfig& cfg = configMgr.config();
    if (strlen(cfg.wifiSsid) > 0) {
        wifi_connect();
    } else {
        webPortal.startAP();
        ui.showScreen(SCREEN_WIFI_SETUP);
    }

    /* ── Auto-connect Flipper ─────────────────────────────── */
    if (cfg.flipperAutoConnect && strlen(cfg.flipperDeviceName) > 0) {
        flipper.begin(cfg.flipperDeviceName);
    }

    ui.updateBattery(100, false);
    _lastInteraction = millis();

    Serial.println("[Huonyx] Setup complete — v2.0 Sensor Edition");
    led_blink(3, 150);  /* 3 blinks = v2 */
}

/* ══════════════════════════════════════════════════════════
 *  LOOP
 * ══════════════════════════════════════════════════════════ */
void loop() {
    /* ── Button polling (extended) ────────────────────────── */
    BtnEvent evt = m5_driver_tick();
    if (evt != BTN_NONE) {
        handle_button_extended(evt);
    }

    /* ── LVGL tick ────────────────────────────────────────── */
    lv_timer_handler();

    /* ── Network ticks ────────────────────────────────────── */
    wifi_tick();
    gateway.loop();
    supabase.loop();
    flipper.loop();
    webPortal.loop();

    /* ── v2.0: Audio streaming (Core 1 side) ─────────────── */
    audioStreamer.loop();

    /* ── v2.0: IMU classifier ─────────────────────────────── */
    imuClassifier.loop();

    /* ── Sensor ticks ─────────────────────────────────────── */
    clock_tick();
    battery_tick();

    /* ── Sleep management ─────────────────────────────────── */
    if (!ui.isSleeping() && !audioStreamer.isRecording() &&
        (millis() - _lastInteraction > SLEEP_TIMEOUT_MS)) {
        ui.sleepDisplay();
    }

    delay(5);
}
