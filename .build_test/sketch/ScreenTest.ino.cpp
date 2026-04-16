#include <Arduino.h>
#line 1 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\ScreenTest\\ScreenTest.ino"
/*
 * HUONYX Screen Test - TFT_eSPI with FSPI fix
 */

#include <SPI.h>
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

#line 10 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\ScreenTest\\ScreenTest.ino"
void setup();
#line 37 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\ScreenTest\\ScreenTest.ino"
void loop();
#line 10 "C:\\Users\\H\\source\\repos\\huonyx-watch\\arduino\\ScreenTest\\ScreenTest.ino"
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TFT_eSPI SCREEN TEST (FSPI fix) ===");
    
    Serial.println("Step 1: Backlight ON...");
    pinMode(3, OUTPUT);
    analogWrite(3, 200);
    
    Serial.println("Step 2: tft.init()...");
    tft.init();
    Serial.println("  OK - no crash!");
    
    Serial.println("Step 3: Fill CYAN...");
    tft.setRotation(0);
    tft.fillScreen(TFT_CYAN);
    
    tft.setTextColor(TFT_BLACK, TFT_CYAN);
    tft.setTextSize(2);
    tft.setCursor(60, 100);
    tft.println("HUONYX");
    tft.setCursor(50, 130);
    tft.println("IT WORKS!");
    
    Serial.println("=== SUCCESS ===");
}

void loop() {
    static uint32_t t = 0;
    static int i = 0;
    uint16_t c[] = {TFT_RED, TFT_GREEN, TFT_BLUE, TFT_CYAN, TFT_YELLOW, TFT_WHITE};
    const char* n[] = {"RED", "GREEN", "BLUE", "CYAN", "YELLOW", "WHITE"};
    if (millis() - t > 2000) {
        t = millis();
        i = (i + 1) % 6;
        tft.fillScreen(c[i]);
        tft.setTextColor(TFT_BLACK, c[i]);
        tft.setTextSize(2);
        tft.setCursor(60, 100);
        tft.println("HUONYX");
        tft.setCursor(70, 130);
        tft.println(n[i]);
        Serial.printf("Color: %s\n", n[i]);
    }
    delay(100);
}

