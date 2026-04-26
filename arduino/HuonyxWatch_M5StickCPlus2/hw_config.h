/**
 * hw_config.h
 * Hardware Configuration for M5StickC Plus2
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * Target: ESP32-PICO-V3-02 (240MHz, 8MB Flash, 2MB PSRAM)
 * Display: ST7789V2 135x240
 * Input: 3x Physical Buttons (no touch screen)
 * Sensors: MPU6886 (IMU), BM8563 (RTC), SPM1423 (Mic)
 * Extras: IR Emitter, Passive Buzzer, Red LED
 */
#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ── Board Identification ─────────────────────────────── */
#define BOARD_NAME          "M5StickC-Plus2"
#define FIRMWARE_VERSION    "2.0.0"
#define FIRMWARE_NAME       "Huonyx Watch"
#define FIRMWARE_VARIANT    "M5StickC-Plus2"

/* ── Display (ST7789V2 via SPI) ───────────────────────── */
#define TFT_WIDTH           135
#define TFT_HEIGHT          240
#define PIN_TFT_MOSI        15
#define PIN_TFT_SCLK        13
#define PIN_TFT_CS           5
#define PIN_TFT_DC          14
#define PIN_TFT_RST         12
#define PIN_TFT_BL          27
#define TFT_BACKLIGHT_ON    HIGH
#define TFT_ROTATION        0   /* Portrait: 135x240 */

/* ── Buttons ──────────────────────────────────────────── */
#define PIN_BTN_A           37  /* Front button (top face) */
#define PIN_BTN_B           39  /* Side button */
#define PIN_BTN_C           35  /* Power/Reset button */
#define BTN_ACTIVE_LOW      1   /* Buttons pull LOW when pressed */
#define BTN_LONG_PRESS_MS   600 /* Long press threshold */

/* ── IMU (MPU6886 via I2C) ────────────────────────────── */
#define PIN_I2C_SDA         21
#define PIN_I2C_SCL         22
#define MPU6886_I2C_ADDR    0x68
#define IMU_WAKE_THRESHOLD  0.8f /* g-force threshold for wrist raise */

/* ── RTC (BM8563 via I2C) ─────────────────────────────── */
#define BM8563_I2C_ADDR     0x51

/* ── Microphone (SPM1423 PDM) ─────────────────────────── */
#define PIN_MIC_CLK          0
#define PIN_MIC_DATA        34

/* Aliases used by audio_streamer.cpp */
#define MIC_CLK_PIN          PIN_MIC_CLK
#define MIC_DATA_PIN         PIN_MIC_DATA

/* ── IR Emitter ───────────────────────────────────────── */
#define PIN_IR_TX           19

/* Alias used by ir_controller.cpp */
#define IR_TX_PIN            PIN_IR_TX

/* ── Red LED ──────────────────────────────────────────── */
#define PIN_LED             10  /* Active LOW */

/* ── Passive Buzzer ───────────────────────────────────── */
#define PIN_BUZZER           2

/* Alias used by main .ino */
#define BUZZER_PIN           PIN_BUZZER
#define BUZZER_NOTIFY_FREQ  2000 /* Hz for notification beep */
#define BUZZER_NOTIFY_MS     80  /* Duration of notification beep */

/* ── Power Hold ───────────────────────────────────────── */
#define PIN_POWER_HOLD       4  /* Must be held HIGH to keep device on */

/* ── LVGL Buffer Config ───────────────────────────────── */
/* 135 * 20 lines = 2700 pixels * 2 bytes = 5.4KB per buffer */
#define LVGL_BUF_LINES      20
#define LVGL_BUF_SIZE       (TFT_WIDTH * LVGL_BUF_LINES)

/* ── WiFi ─────────────────────────────────────────────── */
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_RECONNECT_INTERVAL  30000

/* ── Gateway ──────────────────────────────────────────── */
#define GATEWAY_DEFAULT_PORT     18789
#define GATEWAY_RECONNECT_MS     5000
#define GATEWAY_TICK_TIMEOUT_MS  90000
#define CHAT_MAX_MESSAGES        6   /* Reduced from 8 for smaller screen */
#define CHAT_MAX_MSG_LEN         256

/* ── Flipper Zero BLE ─────────────────────────────────── */
#define FLIPPER_BLE_SERVICE_UUID      "8fe5b3d5-2e7f-4a98-2a48-7acc60569438"
#define FLIPPER_BLE_RX_CHAR_UUID      "19ed82ae-ed21-4c9d-4145-228e62fe0000"
#define FLIPPER_BLE_TX_CHAR_UUID      "19ed82ae-ed21-4c9d-4145-228e63fe0000"
#define FLIPPER_SCAN_TIMEOUT_SEC      10
#define FLIPPER_RECONNECT_MS          5000
#define FLIPPER_CMD_QUEUE_SIZE        4
#define FLIPPER_RESPONSE_BUF_SIZE     384
#define FLIPPER_CMD_TIMEOUT_MS        10000

/* ── Supabase Realtime ────────────────────────────────── */
#define SUPABASE_WS_PATH             "/realtime/v1/websocket"
#define SUPABASE_BROADCAST_PATH      "/realtime/v1/api/broadcast"
#define SUPABASE_CHANNEL_TOPIC       "realtime:flipper-bridge"
#define SUPABASE_HEARTBEAT_MS        30000
#define SUPABASE_RECONNECT_MS        5000
#define SUPABASE_DEFAULT_PORT        443

/* ── NVS Keys ─────────────────────────────────────────── */
#define NVS_NAMESPACE           "huonyx"
#define NVS_KEY_WIFI_SSID       "wifi_ssid"
#define NVS_KEY_WIFI_PASS       "wifi_pass"
#define NVS_KEY_GW_HOST         "gw_host"
#define NVS_KEY_GW_PORT         "gw_port"
#define NVS_KEY_GW_TOKEN        "gw_token"
#define NVS_KEY_GW_USE_SSL      "gw_ssl"
#define NVS_KEY_BRIGHTNESS      "brightness"
#define NVS_KEY_TIMEZONE        "timezone"
#define NVS_KEY_SB_URL          "sb_url"
#define NVS_KEY_SB_KEY          "sb_key"
#define NVS_KEY_FLIP_NAME       "flip_name"
#define NVS_KEY_FLIP_AUTO       "flip_auto"

/* ── Audio Streaming ─────────────────────────────────── */
#define AUDIO_STREAM_SAMPLE_RATE  16000   /* Hz — matches Whisper optimal */
#define AUDIO_STREAM_CHUNK_MS     32      /* ms per WebSocket frame */

/* ── Sleep ────────────────────────────────────────────── */
#define SLEEP_TIMEOUT_MS     30000  /* 30s inactivity before display sleep */

/* ── UI Navigation Screens ────────────────────────────── */
enum ScreenId : uint8_t {
    SCREEN_WATCHFACE = 0,
    SCREEN_CHAT,
    SCREEN_FLIPPER,
    SCREEN_SETTINGS,
    SCREEN_WIFI_SETUP,
    SCREEN_GATEWAY_SETUP,
    SCREEN_SUPABASE_SETUP,
    SCREEN_FLIPPER_SETUP,
    SCREEN_COUNT
};

/* ── Quick Reply Buttons ──────────────────────────────── */
#define QUICK_REPLY_COUNT   4

#endif /* HW_CONFIG_H */
