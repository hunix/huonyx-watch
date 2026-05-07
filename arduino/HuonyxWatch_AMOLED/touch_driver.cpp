/**
 * Huonyx Watch – Touch Driver Implementation
 * FT3168 capacitive touch via I2C
 */
#include "touch_driver.h"

TouchDriver::TouchDriver()
    : _wasPressed(false)
    , _lastUpdateMs(0)
{
    memset(&_current, 0, sizeof(_current));
    memset(&_previous, 0, sizeof(_previous));
    memset(&_downPoint, 0, sizeof(_downPoint));
}

bool TouchDriver::begin() {
    /* Reset touch controller */
    pinMode(TOUCH_RST, OUTPUT);
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);

    /* Configure interrupt pin */
    pinMode(TOUCH_INT, INPUT);

    /* Initialize I2C (shared bus with PMU/IMU) */
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(400000);

    /* Verify touch controller is present */
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("[TOUCH] FT3168 not found!");
        return false;
    }

    Serial.println("[TOUCH] FT3168 initialized");
    return true;
}

void TouchDriver::update() {
    _previous = _current;
    int16_t x, y;

    if (readTouch(x, y)) {
        _current.x = x;
        _current.y = y;
        if (!_current.pressed) {
            /* New touch down */
            _current.pressed = true;
            _current.pressStartMs = millis();
            _downPoint.x = x;
            _downPoint.y = y;
            _downPoint.pressStartMs = millis();
        }
    } else {
        _current.pressed = false;
    }

    _wasPressed = _previous.pressed;
    _lastUpdateMs = millis();
}

TouchGesture TouchDriver::getGesture() {
    TouchGesture g;
    g.event = TOUCH_NONE;
    g.x = _current.x;
    g.y = _current.y;
    g.dx = 0;
    g.dy = 0;

    /* Touch just released */
    if (_wasPressed && !_current.pressed) {
        int16_t dx = _previous.x - _downPoint.x;
        int16_t dy = _previous.y - _downPoint.y;
        g.dx = dx;
        g.dy = dy;
        uint32_t duration = millis() - _downPoint.pressStartMs;

        int16_t absDx = abs(dx);
        int16_t absDy = abs(dy);

        if (absDx > SWIPE_THRESHOLD || absDy > SWIPE_THRESHOLD) {
            /* Swipe gesture */
            if (absDx > absDy) {
                g.event = (dx > 0) ? TOUCH_SWIPE_RIGHT : TOUCH_SWIPE_LEFT;
            } else {
                g.event = (dy > 0) ? TOUCH_SWIPE_DOWN : TOUCH_SWIPE_UP;
            }
        } else if (duration >= LONG_PRESS_MS) {
            g.event = TOUCH_LONG_PRESS;
        } else {
            g.event = TOUCH_TAP;
        }
        g.x = _downPoint.x;
        g.y = _downPoint.y;
    } else if (_current.pressed && !_wasPressed) {
        g.event = TOUCH_DOWN;
    } else if (_current.pressed && _wasPressed) {
        g.event = TOUCH_MOVE;
    }

    return g;
}

bool TouchDriver::readTouch(int16_t &x, int16_t &y) {
    /* Read FT3168 touch data registers */
    uint8_t buf[6];

    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x02); /* Number of touch points register */
    if (Wire.endTransmission(false) != 0) return false;

    if (Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)5) != 5) return false;

    uint8_t numPoints = Wire.read() & 0x0F;
    if (numPoints == 0 || numPoints > 2) return false;

    /* Read touch point 1 */
    uint8_t xH = Wire.read();
    uint8_t xL = Wire.read();
    uint8_t yH = Wire.read();
    uint8_t yL = Wire.read();

    x = (int16_t)(((xH & 0x0F) << 8) | xL);
    y = (int16_t)(((yH & 0x0F) << 8) | yL);

    /* Clamp to display bounds */
    if (x < 0) x = 0;
    if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;

    return true;
}
