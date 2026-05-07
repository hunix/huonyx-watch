/**
 * Huonyx Watch – Touch Driver
 * FT3168 capacitive touch via I2C
 */
#ifndef TOUCH_DRIVER_H
#define TOUCH_DRIVER_H

#include <Arduino.h>
#include <Wire.h>
#include "hw_config.h"

/* Touch event types */
enum TouchEvent : uint8_t {
    TOUCH_NONE = 0,
    TOUCH_DOWN,
    TOUCH_MOVE,
    TOUCH_UP,
    TOUCH_TAP,
    TOUCH_LONG_PRESS,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT
};

/* Touch point data */
struct TouchPoint {
    int16_t  x;
    int16_t  y;
    bool     pressed;
    uint32_t pressStartMs;
};

/* Gesture result */
struct TouchGesture {
    TouchEvent event;
    int16_t    x;
    int16_t    y;
    int16_t    dx;
    int16_t    dy;
};

class TouchDriver {
public:
    TouchDriver();
    bool begin();
    void update();

    /* Get current touch state */
    bool isTouched() const { return _current.pressed; }
    int16_t getX() const { return _current.x; }
    int16_t getY() const { return _current.y; }

    /* Get gesture (call after update()) */
    TouchGesture getGesture();

private:
    TouchPoint _current;
    TouchPoint _previous;
    TouchPoint _downPoint;
    bool       _wasPressed;
    uint32_t   _lastUpdateMs;

    bool readTouch(int16_t &x, int16_t &y);
    static constexpr int16_t SWIPE_THRESHOLD = 50;
    static constexpr uint32_t LONG_PRESS_MS = 600;
};

#endif /* TOUCH_DRIVER_H */
