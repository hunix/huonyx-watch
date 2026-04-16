#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\HuonyxWatch\\User_Setup.h"
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

/* SPI Frequency */
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

/* Fonts */
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
#define SMOOTH_FONT
