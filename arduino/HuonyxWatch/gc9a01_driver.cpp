/**
 * GC9A01 Raw SPI Display Driver for ESP32-C3
 * Direct SPI communication - no TFT_eSPI dependency
 */

#include "gc9a01_driver.h"

void GC9A01::writeCommand(uint8_t cmd) {
    dcCmd();
    csLow();
    _spi->transfer(cmd);
    csHigh();
}

void GC9A01::writeData(uint8_t data) {
    dcData();
    csLow();
    _spi->transfer(data);
    csHigh();
}

void GC9A01::writeData16(uint16_t data) {
    dcData();
    csLow();
    _spi->transfer16(data);
    csHigh();
}

void GC9A01::writeCmdData(uint8_t cmd, const uint8_t* data, uint8_t len) {
    writeCommand(cmd);
    dcData();
    csLow();
    for (uint8_t i = 0; i < len; i++) {
        _spi->transfer(data[i]);
    }
    csHigh();
}

void GC9A01::init() {
    /* GPIO setup */
    pinMode(GC9A01_CS, OUTPUT);
    pinMode(GC9A01_DC, OUTPUT);
    pinMode(GC9A01_BL, OUTPUT);
    digitalWrite(GC9A01_CS, HIGH);
    digitalWrite(GC9A01_DC, HIGH);
    digitalWrite(GC9A01_BL, LOW);

    /* SPI init - FSPI is the only SPI available on ESP32-C3 */
    _spi = new SPIClass(FSPI);
    _spi->begin(GC9A01_SCLK, -1, GC9A01_MOSI, GC9A01_CS);
    _spi->setFrequency(40000000);
    _spi->setDataMode(SPI_MODE0);

    Serial.println("[GC9A01] SPI initialized on FSPI");

    /* ============================================================
     * GC9A01 init sequence — from Arduino_GFX (battle-tested)
     * https://github.com/moononournation/Arduino_GFX
     * ============================================================ */

    uint8_t d;

    writeCommand(0xEF);
    d = 0x14; writeCmdData(0xEB, &d, 1);

    writeCommand(0xFE);   // Inter Register Enable1
    writeCommand(0xEF);   // Inter Register Enable2

    d = 0x14; writeCmdData(0xEB, &d, 1);

    d = 0x40; writeCmdData(0x84, &d, 1);
    d = 0xFF; writeCmdData(0x85, &d, 1);
    d = 0xFF; writeCmdData(0x86, &d, 1);
    d = 0xFF; writeCmdData(0x87, &d, 1);
    d = 0x0A; writeCmdData(0x88, &d, 1);
    d = 0x21; writeCmdData(0x89, &d, 1);
    d = 0x00; writeCmdData(0x8A, &d, 1);
    d = 0x80; writeCmdData(0x8B, &d, 1);
    d = 0x01; writeCmdData(0x8C, &d, 1);
    d = 0x01; writeCmdData(0x8D, &d, 1);
    d = 0xFF; writeCmdData(0x8E, &d, 1);
    d = 0xFF; writeCmdData(0x8F, &d, 1);

    /* Display Function Control */
    uint8_t b6[] = {0x00, 0x20};
    writeCmdData(0xB6, b6, 2);

    /* Pixel format: 16-bit/pixel MCU interface */
    d = 0x05; writeCmdData(0x3A, &d, 1);

    /* Gate scan settings (0x90) */
    uint8_t r90[] = {0x08, 0x08, 0x08, 0x08};
    writeCmdData(0x90, r90, 4);

    d = 0x06; writeCmdData(0xBD, &d, 1);
    d = 0x00; writeCmdData(0xBC, &d, 1);

    /* Panel settings */
    uint8_t rFF[] = {0x60, 0x01, 0x04};
    writeCmdData(0xFF, rFF, 3);

    d = 0x13; writeCmdData(0xC3, &d, 1);
    d = 0x13; writeCmdData(0xC4, &d, 1);
    d = 0x22; writeCmdData(0xC9, &d, 1);
    d = 0x11; writeCmdData(0xBE, &d, 1);

    uint8_t rE1[] = {0x10, 0x0E};
    writeCmdData(0xE1, rE1, 2);

    uint8_t rDF[] = {0x21, 0x0c, 0x02};
    writeCmdData(0xDF, rDF, 3);

    /* Gamma settings */
    uint8_t gamma1[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
    writeCmdData(0xF0, gamma1, 6);
    uint8_t gamma2[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
    writeCmdData(0xF1, gamma2, 6);
    uint8_t gamma3[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
    writeCmdData(0xF2, gamma3, 6);
    uint8_t gamma4[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
    writeCmdData(0xF3, gamma4, 6);

    uint8_t rED[] = {0x1B, 0x0B};
    writeCmdData(0xED, rED, 2);
    d = 0x77; writeCmdData(0xAE, &d, 1);
    d = 0x63; writeCmdData(0xCD, &d, 1);

    /* Frame rate control (0x70) */
    uint8_t r70[] = {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03};
    writeCmdData(0x70, r70, 9);

    d = 0x34; writeCmdData(0xE8, &d, 1);

    /* ==== CRITICAL: Gate scan registers - missing these causes stripes ==== */
    uint8_t r62[] = {0x18, 0x0D, 0x71, 0xED, 0x70, 0x70,
                     0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70};
    writeCmdData(0x62, r62, 12);

    uint8_t r63[] = {0x18, 0x11, 0x71, 0xF1, 0x70, 0x70,
                     0x18, 0x13, 0x71, 0xF3, 0x70, 0x70};
    writeCmdData(0x63, r63, 12);

    uint8_t r64[] = {0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07};
    writeCmdData(0x64, r64, 7);

    uint8_t r66[] = {0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00};
    writeCmdData(0x66, r66, 10);

    uint8_t r67[] = {0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98};
    writeCmdData(0x67, r67, 10);

    uint8_t r74[] = {0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00};
    writeCmdData(0x74, r74, 7);

    uint8_t r98[] = {0x3e, 0x07};
    writeCmdData(0x98, r98, 2);

    /* Tearing effect line ON */
    writeCommand(0x35);

    /* Sleep OUT */
    writeCommand(0x11);
    delay(120);

    /* Display ON */
    writeCommand(0x29);
    delay(20);

    /* Fill screen black */
    fillScreen(0x0000);

    Serial.println("[GC9A01] Display initialized successfully");
}

void GC9A01::setBacklight(uint8_t brightness) {
    analogWrite(GC9A01_BL, brightness);
}

void GC9A01::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset[] = {(uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
                       (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)};
    writeCmdData(0x2A, caset, 4);

    uint8_t raset[] = {(uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
                       (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)};
    writeCmdData(0x2B, raset, 4);

    writeCommand(0x2C);  // Memory write
}

void GC9A01::pushPixels(const uint16_t* data, uint32_t len) {
    dcData();
    csLow();
    for (uint32_t i = 0; i < len; i++) {
        uint16_t px = data[i];
        _spi->transfer(px >> 8);
        _spi->transfer(px & 0xFF);
    }
    csHigh();
}

void GC9A01::pushPixelsRaw(const uint8_t* data, uint32_t bytes) {
    dcData();
    csLow();
    _spi->writeBytes(data, bytes);
    csHigh();
}

void GC9A01::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GC9A01_WIDTH)  w = GC9A01_WIDTH - x;
    if (y + h > GC9A01_HEIGHT) h = GC9A01_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    setWindow(x, y, x + w - 1, y + h - 1);

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    dcData();
    csLow();
    uint32_t pixels = (uint32_t)w * h;
    for (uint32_t i = 0; i < pixels; i++) {
        _spi->transfer(hi);
        _spi->transfer(lo);
    }
    csHigh();
}

void GC9A01::fillScreen(uint16_t color) {
    fillRect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}
