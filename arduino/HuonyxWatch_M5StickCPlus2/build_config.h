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

/* ── M5StickC Plus2 specific ──────────────────────────────
 * Tell M5Unified/M5GFX which board we are targeting.
 */
#define ARDUINO_M5STACK_STICKC_PLUS2 1

/* ── Debug ────────────────────────────────────────────────
 * Set to 0 for production, 4 for verbose debugging.
 */
#define CORE_DEBUG_LEVEL 0

#endif /* BUILD_CONFIG_H */
