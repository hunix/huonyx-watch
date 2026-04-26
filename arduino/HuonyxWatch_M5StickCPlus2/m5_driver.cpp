/**
 * m5_driver.cpp
 * M5StickC Plus2 Hardware Driver Implementation
 * Huonyx AI Smartwatch — M5StickC Plus2 Port
 */
#include "build_config.h"
#include "m5_driver.h"
#include <lvgl.h>
#include <WiFi.h>
#include <time.h>

/* ── Global display instance ──────────────────────────── */
M5GFX display;

/* ── Button state tracking ────────────────────────────── */
static bool     _btnAState     = false;
static bool     _btnBState     = false;
static uint32_t _btnAPressMs   = 0;
static uint32_t _btnBPressMs   = 0;
static bool     _btnAHandled   = false;
static bool     _btnBHandled   = false;

/* ── IMU (MPU6886) register definitions ───────────────── */
#define MPU6886_WHOAMI          0x75
#define MPU6886_PWR_MGMT_1      0x6B
#define MPU6886_ACCEL_CONFIG    0x1C
#define MPU6886_GYRO_CONFIG     0x1B
#define MPU6886_ACCEL_XOUT_H    0x3B
#define MPU6886_GYRO_XOUT_H     0x43

/* ── RTC (BM8563) register definitions ───────────────── */
#define BM8563_SECONDS          0x02
#define BM8563_MINUTES          0x03
#define BM8563_HOURS            0x04
#define BM8563_DAY              0x05
#define BM8563_WEEKDAY          0x06
#define BM8563_MONTH            0x07
#define BM8563_YEAR             0x08

/* ── BCD helpers ──────────────────────────────────────── */
static uint8_t bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/* ── I2C helpers ──────────────────────────────────────── */
static bool i2c_write_byte(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool i2c_read_bytes(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    Wire.requestFrom((uint8_t)addr, len);
    for (uint8_t i = 0; i < len; i++) {
        if (!Wire.available()) return false;
        buf[i] = Wire.read();
    }
    return true;
}

/* ══════════════════════════════════════════════════════════
 *  INITIALIZATION
 * ══════════════════════════════════════════════════════════ */
void m5_driver_init() {
    /* ── Power Hold ───────────────────────────────────── */
    power_hold();

    /* ── I2C ──────────────────────────────────────────── */
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.setClock(400000);

    /* ── Display ──────────────────────────────────────── */
    display.init();
    display.setRotation(TFT_ROTATION);
    display.fillScreen(TFT_BLACK);
    m5_set_backlight(128);

    /* ── IMU (MPU6886) init ───────────────────────────── */
    /* Wake up from sleep */
    i2c_write_byte(MPU6886_I2C_ADDR, MPU6886_PWR_MGMT_1, 0x00);
    delay(10);
    /* Set accelerometer range to ±8g */
    i2c_write_byte(MPU6886_I2C_ADDR, MPU6886_ACCEL_CONFIG, 0x10);
    /* Set gyroscope range to ±2000 deg/s */
    i2c_write_byte(MPU6886_I2C_ADDR, MPU6886_GYRO_CONFIG, 0x18);
    Serial.println("[M5] IMU (MPU6886) initialized");

    /* ── RTC (BM8563) init ────────────────────────────── */
    /* Clear stop bit to start oscillator */
    i2c_write_byte(BM8563_I2C_ADDR, 0x00, 0x00);
    i2c_write_byte(BM8563_I2C_ADDR, 0x01, 0x00);
    Serial.println("[M5] RTC (BM8563) initialized");

    /* ── Buttons ──────────────────────────────────────── */
    pinMode(PIN_BTN_A, INPUT_PULLUP);
    pinMode(PIN_BTN_B, INPUT_PULLUP);
    Serial.println("[M5] Buttons initialized");

    /* ── Buzzer ───────────────────────────────────────── */
    pinMode(PIN_BUZZER, OUTPUT);
    digitalWrite(PIN_BUZZER, LOW);

    /* ── LED ──────────────────────────────────────────── */
    /* G19 is shared between the Red LED and the IR emitter RMT peripheral.
     * The IrController::begin() call (in setup()) will configure G19 as an
     * RMT output. We set it HIGH here (LED off) before RMT takes over.
     * After ir_controller.begin(), use led_set() only when no IR TX is active. */
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, HIGH);  /* Active LOW — LED off until RMT takes over */

    Serial.println("[M5] Hardware driver initialized");
}

/* ══════════════════════════════════════════════════════════
 *  BUTTON POLLING
 * ══════════════════════════════════════════════════════════ */
BtnEvent m5_driver_tick() {
    uint32_t now = millis();
    BtnEvent evt = BTN_NONE;

    /* ── Button A ─────────────────────────────────────── */
    bool aPressed = (digitalRead(PIN_BTN_A) == LOW);
    if (aPressed && !_btnAState) {
        /* Press started */
        _btnAState   = true;
        _btnAPressMs = now;
        _btnAHandled = false;
    } else if (!aPressed && _btnAState) {
        /* Released */
        _btnAState = false;
        if (!_btnAHandled) {
            uint32_t held = now - _btnAPressMs;
            evt = (held >= BTN_LONG_PRESS_MS) ? BTN_A_LONG : BTN_A_SHORT;
        }
    } else if (aPressed && _btnAState && !_btnAHandled) {
        /* Still held — check for long press trigger */
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
    display.setBrightness(brightness);
}

void m5_display_flush(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2,
                      const lv_color_t* colormap) {
    display.startWrite();
    display.setAddrWindow(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
    display.pushPixels((uint16_t*)colormap, (x2 - x1 + 1) * (y2 - y1 + 1));
    display.endWrite();
}

/* ══════════════════════════════════════════════════════════
 *  RTC (BM8563)
 * ══════════════════════════════════════════════════════════ */
bool rtc_get_time(RtcTime* t) {
    uint8_t buf[7];
    if (!i2c_read_bytes(BM8563_I2C_ADDR, BM8563_SECONDS, buf, 7)) return false;
    t->second  = bcd2dec(buf[0] & 0x7F);
    t->minute  = bcd2dec(buf[1] & 0x7F);
    t->hour    = bcd2dec(buf[2] & 0x3F);
    t->day     = bcd2dec(buf[3] & 0x3F);
    t->weekday = buf[4] & 0x07;
    t->month   = bcd2dec(buf[5] & 0x1F);
    t->year    = bcd2dec(buf[6]) + 2000;
    return true;
}

bool rtc_set_time(const RtcTime* t) {
    uint8_t buf[7];
    buf[0] = dec2bcd(t->second);
    buf[1] = dec2bcd(t->minute);
    buf[2] = dec2bcd(t->hour);
    buf[3] = dec2bcd(t->day);
    buf[4] = t->weekday & 0x07;
    buf[5] = dec2bcd(t->month);
    buf[6] = dec2bcd(t->year - 2000);
    Wire.beginTransmission(BM8563_I2C_ADDR);
    Wire.write(BM8563_SECONDS);
    for (uint8_t i = 0; i < 7; i++) Wire.write(buf[i]);
    return Wire.endTransmission() == 0;
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
 *  IMU (MPU6886)
 * ══════════════════════════════════════════════════════════ */
bool imu_read(ImuData* data) {
    uint8_t buf[14];
    if (!i2c_read_bytes(MPU6886_I2C_ADDR, MPU6886_ACCEL_XOUT_H, buf, 14)) return false;
    /* Accelerometer: ±8g range → 4096 LSB/g */
    data->accX = (int16_t)((buf[0] << 8) | buf[1])  / 4096.0f;
    data->accY = (int16_t)((buf[2] << 8) | buf[3])  / 4096.0f;
    data->accZ = (int16_t)((buf[4] << 8) | buf[5])  / 4096.0f;
    /* Gyroscope: ±2000 deg/s range → 16.4 LSB/deg/s */
    data->gyrX = (int16_t)((buf[8]  << 8) | buf[9])  / 16.4f;
    data->gyrY = (int16_t)((buf[10] << 8) | buf[11]) / 16.4f;
    data->gyrZ = (int16_t)((buf[12] << 8) | buf[13]) / 16.4f;
    return true;
}

bool imu_detect_wrist_raise() {
    ImuData d;
    if (!imu_read(&d)) return false;
    /* Wrist raise: significant upward Z acceleration change */
    float magnitude = sqrt(d.accX * d.accX + d.accY * d.accY + d.accZ * d.accZ);
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
    /* G19 is shared with IR RMT. IrController::handleCommand() is synchronous,
     * so this is safe to call after it returns. The RMT peripheral idles LOW
     * between transmissions, so a brief digitalWrite here will not corrupt
     * any in-flight IR frame. */
    for (uint8_t i = 0; i < times; i++) {
        led_set(true);
        delay(intervalMs / 2);
        led_set(false);
        if (i < times - 1) delay(intervalMs / 2);
    }
}

/* ══════════════════════════════════════════════════════════
 *  POWER
 * ══════════════════════════════════════════════════════════ */
void power_hold() {
    pinMode(PIN_POWER_HOLD, OUTPUT);
    digitalWrite(PIN_POWER_HOLD, HIGH);
}

void power_off() {
    m5_set_backlight(0);
    delay(200);
    digitalWrite(PIN_POWER_HOLD, LOW);
}
