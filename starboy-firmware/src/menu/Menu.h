#pragma once
#include <Arduino.h>
#include <lvgl.h>

class Menu {
public:
    static Menu& instance();

    void begin();
    void open();
    void onShortPress();
    void onLongPress();
    bool isOpen() const { return _open; }

private:
    Menu() = default;

    enum class State { NAVIGATING, EDITING };
    enum class Item  { BRIGHTNESS=0, VOLUME, INFO, REBOOT, SHUTDOWN };

    static constexpr int ITEM_COUNT = 5;

    // Ring positions: dx,dy from center (233,233), radius=160px
    static constexpr int16_t kRingDx[5] = {   0, 152,  94, -94, -152 };
    static constexpr int16_t kRingDy[5] = {-160, -49, 129, 129,  -49 };
    static constexpr const char* kNames[5] = {
        "Яркость", "Громкость", "Инфо", "Перезагр.", "Выкл."
    };

    void buildScreen();
    void destroyScreen();
    void updateUI();
    void updateInfoText();

    bool    _open           = false;
    State   _state          = State::NAVIGATING;
    int     _focus          = 0;
    bool    _confirmPending = false;
    uint8_t _brightness     = 75;
    uint8_t _volume         = 50;

    lv_obj_t* _screen                      = nullptr;
    lv_obj_t* _ring_labels[ITEM_COUNT]     = {};
    lv_obj_t* _center_name                 = nullptr;
    lv_obj_t* _center_value                = nullptr;
    lv_obj_t* _arc                         = nullptr;
    lv_obj_t* _arc_label                   = nullptr;
    lv_obj_t* _info_label                  = nullptr;
    lv_obj_t* _hint_label                  = nullptr;
};
