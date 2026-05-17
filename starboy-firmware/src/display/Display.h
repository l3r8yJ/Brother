#pragma once

#include <Arduino_GFX.h>
#include <databus/Arduino_ESP32QSPI.h>
#include <display/Arduino_CO5300.h>
#include <canvas/Arduino_Canvas.h>
#include <lvgl.h>
#include <freertos/semphr.h>

static constexpr uint16_t DISPLAY_WIDTH  = 466;
static constexpr uint16_t DISPLAY_HEIGHT = 466;

class Display {
public:
    static Display& instance();

    bool begin();
    void lvglInit();

    Arduino_Canvas* gfx() { return _canvas; }

    void flush();
    void fillScreen(uint16_t color);
    void setBrightness(uint8_t brightness);
    void drawBitmap(int32_t x, int32_t y, uint16_t* data, int32_t w, int32_t h);

    void toggleDisplay();
    bool isDisplayOn() const { return _displayOn; }

private:
    Display() = default;

    static void lvglFlushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);

    Arduino_DataBus*  _bus      = nullptr;
    Arduino_CO5300*   _gfx      = nullptr;
    Arduino_Canvas*   _canvas   = nullptr;
    bool              _displayOn = true;
    SemaphoreHandle_t _busMutex  = nullptr;
};
