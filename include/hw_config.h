/**
 * Hardware Configuration for ESP32-2424S012
 * Huonyx AI Smartwatch
 */

#ifndef HW_CONFIG_H
#define HW_CONFIG_H

/* ── Board Identification ─────────────────────────────── */
#define BOARD_NAME          "ESP32-2424S012"
#define FIRMWARE_VERSION    "1.0.0"
#define FIRMWARE_NAME       "Huonyx Watch"

/* ── Display (GC9A01 via SPI) ─────────────────────────── */
#define TFT_WIDTH           240
#define TFT_HEIGHT          240
#define PIN_TFT_MOSI        7
#define PIN_TFT_SCLK        6
#define PIN_TFT_CS           10
#define PIN_TFT_DC           2
#define PIN_TFT_RST         -1
#define PIN_TFT_BL           3
#define TFT_BACKLIGHT_ON     HIGH

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

/* ── LVGL Buffer Config ───────────────────────────────── */
#define LVGL_BUF_SIZE        (TFT_WIDTH * 24)  /* ~24 lines buffer */

/* ── WiFi ─────────────────────────────────────────────── */
#define WIFI_CONNECT_TIMEOUT_MS  15000
#define WIFI_RECONNECT_INTERVAL  30000

/* ── Gateway ──────────────────────────────────────────── */
#define GATEWAY_DEFAULT_PORT     18789
#define GATEWAY_RECONNECT_MS     5000
#define GATEWAY_TICK_TIMEOUT_MS  90000
#define CHAT_MAX_MESSAGES        8
#define CHAT_MAX_MSG_LEN         256

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

#endif /* HW_CONFIG_H */
