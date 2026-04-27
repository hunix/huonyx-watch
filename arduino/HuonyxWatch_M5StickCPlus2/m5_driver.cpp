/**
 * m5_driver.cpp
 * M5StickC Plus2 Hardware Driver Implementation
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 *
 * v2.1 FIX: Replaced raw M5GFX + Wire.begin() with M5Unified M5.begin().
 * The AXP2101 PMIC must be initialized first or the display stays blank.
 */
#include "build_config.h"
#include "m5_driver.h"
#include <WiFi.h>
#include <time.h>

/* ── Button state tracking ────────────────────────────── */
static bool     _btnAState     = false;
static bool     _btnBState     = false;
static uint32_t _btnAPressMs   = 0;
static uint32_t _btnBPressMs   = 0;
static bool     _btnAHandled   = false;
static bool     _btnBHandled   = false;

/* ══════════════════════════════════════════════════════════
 *  INITIALIZATION
 * ══════════════════════════════════════════════════════════ */
void m5_driver_init() {
    /* ── M5Unified begin ─────────────────────────────────
     * This is the ONLY correct way to initialize the M5StickC Plus2.
     * M5.begin() initializes the AXP2101 PMIC first, which enables
     * the display power rail, then initializes the display, IMU, and RTC.
     * Without this, the screen stays completely dark.
     * ─────────────────────────────────────────────────── */
    auto cfg = M5.config();
    cfg.internal_imu = true;   /* Enable MPU6886 */
    cfg.internal_rtc = true;   /* Enable BM8563 */
    cfg.internal_mic = false;  /* We drive the SPM1423 mic via I2S directly */
    cfg.internal_spk = false;  /* No speaker on M5StickC Plus2 */
    M5.begin(cfg);

    /* ── Display ──────────────────────────────────────── */
    M5.Display.setRotation(TFT_ROTATION);
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setBrightness(128);
    Serial.println("[M5] Display initialized via M5Unified");

    /* ── IMU ──────────────────────────────────────────── */
    if (M5.Imu.isEnabled()) {
        Serial.println("[M5] IMU (MPU6886) initialized via M5Unified");
    } else {
        Serial.println("[M5] WARNING: IMU not detected");
    }

    /* ── RTC ──────────────────────────────────────────── */
    if (M5.Rtc.isEnabled()) {
        Serial.println("[M5] RTC (BM8563) initialized via M5Unified");
    } else {
        Serial.println("[M5] WARNING: RTC not detected");
    }

    /* ── Buttons ──────────────────────────────────────── */
    /* M5Unified configures button GPIOs internally, but we also set them
     * as INPUT_PULLUP for direct digitalRead() in our polling code */
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    Serial.println("[M5] Buttons initialized");

    /* ── Buzzer ───────────────────────────────────────── */
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    /* ── LED ──────────────────────────────────────────── */
    /* G19 is shared between the Red LED and the IR emitter RMT peripheral.
     * IrController::begin() (called after this) will configure G19 as RMT.
     * We set it HIGH here (LED off) before RMT takes over. */
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);  /* Active LOW — LED off */

    Serial.println("[M5] Hardware driver v2.1 initialized (M5Unified)");
}

/* ══════════════════════════════════════════════════════════
 *  BUTTON POLLING
 * ══════════════════════════════════════════════════════════ */
BtnEvent m5_driver_tick() {
    /* Update M5Unified internal button state */
    M5.update();

    uint32_t now = millis();
    BtnEvent evt = BTN_NONE;

    /* ── A+B combo detection (must check before individual buttons) ── */
    bool aDown = (digitalRead(PIN_BTN_A) == LOW);
    bool bDown = (digitalRead(PIN_BTN_B) == LOW);
    if (aDown && bDown) {
        _btnAHandled = true;
        _btnBHandled = true;
        return BTN_A_B_COMBO;
    }

    /* ── Button A ─────────────────────────────────────── */
    bool aPressed = aDown;
    if (aPressed && !_btnAState) {
        _btnAState   = true;
        _btnAPressMs = now;
        _btnAHandled = false;
    } else if (!aPressed && _btnAState) {
        _btnAState = false;
        if (!_btnAHandled) {
            uint32_t held = now - _btnAPressMs;
            evt = (held >= BTN_LONG_PRESS_MS) ? BTN_A_LONG : BTN_A_SHORT;
        }
    } else if (aPressed && _btnAState && !_btnAHandled) {
        if ((now - _btnAPressMs) >= BTN_LONG_PRESS_MS) {
            _btnAHandled = true;
            evt = BTN_A_LONG;
        }
    }

    /* ── Button B ─────────────────────────────────────── */
    bool bPressed = (digitalRead(PIN_BTN_B) == LOW);
    if (bPressed && !_btnBState) {
        _btnBState   = true;
        _btnBPressMs = now;
        _btnBHandled = false;
    } else if (!bPressed && _btnBState) {
        _btnBState = false;
        if (!_btnBHandled) {
            uint32_t held = now - _btnBPressMs;
            evt = (held >= BTN_LONG_PRESS_MS) ? BTN_B_LONG : BTN_B_SHORT;
        }
    } else if (bPressed && _btnBState && !_btnBHandled) {
        if ((now - _btnBPressMs) >= BTN_LONG_PRESS_MS) {
            _btnBHandled = true;
            evt = BTN_B_LONG;
        }
    }

    return evt;
}

/* ══════════════════════════════════════════════════════════
 *  DISPLAY
 * ══════════════════════════════════════════════════════════ */
void m5_set_backlight(uint8_t brightness) {
    M5.Display.setBrightness(brightness);
}

void m5_display_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                      const lv_color_t* colormap) {
    M5.Display.startWrite();
    M5.Display.setAddrWindow(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    M5.Display.pushPixels((uint16_t*)colormap, (x2 - x1 + 1) * (y2 - y1 + 1));
    M5.Display.endWrite();
}

/* ══════════════════════════════════════════════════════════
 *  BATTERY (via AXP2101 through M5Unified)
 * ══════════════════════════════════════════════════════════ */
float m5_battery_voltage() {
    return M5.Power.getBatteryVoltage() / 1000.0f;  /* mV → V */
}

uint8_t m5_battery_percent() {
    int pct = M5.Power.getBatteryLevel();
    if (pct < 0) return 0;
    if (pct > 100) return 100;
    return (uint8_t)pct;
}

bool m5_is_charging() {
    return M5.Power.isCharging();
}

/* ══════════════════════════════════════════════════════════
 *  RTC (BM8563 via M5Unified)
 * ══════════════════════════════════════════════════════════ */
bool rtc_get_time(RtcTime* t) {
    if (!M5.Rtc.isEnabled()) return false;
    auto dt = M5.Rtc.getDateTime();
    t->second  = dt.time.seconds;
    t->minute  = dt.time.minutes;
    t->hour    = dt.time.hours;
    t->day     = dt.date.date;
    t->month   = dt.date.month;
    t->year    = dt.date.year;
    t->weekday = dt.date.weekDay;
    return true;
}

bool rtc_set_time(const RtcTime* t) {
    if (!M5.Rtc.isEnabled()) return false;
    m5::rtc_datetime_t dt;
    dt.time.seconds = t->second;
    dt.time.minutes = t->minute;
    dt.time.hours   = t->hour;
    dt.date.date    = t->day;
    dt.date.month   = t->month;
    dt.date.year    = t->year;
    dt.date.weekDay = t->weekday;
    M5.Rtc.setDateTime(dt);
    return true;
}

bool rtc_sync_from_ntp(const char* ntpServer, long gmtOffset) {
    if (WiFi.status() != WL_CONNECTED) return false;
    configTime(gmtOffset, 0, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 5000)) return false;
    RtcTime t;
    t.second  = timeinfo.tm_sec;
    t.minute  = timeinfo.tm_min;
    t.hour    = timeinfo.tm_hour;
    t.day     = timeinfo.tm_mday;
    t.month   = timeinfo.tm_mon + 1;
    t.year    = timeinfo.tm_year + 1900;
    t.weekday = timeinfo.tm_wday;
    return rtc_set_time(&t);
}

/* ══════════════════════════════════════════════════════════
 *  IMU (MPU6886 via M5Unified)
 * ══════════════════════════════════════════════════════════ */
bool imu_read(ImuData* data) {
    if (!M5.Imu.isEnabled()) return false;
    auto imuData = M5.Imu.getImuData();
    data->accX = imuData.accel.x;
    data->accY = imuData.accel.y;
    data->accZ = imuData.accel.z;
    data->gyrX = imuData.gyro.x;
    data->gyrY = imuData.gyro.y;
    data->gyrZ = imuData.gyro.z;
    return true;
}

bool imu_detect_wrist_raise() {
    ImuData d;
    if (!imu_read(&d)) return false;
    float magnitude = sqrtf(d.accX * d.accX + d.accY * d.accY + d.accZ * d.accZ);
    return (magnitude > IMU_WAKE_THRESHOLD + 1.0f);
}

/* ══════════════════════════════════════════════════════════
 *  BUZZER
 * ══════════════════════════════════════════════════════════ */
void buzzer_notify() {
    tone(PIN_BUZZER, BUZZER_NOTIFY_FREQ, BUZZER_NOTIFY_MS);
}

void buzzer_error() {
    tone(PIN_BUZZER, 800, 100);
    delay(150);
    tone(PIN_BUZZER, 600, 100);
}

void buzzer_success() {
    tone(PIN_BUZZER, 1200, 80);
    delay(100);
    tone(PIN_BUZZER, 1600, 80);
}

/* ══════════════════════════════════════════════════════════
 *  LED
 * ══════════════════════════════════════════════════════════ */
void led_set(bool on) {
    digitalWrite(PIN_LED, on ? LOW : HIGH);  /* Active LOW */
}

void led_blink(uint8_t times, uint16_t intervalMs) {
    /* G19 is shared with IR RMT. Safe to call between IR transmissions. */
    for (uint8_t i = 0; i < times; i++) {
        led_set(true);
        delay(intervalMs / 2);
        led_set(false);
        if (i < times - 1) delay(intervalMs / 2);
    }
}

/* ══════════════════════════════════════════════════════════
 *  POWER (via AXP2101 through M5Unified)
 * ══════════════════════════════════════════════════════════ */
void power_off() {
    m5_set_backlight(0);
    delay(200);
    M5.Power.powerOff();
}
