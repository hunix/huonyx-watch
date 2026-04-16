/*
 * HUONYX Raw Screen Test - Bypasses TFT_eSPI completely
 * Uses raw SPI to talk to GC9A01 display directly
 * This tests if the SPI hardware and pins actually work
 */

#include <SPI.h>

// ESP32-2424S012 pin mapping
#define TFT_MOSI  7
#define TFT_SCLK  6
#define TFT_CS    10
#define TFT_DC    2
#define TFT_RST   -1   // connected to EN/chip reset
#define TFT_BL    3

#define SCREEN_W  240
#define SCREEN_H  240

SPIClass* vspi = NULL;

void tftCommand(uint8_t cmd) {
    digitalWrite(TFT_DC, LOW);   // command mode
    digitalWrite(TFT_CS, LOW);
    vspi->transfer(cmd);
    digitalWrite(TFT_CS, HIGH);
}

void tftData(uint8_t data) {
    digitalWrite(TFT_DC, HIGH);  // data mode
    digitalWrite(TFT_CS, LOW);
    vspi->transfer(data);
    digitalWrite(TFT_CS, HIGH);
}

void tftCommandData(uint8_t cmd, const uint8_t* data, uint8_t len) {
    tftCommand(cmd);
    for (uint8_t i = 0; i < len; i++) {
        tftData(data[i]);
    }
}

void gc9a01_init() {
    Serial.println("  Sending GC9A01 init sequence...");
    
    // Software reset
    tftCommand(0xFE);  // Inter Register Enable1
    tftCommand(0xEF);  // Inter Register Enable2
    
    // Various GC9A01 magic init commands
    uint8_t d;
    
    d = 0x14; tftCommandData(0xEB, &d, 1);
    d = 0x14; tftCommandData(0x84, &d, 1);
    d = 0x14; tftCommandData(0x85, &d, 1);
    d = 0x14; tftCommandData(0x86, &d, 1);
    d = 0x14; tftCommandData(0x87, &d, 1);
    d = 0x14; tftCommandData(0x88, &d, 1);
    d = 0x0A; tftCommandData(0x89, &d, 1);
    d = 0x21; tftCommandData(0x8A, &d, 1);
    d = 0x00; tftCommandData(0x8B, &d, 1);
    d = 0x80; tftCommandData(0x8C, &d, 1);
    d = 0x01; tftCommandData(0x8D, &d, 1);
    d = 0x01; tftCommandData(0x8E, &d, 1);
    d = 0xFF; tftCommandData(0x8F, &d, 1);
    
    // Pixel format: 16bit/pixel (RGB565)
    uint8_t pf = 0x05;
    tftCommandData(0x3A, &pf, 1);
    
    // Memory Access Control
    uint8_t mac = 0x08;   // RGB order
    tftCommandData(0x36, &mac, 1);
    
    // Column address set (0-239)
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};
    tftCommandData(0x2A, caset, 4);
    
    // Row address set (0-239)
    uint8_t raset[] = {0x00, 0x00, 0x00, 0xEF};
    tftCommandData(0x2B, raset, 4);
    
    // Display inversion on (GC9A01 usually needs this)
    tftCommand(0x21);
    
    // Sleep out
    tftCommand(0x11);
    delay(120);
    
    // Display ON
    tftCommand(0x29);
    delay(20);
    
    Serial.println("  GC9A01 init sequence complete!");
}

void fillScreen(uint16_t color) {
    // Set column address
    uint8_t caset[] = {0x00, 0x00, 0x00, 0xEF};
    tftCommandData(0x2A, caset, 4);
    
    // Set row address
    uint8_t raset[] = {0x00, 0x00, 0x00, 0xEF};
    tftCommandData(0x2B, raset, 4);
    
    // Memory write
    tftCommand(0x2C);
    
    // Send pixels
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_CS, LOW);
    
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    
    for (uint32_t i = 0; i < (uint32_t)SCREEN_W * SCREEN_H; i++) {
        vspi->transfer(hi);
        vspi->transfer(lo);
    }
    
    digitalWrite(TFT_CS, HIGH);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== HUONYX RAW SCREEN TEST ===");
    Serial.println("(No TFT_eSPI - pure SPI)");
    
    // Step 1: GPIO setup
    Serial.println("Step 1: Setting up GPIOs...");
    pinMode(TFT_CS, OUTPUT);
    pinMode(TFT_DC, OUTPUT);
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_CS, HIGH);  // deselect
    digitalWrite(TFT_DC, HIGH);
    digitalWrite(TFT_BL, LOW);   // backlight off initially
    Serial.println("  GPIOs configured");
    
    // Step 2: SPI init
    Serial.println("Step 2: SPI.begin()...");
    vspi = new SPIClass(FSPI);
    vspi->begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
    vspi->setFrequency(40000000);
    Serial.println("  SPI initialized at 40MHz");
    
    // Step 3: GC9A01 init
    Serial.println("Step 3: GC9A01 display init...");
    gc9a01_init();
    
    // Step 4: Backlight ON
    Serial.println("Step 4: Backlight ON...");
    analogWrite(TFT_BL, 200);
    Serial.println("  Backlight should be ON");
    
    // Step 5: Fill with cyan
    Serial.println("Step 5: Filling screen with CYAN...");
    fillScreen(0x07FF);  // Cyan in RGB565
    Serial.println("  Screen should be CYAN!");
    
    Serial.println("\n=== RAW TEST COMPLETE ===");
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.println("If screen is CYAN: hardware works, TFT_eSPI is broken");
    Serial.println("If screen is BLACK: SPI pins or GC9A01 init is wrong");
}

void loop() {
    static uint32_t lastChange = 0;
    static int idx = 0;
    
    // Cycle colors every 3 seconds
    uint16_t colors[] = {
        0xF800,  // Red
        0x07E0,  // Green
        0x001F,  // Blue
        0x07FF,  // Cyan
        0xFFE0,  // Yellow
        0xFFFF   // White
    };
    const char* names[] = {"RED", "GREEN", "BLUE", "CYAN", "YELLOW", "WHITE"};
    
    if (millis() - lastChange > 3000) {
        lastChange = millis();
        Serial.printf("Filling: %s\n", names[idx]);
        fillScreen(colors[idx]);
        idx = (idx + 1) % 6;
    }
    delay(100);
}
