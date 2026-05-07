/**
 * Huonyx Watch – Display Driver
 * CO5300 AMOLED 412x412 via QSPI using Arduino_GFX
 */
#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "hw_config.h"

/* Forward declare the global gfx pointer */
extern Arduino_GFX *gfx;

/* Initialize the display hardware */
bool display_init();

/* Set display brightness (0-255) */
void display_set_brightness(uint8_t level);

/* Turn display on/off */
void display_sleep(bool sleep);

#endif /* DISPLAY_DRIVER_H */
