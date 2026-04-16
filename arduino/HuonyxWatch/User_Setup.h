/**
 * TFT_eSPI User Setup for ESP32-2424S012
 * GC9A01 240x240 Round Display + ESP32-C3-MINI-1U
 *
 * IMPORTANT: Copy this file to your TFT_eSPI library folder:
 *   Arduino/libraries/TFT_eSPI/User_Setup.h
 *   (replacing the existing one)
 *
 * Or if using the PowerShell flash script, this is done automatically.
 */

#define USER_SETUP_LOADED

#define GC9A01_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

/* ESP32-C3 SPI Pins */
#define TFT_MOSI   7
#define TFT_SCLK   6
#define TFT_CS     10
#define TFT_DC      2
#define TFT_RST    -1
#define TFT_BL      3
#define TFT_MISO   -1

/*
 * CRITICAL: ESP32-C3 only has SPI2 (FSPI). The default VSPI (SPI3)
 * does NOT exist on C3 and causes a Store Access Fault crash in
 * spi.beginTransaction(). Force FSPI to prevent this.
 */
#define USE_FSPI_PORT

/* SPI Frequency */
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

/* Fonts - LVGL handles all text, but keep GLCD for TFT_eSPI boot splash */
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
