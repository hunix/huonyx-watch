/**
 * Huonyx Watch – PCF85063 RTC Driver Implementation
 *
 * Register map (relevant subset):
 *   0x00 Control_1     STOP bit must be 0 for oscillator to run
 *   0x04 Seconds       bit7 = OS (oscillator stopped, time invalid)
 *   0x05 Minutes
 *   0x06 Hours         bit5 in 12h mode is AM/PM; we use 24h mode
 *   0x07 Days          1..31 BCD
 *   0x08 Weekdays      0..6
 *   0x09 Months        1..12 BCD
 *   0x0A Years         0..99 BCD (year - 2000)
 *
 * I2C bus is shared with FT3168 + AXP2101 + QMI8658 — assume Wire.begin()
 * has already been called by touch_driver.
 */
#include "rtc_driver.h"
#include <sys/time.h>

RTCDriver::RTCDriver()
    : _present(false)
    , _valid(false)
{}

bool RTCDriver::begin() {
    /* Probe */
    Wire.beginTransmission(RTC_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("[RTC] PCF85063 not found");
        _present = false;
        return false;
    }
    _present = true;

    /* Make sure STOP bit is cleared so the oscillator runs. */
    uint8_t ctrl1;
    if (i2cRead(0x00, &ctrl1, 1)) {
        if (ctrl1 & 0x20) {
            uint8_t cleared = ctrl1 & ~0x20;
            i2cWrite(0x00, &cleared, 1);
            Serial.println("[RTC] STOP bit cleared");
        }
    }

    /* Check OS bit on seconds register */
    uint8_t sec;
    if (i2cRead(0x04, &sec, 1)) {
        _valid = !(sec & 0x80);
        Serial.printf("[RTC] Present, %s\n", _valid ? "time valid" : "time invalid (OS=1)");
    }

    return true;
}

bool RTCDriver::readTime(struct tm* out) {
    if (!_present || !out) return false;

    uint8_t buf[7];
    if (!i2cRead(0x04, buf, 7)) return false;

    if (buf[0] & 0x80) {
        _valid = false;
        return false;
    }

    out->tm_sec  = bcd2bin(buf[0] & 0x7F);
    out->tm_min  = bcd2bin(buf[1] & 0x7F);
    out->tm_hour = bcd2bin(buf[2] & 0x3F);
    out->tm_mday = bcd2bin(buf[3] & 0x3F);
    out->tm_wday = buf[4] & 0x07;
    out->tm_mon  = bcd2bin(buf[5] & 0x1F) - 1;       /* tm_mon is 0..11 */
    out->tm_year = bcd2bin(buf[6]) + 100;            /* tm_year is years since 1900 */
    out->tm_yday = 0;
    out->tm_isdst = -1;

    _valid = true;
    return true;
}

bool RTCDriver::writeTime(const struct tm* in) {
    if (!_present || !in) return false;

    /* PCF85063 only stores 2-digit year (assumes 20xx). */
    int year = in->tm_year + 1900;
    if (year < 2000 || year > 2099) return false;

    uint8_t buf[7];
    buf[0] = bin2bcd(in->tm_sec)  & 0x7F;     /* clears OS */
    buf[1] = bin2bcd(in->tm_min)  & 0x7F;
    buf[2] = bin2bcd(in->tm_hour) & 0x3F;
    buf[3] = bin2bcd(in->tm_mday) & 0x3F;
    buf[4] = in->tm_wday & 0x07;
    buf[5] = bin2bcd(in->tm_mon + 1) & 0x1F;
    buf[6] = bin2bcd(year - 2000);

    if (!i2cWrite(0x04, buf, 7)) return false;
    _valid = true;
    return true;
}

bool RTCDriver::seedSystemClock() {
    struct tm t;
    if (!readTime(&t)) return false;

    /* The RTC stores local time. Convert to UTC by treating it as local
     * (the TZ env var has been set already) and using mktime. */
    time_t epoch = mktime(&t);
    if (epoch < 0) return false;

    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
    Serial.printf("[RTC] Seeded system clock from RTC (%04d-%02d-%02d %02d:%02d:%02d)\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec);
    return true;
}

bool RTCDriver::i2cWrite(uint8_t reg, const uint8_t* data, size_t len) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(reg);
    Wire.write(data, len);
    return Wire.endTransmission() == 0;
}

bool RTCDriver::i2cRead(uint8_t reg, uint8_t* data, size_t len) {
    Wire.beginTransmission(RTC_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)RTC_I2C_ADDR, (uint8_t)len) != len) return false;
    for (size_t i = 0; i < len; i++) data[i] = Wire.read();
    return true;
}
