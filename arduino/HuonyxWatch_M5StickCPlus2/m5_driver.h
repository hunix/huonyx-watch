/**
 * m5_driver.h
 * M5StickC Plus2 Hardware Driver
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * Handles:
 *   - ST7789V2 display via M5GFX
 *   - MPU6886 IMU (wake on wrist raise)
 *   - BM8563 RTC (hardware timekeeping)
 *   - Button A/B debounce and long-press detection
 *   - Passive buzzer notifications
 *   - Red LED control
 *   - Power hold management
 */
#ifndef M5_DRIVER_H
#define M5_DRIVER_H

#include <Arduino.h>
#include <M5GFX.h>
#include <Wire.h>
#include "hw_config.h"

/* ── Button event types ───────────────────────────────── */
enum BtnEvent : uint8_t {
    BTN_NONE        = 0,
    BTN_A_SHORT     = 1,
    BTN_A_LONG      = 2,
    BTN_B_SHORT     = 3,
    BTN_B_LONG      = 4,
};

/* ── IMU data structure ───────────────────────────────── */
struct ImuData {
    float accX, accY, accZ;   /* Acceleration in g */
    float gyrX, gyrY, gyrZ;   /* Gyroscope in deg/s */
};

/* ── RTC time structure ───────────────────────────────── */
struct RtcTime {
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    uint8_t  weekday;
};

/* ── Global display instance ──────────────────────────── */
extern M5GFX display;

/* ── Driver API ───────────────────────────────────────── */

/**
 * Initialize all hardware (display, I2C, IMU, RTC, buttons, buzzer).
 * Must be called once in setup() before LVGL initialization.
 */
void m5_driver_init();

/**
 * Must be called in loop() to poll buttons and IMU.
 * Returns the latest button event (or BTN_NONE).
 */
BtnEvent m5_driver_tick();

/* Display */
void m5_set_backlight(uint8_t brightness);  /* 0-255 */
void m5_display_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                      const lv_color_t* colormap);

/* RTC */
bool rtc_get_time(RtcTime* t);
bool rtc_set_time(const RtcTime* t);
bool rtc_sync_from_ntp(const char* ntpServer = "pool.ntp.org", long gmtOffset = 0);

/* IMU */
bool imu_read(ImuData* data);
bool imu_detect_wrist_raise();

/* Buzzer */
void buzzer_notify();           /* Single short beep for notifications */
void buzzer_error();            /* Double beep for errors */
void buzzer_success();          /* Rising tone for success */

/* LED */
void led_set(bool on);          /* Red LED control */
void led_blink(uint8_t times, uint16_t intervalMs);

/* Power */
void power_hold();              /* Call once in setup to keep device on */
void power_off();               /* Graceful shutdown */

#endif /* M5_DRIVER_H */
