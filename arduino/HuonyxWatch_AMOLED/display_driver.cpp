/**
 * Huonyx Watch – Display Driver Implementation
 * CO5300 AMOLED 412x412 via QSPI using Arduino_GFX
 */
#include "display_driver.h"

/* QSPI bus for CO5300 */
static Arduino_DataBus *bus = nullptr;
Arduino_GFX *gfx = nullptr;

bool display_init() {
    bus = new Arduino_ESP32QSPI(
        LCD_CS,     /* CS */
        LCD_SCLK,   /* SCK */
        LCD_SDIO0,  /* D0 */
        LCD_SDIO1,  /* D1 */
        LCD_SDIO2,  /* D2 */
        LCD_SDIO3   /* D3 */
    );

    gfx = new Arduino_CO5300(
        bus,
        LCD_RESET,      /* RST */
        0,              /* rotation */
        LCD_WIDTH,
        LCD_HEIGHT,
        LCD_COL_OFFSET, /* col_offset1 */
        0,              /* row_offset1 */
        0,              /* col_offset2 */
        0               /* row_offset2 */
    );

    if (!gfx->begin()) {
        return false;
    }

    gfx->fillScreen(BLACK);
    display_set_brightness(LCD_BRIGHTNESS_DEF);
    return true;
}

void display_set_brightness(uint8_t level) {
    /* CO5300 brightness is controlled via the AXP2101 PMU DLDO
     * or via display command. For now we use a simple approach. */
    /* The Arduino_CO5300 driver handles brightness internally */
    (void)level;
}

void display_sleep(bool sleep) {
    if (sleep) {
        gfx->displayOff();
    } else {
        gfx->displayOn();
    }
}
