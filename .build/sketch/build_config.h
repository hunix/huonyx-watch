#line 1 "C:\\Users\\HK\\sources\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\build_config.h"
/**
 * build_config.h
 * Huonyx AI Smartwatch - Compile-time configuration
 *
 * All defines that were previously passed as -D compiler flags
 * are placed here instead, so no --build-property arguments are
 * needed with arduino-cli or Arduino IDE.
 *
 * MUST be the very first include in HuonyxWatch.ino (before all others).
 */

#ifndef BUILD_CONFIG_H
#define ARDUINO_USB_MODE 1
#define ARDUINO_USB_CDC_ON_BOOT 1
#define BUILD_CONFIG_H

/* ── TFT_eSPI ─────────────────────────────────────────────
 * USER_SETUP_LOADED tells TFT_eSPI to use the User_Setup.h
 * in the sketch folder (patched by flash.ps1) instead of
 * searching the library folder.
 */
// #define USER_SETUP_LOADED 1


/* ── LVGL ─────────────────────────────────────────────────
 * LV_CONF_INCLUDE_SIMPLE tells LVGL to find lv_conf.h by
 * a simple #include "lv_conf.h" rather than a relative path.
 * This works because flash.ps1 copies lv_conf.h next to the
 * lvgl library folder AND into lvgl/src/.
 */
#define LV_CONF_INCLUDE_SIMPLE 1

/* ── NimBLE-Arduino ───────────────────────────────────────
 * Configure NimBLE for Central-only (BLE scanner/client)
 * operation to save memory on the ESP32-C3.
 * These must be defined before any NimBLE header is included.
 */
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL       1
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL    0
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER   0
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER      1
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS    1
#define CONFIG_BT_NIMBLE_MAX_BONDS          1
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU  128

/* ── Debug ────────────────────────────────────────────────
 * Disable ESP32 core debug output to save flash/RAM.
 */
#define CORE_DEBUG_LEVEL 4

#endif /* BUILD_CONFIG_H */
