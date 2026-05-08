#pragma once

#include <Arduino_GFX.h>
#include <databus/Arduino_ESP32QSPI.h>
#include <display/Arduino_CO5300.h>
// LVGL requires arduino-esp32 3.x — deferred until platform migration
// #include <lvgl.h>

// Physical display resolution
static constexpr uint16_t DISPLAY_WIDTH  = 466;
static constexpr uint16_t DISPLAY_HEIGHT = 466;

// LVGL draw buffer: 2x quarter-screen in DMA-capable internal SRAM
class Display {
public:
    static Display& instance();

    bool begin();

    // Raw GFX access for direct drawing (eyes animation)
    Arduino_CO5300* gfx() { return _gfx; }

    // Fill entire screen with one color (RGB565)
    void fillScreen(uint16_t color);

    // Set backlight brightness 0–255
    void setBrightness(uint8_t brightness);

private:
    Display() = default;

    Arduino_DataBus* _bus = nullptr;
    Arduino_CO5300*  _gfx = nullptr;
};
