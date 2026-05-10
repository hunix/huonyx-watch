/**
 * Huonyx Watch – PCF85063 RTC Driver
 * I2C addr 0x51. Provides time persistence across reboots without WiFi.
 */
#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include "hw_config.h"

class RTCDriver {
public:
    RTCDriver();

    /* Probe + wake oscillator. Safe to call after Wire.begin(). */
    bool begin();

    /* Read current time. Returns false if oscillator stopped (OS bit set). */
    bool readTime(struct tm* out);

    /* Write the chip with the given local time. Clears OS bit. */
    bool writeTime(const struct tm* in);

    /* Push to system clock (settimeofday). Returns false if RTC unset. */
    bool seedSystemClock();

    bool isPresent() const { return _present; }
    bool isValid()   const { return _valid;   }

private:
    bool _present;
    bool _valid;

    bool i2cWrite(uint8_t reg, const uint8_t* data, size_t len);
    bool i2cRead(uint8_t reg, uint8_t* data, size_t len);
    static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
    static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }
};

#endif /* RTC_DRIVER_H */
