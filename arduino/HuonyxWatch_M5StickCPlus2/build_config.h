/**
 * build_config.h
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 * Compile-time configuration
 *
 * MUST be the very first include in HuonyxWatch_M5StickCPlus2.ino
 */
#ifndef BUILD_CONFIG_H
#define BUILD_CONFIG_H

/* ── LVGL ─────────────────────────────────────────────────
 * LV_CONF_INCLUDE_SIMPLE tells LVGL to find lv_conf.h by
 * a simple #include "lv_conf.h" rather than a relative path.
 */
#define LV_CONF_INCLUDE_SIMPLE 1

/* ── Flipper Zero BLE ────────────────────────────────────
 * Set to 1 to enable direct BLE connection to Flipper Zero.
 * Set to 0 to disable BLE entirely and save ~47KB IRAM.
 *
 * When disabled, Flipper control is still available through
 * the Supabase bridge (WebSocket over WiFi).
 *
 * NOTE: The ESP32 Arduino 3.x BLE stack (NimBLE) uses ~47KB
 * of IRAM which exceeds the 176KB hardware limit when combined
 * with WiFi + LVGL. Disable BLE if you get IRAM overflow.
 */
#define ENABLE_FLIPPER_BLE  0

#if ENABLE_FLIPPER_BLE
/* ── NimBLE-Arduino ───────────────────────────────────────
 * Configure NimBLE for Central-only (BLE scanner/client)
 * operation to save memory.
 */
#define CONFIG_BT_NIMBLE_ROLE_CENTRAL       1
#define CONFIG_BT_NIMBLE_ROLE_PERIPHERAL    0
#define CONFIG_BT_NIMBLE_ROLE_BROADCASTER   0
#define CONFIG_BT_NIMBLE_ROLE_OBSERVER      1
#define CONFIG_BT_NIMBLE_MAX_CONNECTIONS    1
#define CONFIG_BT_NIMBLE_MAX_BONDS          1
#define CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU  128
#endif /* ENABLE_FLIPPER_BLE */

/* ── M5StickC Plus2 specific ──────────────────────────────
 * Tell M5Unified/M5GFX which board we are targeting.
 */
#define ARDUINO_M5STACK_STICKC_PLUS2 1

/* ── Debug ────────────────────────────────────────────────
 * Set to 0 for production, 4 for verbose debugging.
 */
#define CORE_DEBUG_LEVEL 0

#endif /* BUILD_CONFIG_H */
