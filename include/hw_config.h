/**
 * Hardware Configuration for ESP32-2424S012
 * Huonyx AI Smartwatch
 */

#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ── Board Identification ─────────────────────────────── */
#define BOARD_NAME          "ESP32-2424S012"
#define FIRMWARE_VERSION    "3.1.0"
#define FIRMWARE_NAME       "Huonyx Watch"
#define OTA_HOSTNAME        "huonyx-watch"  /* mDNS hostname for ArduinoOTA */

/* ── Display (GC9A01 via SPI) ─────────────────────────── */
#define TFT_WIDTH           240
#define TFT_HEIGHT          240
#define PIN_TFT_MOSI        7
#define PIN_TFT_SCLK        6
#define PIN_TFT_CS          10
#define PIN_TFT_DC           2
#define PIN_TFT_RST         -1
#define PIN_TFT_BL           3
#define TFT_BACKLIGHT_ON    HIGH

/* ── Touch (CST816D via I2C) ──────────────────────────── */
#define PIN_TOUCH_SDA        4
#define PIN_TOUCH_SCL        5
#define PIN_TOUCH_INT        0
#define PIN_TOUCH_RST        1
#define TOUCH_I2C_ADDR       0x15

/* ── Buttons ──────────────────────────────────────────── */
#define PIN_BOOT_BTN         9

/* ── Battery (IP5306 via I2C) ─────────────────────────── */
#define IP5306_I2C_ADDR      0x75
#define BATTERY_LOW_PCT      15   /* % — show warning overlay  */
#define BATTERY_CRIT_PCT     5    /* % — arc turns red, unsafe to reset */

/* ── LVGL Buffer Config ───────────────────────────────── */
/* Two equal buffers → DMA double-buffering → eliminates tearing */
#define LVGL_BUF_SIZE        (TFT_WIDTH * 16)   /* ~16 lines per buffer */

/* ── WiFi ─────────────────────────────────────────────── */
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_RECONNECT_INTERVAL  30000

/* ── Gateway ──────────────────────────────────────────── */
#define GATEWAY_DEFAULT_PORT      18789
#define GATEWAY_RECONNECT_MS       5000   /* Initial backoff interval */
#define GATEWAY_RECONNECT_MAX_MS  120000  /* Exponential backoff cap  */
#define GATEWAY_TICK_TIMEOUT_MS   90000
#define CHAT_MAX_MESSAGES          8
#define CHAT_MAX_MSG_LEN           256

/* ── Flipper Zero BLE ─────────────────────────────────── */
#define FLIPPER_BLE_SERVICE_UUID      "8fe5b3d5-2e7f-4a98-2a48-7acc60569438"
#define FLIPPER_BLE_RX_CHAR_UUID      "19ed82ae-ed21-4c9d-4145-228e62fe0000"
#define FLIPPER_BLE_TX_CHAR_UUID      "19ed82ae-ed21-4c9d-4145-228e63fe0000"
#define FLIPPER_SCAN_TIMEOUT_SEC      10
#define FLIPPER_RECONNECT_MS           5000   /* Initial backoff interval */
#define FLIPPER_RECONNECT_MAX_MS      120000  /* Exponential backoff cap  */
#define FLIPPER_CMD_QUEUE_SIZE         8      /* Doubled from 4 */
#define FLIPPER_RESPONSE_BUF_SIZE      384
#define FLIPPER_CMD_TIMEOUT_MS         10000
#define FLIPPER_CMD_HISTORY_SIZE       4      /* Recent commands ring-buffer */

/* ── Supabase Realtime ────────────────────────────────── */
#define SUPABASE_WS_PATH             "/realtime/v1/websocket"
#define SUPABASE_BROADCAST_PATH      "/realtime/v1/api/broadcast"
#define SUPABASE_CHANNEL_TOPIC       "realtime:flipper-bridge"
#define SUPABASE_HEARTBEAT_MS        30000
#define SUPABASE_RECONNECT_MS         5000   /* Initial backoff interval */
#define SUPABASE_RECONNECT_MAX_MS    120000  /* Exponential backoff cap  */
#define SUPABASE_DEFAULT_PORT        443

/* ── Heap Safety Thresholds ───────────────────────────── */
#define HEAP_DANGER_BYTES    10240   /* < 10 KB → log warn          */
#define HEAP_CRITICAL_BYTES   8192   /* < 8 KB for N checks → reboot */
#define HEAP_CRITICAL_STRIKES    3   /* consecutive bad readings     */

/* ── NVS Keys ─────────────────────────────────────────── */
#define NVS_NAMESPACE           "huonyx"
#define NVS_KEY_WIFI_SSID       "wifi_ssid"
#define NVS_KEY_WIFI_PASS       "wifi_pass"
#define NVS_KEY_GW_HOST         "gw_host"
#define NVS_KEY_GW_PORT         "gw_port"
#define NVS_KEY_GW_TOKEN        "gw_token"
#define NVS_KEY_GW_USE_SSL      "gw_ssl"
#define NVS_KEY_GW_FINGERPRINT  "gw_fp"
#define NVS_KEY_BRIGHTNESS      "brightness"
#define NVS_KEY_TIMEZONE        "timezone"
#define NVS_KEY_SB_URL          "sb_url"
#define NVS_KEY_SB_KEY          "sb_key"
#define NVS_KEY_SB_FINGERPRINT  "sb_fp"
#define NVS_KEY_FLIP_NAME       "flip_name"
#define NVS_KEY_FLIP_AUTO       "flip_auto"

#endif /* HW_CONFIG_H */
