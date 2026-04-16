#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\touch_driver.h"
/**
 * CST816D Touch Driver for ESP32-2424S012
 * Huonyx AI Smartwatch
 */

#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <Wire.h>

/* Gesture IDs from CST816 */
enum TouchGesture : uint8_t {
    GESTURE_NONE       = 0x00,
    GESTURE_SWIPE_UP   = 0x01,
    GESTURE_SWIPE_DOWN = 0x02,
    GESTURE_SWIPE_LEFT = 0x03,
    GESTURE_SWIPE_RIGHT= 0x04,
    GESTURE_SINGLE_TAP = 0x05,
    GESTURE_DOUBLE_TAP = 0x0B,
    GESTURE_LONG_PRESS = 0x0C,
};

struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool     pressed;
    TouchGesture gesture;
};

class CST816D_Driver {
public:
    CST816D_Driver(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin, uint8_t addr = 0x15);
    bool begin();
    bool read(TouchPoint &point);
    TouchGesture getGesture();

private:
    uint8_t _sda, _scl, _intPin, _rstPin, _addr;
    bool    _initialized;
    uint8_t readRegister(uint8_t reg);
    void    writeRegister(uint8_t reg, uint8_t val);
};

#endif /* TOUCH_DRIVER_H */
