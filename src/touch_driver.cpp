/**
 * CST816D Touch Driver Implementation
 * Huonyx AI Smartwatch
 */

#include "touch_driver.h"
#include "hw_config.h"

/* CST816 Register Map */
#define CST816_REG_GESTURE   0x01
#define CST816_REG_FINGER    0x02
#define CST816_REG_XH        0x03
#define CST816_REG_XL        0x04
#define CST816_REG_YH        0x05
#define CST816_REG_YL        0x06
#define CST816_REG_CHIPID    0xA7
#define CST816_REG_SLEEP     0xA5
#define CST816_REG_IRQCTL    0xFA
#define CST816_REG_AUTOSLEEP 0xFE

CST816D_Driver::CST816D_Driver(uint8_t sda, uint8_t scl, uint8_t intPin, uint8_t rstPin, uint8_t addr)
    : _sda(sda), _scl(scl), _intPin(intPin), _rstPin(rstPin), _addr(addr), _initialized(false) {}

bool CST816D_Driver::begin() {
    /* Configure pins */
    pinMode(_intPin, INPUT);
    pinMode(_rstPin, OUTPUT);

    /* Hardware reset */
    digitalWrite(_rstPin, LOW);
    delay(10);
    digitalWrite(_rstPin, HIGH);
    delay(50);

    /* Initialize I2C */
    Wire.begin(_sda, _scl);
    Wire.setClock(400000);

    /* Verify chip presence */
    uint8_t chipId = readRegister(CST816_REG_CHIPID);
    if (chipId == 0 || chipId == 0xFF) {
        return false;
    }

    /* Configure: enable motion interrupt, disable auto-sleep for responsiveness */
    writeRegister(CST816_REG_IRQCTL, 0x60);    /* Motion + touch IRQ */
    writeRegister(CST816_REG_AUTOSLEEP, 0x00);  /* Disable auto-sleep */

    _initialized = true;
    return true;
}

bool CST816D_Driver::read(TouchPoint &point) {
    if (!_initialized) return false;

    Wire.beginTransmission(_addr);
    Wire.write(CST816_REG_GESTURE);
    if (Wire.endTransmission() != 0) return false;

    Wire.requestFrom(_addr, (uint8_t)6);
    if (Wire.available() < 6) return false;

    uint8_t gesture = Wire.read();
    uint8_t fingers = Wire.read();
    uint8_t xh      = Wire.read();
    uint8_t xl      = Wire.read();
    uint8_t yh      = Wire.read();
    uint8_t yl      = Wire.read();

    point.gesture = (TouchGesture)gesture;
    point.pressed = (fingers > 0);
    point.x = ((xh & 0x0F) << 8) | xl;
    point.y = ((yh & 0x0F) << 8) | yl;

    /* Clamp to display bounds */
    if (point.x >= TFT_WIDTH)  point.x = TFT_WIDTH - 1;
    if (point.y >= TFT_HEIGHT) point.y = TFT_HEIGHT - 1;

    return true;
}

TouchGesture CST816D_Driver::getGesture() {
    return (TouchGesture)readRegister(CST816_REG_GESTURE);
}

uint8_t CST816D_Driver::readRegister(uint8_t reg) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    if (Wire.endTransmission() != 0) return 0;
    Wire.requestFrom(_addr, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0;
}

void CST816D_Driver::writeRegister(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(_addr);
    Wire.write(reg);
    Wire.write(val);
    Wire.endTransmission();
}
