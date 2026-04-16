#line 1 "C:\\Users\\HK\\sources\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\gc9a01_driver.h"
/**
 * GC9A01 Raw SPI Display Driver for ESP32-C3
 * Replaces TFT_eSPI which crashes on ESP32-C3 SPI init
 * 
 * Provides the minimal API needed by LVGL flush callback:
 *   - init(), setWindow(), pushPixels(), fillRect()
 */

#ifndef GC9A01_DRIVER_H
#define GC9A01_DRIVER_H

#include <Arduino.h>
#include <SPI.h>

/* ESP32-2424S012 pin mapping */
#define GC9A01_MOSI   7
#define GC9A01_SCLK   6
#define GC9A01_CS     10
#define GC9A01_DC     2
#define GC9A01_RST    -1
#define GC9A01_BL     3
#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240

class GC9A01 {
public:
    void init();
    void setBacklight(uint8_t brightness);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void pushPixels(const uint16_t* data, uint32_t len);
    void pushPixelsRaw(const uint8_t* data, uint32_t bytes);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillScreen(uint16_t color);
    
    uint16_t width()  { return GC9A01_WIDTH; }
    uint16_t height() { return GC9A01_HEIGHT; }

private:
    SPIClass* _spi = nullptr;
    
    void writeCommand(uint8_t cmd);
    void writeData(uint8_t data);
    void writeData16(uint16_t data);
    void writeCmdData(uint8_t cmd, const uint8_t* data, uint8_t len);
    
    inline void csLow()  { digitalWrite(GC9A01_CS, LOW); }
    inline void csHigh() { digitalWrite(GC9A01_CS, HIGH); }
    inline void dcCmd()  { digitalWrite(GC9A01_DC, LOW); }
    inline void dcData() { digitalWrite(GC9A01_DC, HIGH); }
};

#endif /* GC9A01_DRIVER_H */
