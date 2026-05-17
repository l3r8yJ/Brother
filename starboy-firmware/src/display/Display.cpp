#include "Display.h"
#include <Arduino.h>
#include <lvgl.h> 

static uint8_t s_lvglBuf1[DISPLAY_WIDTH * 10 * 2];
static uint8_t s_lvglBuf2[DISPLAY_WIDTH * 10 * 2];

void Display::lvglInit() {
    lv_init();
    lv_display_t* disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, lvglFlushCb);
    lv_display_set_buffers(disp, s_lvglBuf1, s_lvglBuf2,
                           sizeof(s_lvglBuf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    Serial.println("[Display] LVGL OK");
}

void Display::lvglFlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    Display& d = Display::instance();
    xSemaphoreTake(d._busMutex, portMAX_DELAY);
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    d._gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)pxMap, w, h);
    lv_display_flush_ready(disp);
    xSemaphoreGive(d._busMutex);
}

void Display::drawBitmap(int32_t x, int32_t y, uint16_t* data, int32_t w, int32_t h) {
    _gfx->draw16bitRGBBitmap(x, y, data, w, h);
}

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
        9, 0, 0, 0
    );

    _busMutex = xSemaphoreCreateMutex();

    _canvas = new Arduino_Canvas(DISPLAY_WIDTH, DISPLAY_HEIGHT, _gfx);
    if (!_canvas->begin()) {
        Serial.println("[Display] begin() failed");
        return false;
    }

    // Clear full controller RAM (480px wide) to kill edge artifacts
    _gfx->fillScreen(RGB565_BLACK);
    _canvas->fillScreen(RGB565_BLACK);
    _canvas->flush();
    _gfx->setBrightness(100);
    Serial.printf("[Display] canvas buf @ %p\n", _canvas->getFramebuffer());
    Serial.println("[Display] CO5300 OK");
    return true;
}

void Display::flush() {
    xSemaphoreTake(_busMutex, portMAX_DELAY);
    _canvas->flush();
    xSemaphoreGive(_busMutex);
}

void Display::fillScreen(uint16_t color) {
    xSemaphoreTake(_busMutex, portMAX_DELAY);
    _canvas->fillScreen(color);
    _canvas->flush();
    xSemaphoreGive(_busMutex);
}


void Display::toggleDisplay() {
    _displayOn = !_displayOn;
    _gfx->setBrightness(_displayOn ? 200 : 0);
    Serial.printf("[Display] %s\n", _displayOn ? "ON" : "OFF");
}

// Также обнови setBrightness, чтобы не включать дисплей случайно:
void Display::setBrightness(uint8_t brightness) {
    if (_displayOn) {
        _gfx->setBrightness(brightness);
    }
}