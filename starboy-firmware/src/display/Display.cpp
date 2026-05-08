#include "Display.h"
#include <Arduino.h>

Display& Display::instance() {
    static Display inst;
    return inst;
}

bool Display::begin() {
    _bus = new Arduino_ESP32QSPI(
        LCD_CS,  // GPIO10
        LCD_SCL, // GPIO11
        LCD_D0,  // GPIO12
        LCD_D1,  // GPIO13
        LCD_D2,  // GPIO14
        LCD_D3   // GPIO15
    );

    // col_offset1=6: CO5300 is 480px wide, panel is 466px
    _gfx = new Arduino_CO5300(
        _bus,
        LCD_RESET,
        0,             // rotation
        DISPLAY_WIDTH,
        DISPLAY_HEIGHT,
        6, 0, 0, 0
    );

    if (!_gfx->begin()) {
        Serial.println("[Display] begin() failed");
        return false;
    }

    _gfx->fillScreen(RGB565_BLACK);
    _gfx->setBrightness(200);
    Serial.println("[Display] CO5300 OK");
    return true;
}

void Display::fillScreen(uint16_t color) {
    _gfx->fillScreen(color);
}

void Display::setBrightness(uint8_t brightness) {
    _gfx->setBrightness(brightness);
}
