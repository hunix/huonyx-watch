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

    /* GC9A01 initialization sequence */
    writeCommand(0xFE);  // Inter Register Enable1
    writeCommand(0xEF);  // Inter Register Enable2

    /* Power/voltage settings */
    uint8_t d;
    d = 0x14; writeCmdData(0xEB, &d, 1);
    d = 0x14; writeCmdData(0x84, &d, 1);
    d = 0x14; writeCmdData(0x85, &d, 1);
    d = 0x14; writeCmdData(0x86, &d, 1);
    d = 0x14; writeCmdData(0x87, &d, 1);
    d = 0x14; writeCmdData(0x88, &d, 1);
    d = 0x0A; writeCmdData(0x89, &d, 1);
    d = 0x21; writeCmdData(0x8A, &d, 1);
    d = 0x00; writeCmdData(0x8B, &d, 1);
    d = 0x80; writeCmdData(0x8C, &d, 1);
    d = 0x01; writeCmdData(0x8D, &d, 1);
    d = 0x01; writeCmdData(0x8E, &d, 1);
    d = 0xFF; writeCmdData(0x8F, &d, 1);

    /* Functional settings */
    uint8_t b6[] = {0x00, 0x20};
    writeCmdData(0xB6, b6, 2);  // Display Function Control

    /* Pixel format: 16-bit RGB565 */
    d = 0x55; writeCmdData(0x3A, &d, 1);

    /* Memory Access Control: MY=0, MX=0, MV=0, ML=0, BGR=1 */
    d = 0x08; writeCmdData(0x36, &d, 1);

    /* Column address set (0-239) */
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};
    writeCmdData(0x2A, caset, 4);

    /* Row address set (0-239) */
    uint8_t raset[] = {0x00, 0x00, 0x00, 0xEF};
    writeCmdData(0x2B, raset, 4);

    /* Gamma settings */
    uint8_t gamma1[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
    writeCmdData(0xE0, gamma1, 6);
    uint8_t gamma2[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
    writeCmdData(0xE1, gamma2, 6);
    uint8_t gamma3[] = {0x45, 0x09, 0x08, 0x08, 0x26, 0x2A};
    writeCmdData(0xE2, gamma3, 6);
    uint8_t gamma4[] = {0x43, 0x70, 0x72, 0x36, 0x37, 0x6F};
    writeCmdData(0xE3, gamma4, 6);

    /* More power settings */
    uint8_t ed[] = {0x1B, 0x0B};
    writeCmdData(0xED, ed, 2);
    uint8_t ae[] = {0x77};
    writeCmdData(0xAE, ae, 1);
    d = 0x01; writeCmdData(0xCD, &d, 1);

    /* Frame rate */
    uint8_t fr[] = {0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
                    0x08, 0x08, 0x08, 0x08, 0x08, 0x08};
    writeCmdData(0x70, fr, 14);

    /* More display settings */
    d = 0x21; writeCmdData(0xE8, &d, 1);
    uint8_t e9[] = {0x13, 0x11};
    writeCmdData(0xE9, e9, 2);
    uint8_t ec[] = {0x13, 0x02, 0x12};
    writeCmdData(0xEC, ec, 3);

    /* Tearing effect */
    d = 0x00; writeCmdData(0x35, &d, 1);
    
    /* Display inversion ON (GC9A01 needs this for correct colors) */
    writeCommand(0x21);

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
    
    /* Use SPI transfer for pixel data - swap bytes for RGB565 big-endian */
    for (uint32_t i = 0; i < len; i++) {
        _spi->transfer16(data[i]);
    }
    
    csHigh();
}

void GC9A01::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GC9A01_WIDTH)  w = GC9A01_WIDTH - x;
    if (y + h > GC9A01_HEIGHT) h = GC9A01_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    setWindow(x, y, x + w - 1, y + h - 1);

    dcData();
    csLow();
    uint32_t pixels = (uint32_t)w * h;
    for (uint32_t i = 0; i < pixels; i++) {
        _spi->transfer16(color);
    }
    csHigh();
}

void GC9A01::fillScreen(uint16_t color) {
    fillRect(0, 0, GC9A01_WIDTH, GC9A01_HEIGHT, color);
}
