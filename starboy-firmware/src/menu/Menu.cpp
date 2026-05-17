#include "Menu.h"
#include "display/Display.h"
#include "eyes/Eyes.h"
#include "audio/Audio.h"
#include <Arduino.h>
#include <esp_sleep.h>

constexpr int16_t Menu::kRingDx[5];
constexpr int16_t Menu::kRingDy[5];

Menu& Menu::instance() {
    static Menu inst;
    return inst;
}

void Menu::begin() {}

void Menu::open() {
    _open           = true;
    _state          = State::NAVIGATING;
    _focus          = 0;
    _confirmPending = false;
    buildScreen();
}

void Menu::onShortPress() {
    if (!_open) return;

    if (_state == State::NAVIGATING) {
        if (_focus < ITEM_COUNT - 1) {
            _focus++;
            updateUI();
        } else {
            destroyScreen();
        }
        return;
    }

    // EDITING
    Item item = static_cast<Item>(_focus);
    if (item == Item::BRIGHTNESS) {
        _brightness += 25;
        if (_brightness > 100) _brightness = 0;
        Display::instance().setBrightness(_brightness);
        updateUI();
    } else if (item == Item::VOLUME) {
        _volume += 25;
        if (_volume > 100) _volume = 0;
        Audio::instance().setVolume(_volume);
        updateUI();
    } else if (item == Item::REBOOT) {
        if (_confirmPending) {
            destroyScreen();
            delay(100);
            ESP.restart();
        } else {
            _confirmPending = true;
            updateUI();
        }
    } else if (item == Item::SHUTDOWN) {
        if (_confirmPending) {
            destroyScreen();
            Display::instance().setBrightness(0);
            delay(100);
            esp_deep_sleep_start();
        } else {
            _confirmPending = true;
            updateUI();
        }
    } else if (item == Item::INFO) {
        updateInfoText();
        updateUI();
    }
}

void Menu::onLongPress() {
    if (!_open) return;

    if (_state == State::NAVIGATING) {
        _state          = State::EDITING;
        _confirmPending = false;
        updateUI();
        return;
    }
    _state          = State::NAVIGATING;
    _confirmPending = false;
    updateUI();
}

// ── Private ───────────────────────────────────────────────────────────────────

void Menu::buildScreen() {
    _screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_screen, LV_OPA_COVER, 0);
    lv_obj_set_size(_screen, 466, 466);

    // Ring labels — positioned on circle of radius 160px
    for (int i = 0; i < ITEM_COUNT; i++) {
        _ring_labels[i] = lv_label_create(_screen);
        lv_label_set_text(_ring_labels[i], kNames[i]);
        lv_obj_set_style_text_font(_ring_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(_ring_labels[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(_ring_labels[i], LV_ALIGN_CENTER, kRingDx[i], kRingDy[i]);
    }

    // Center: selected item name
    _center_name = lv_label_create(_screen);
    lv_obj_set_style_text_font(_center_name, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_center_name, lv_color_hex(0xFFAA00), 0);
    lv_obj_set_style_text_align(_center_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_center_name, LV_ALIGN_CENTER, 0, -20);

    // Center: current value or status
    _center_value = lv_label_create(_screen);
    lv_obj_set_style_text_font(_center_value, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_center_value, lv_color_white(), 0);
    lv_obj_set_style_text_align(_center_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_center_value, LV_ALIGN_CENTER, 0, +20);

    // Arc for brightness/volume editing
    _arc = lv_arc_create(_screen);
    lv_obj_set_size(_arc, 140, 140);
    lv_obj_align(_arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_range(_arc, 0, 100);
    lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);

    _arc_label = lv_label_create(_screen);
    lv_obj_set_style_text_font(_arc_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_arc_label, lv_color_hex(0xFFAA00), 0);
    lv_obj_align(_arc_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(_arc_label, LV_OBJ_FLAG_HIDDEN);

    // Info multiline label
    _info_label = lv_label_create(_screen);
    lv_obj_set_style_text_font(_info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_info_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_width(_info_label, 200);
    lv_obj_align(_info_label, LV_ALIGN_CENTER, 0, +30);
    lv_label_set_text(_info_label, "");
    lv_obj_add_flag(_info_label, LV_OBJ_FLAG_HIDDEN);

    // Hint at bottom
    _hint_label = lv_label_create(_screen);
    lv_obj_set_style_text_font(_hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_hint_label, lv_color_hex(0x555555), 0);
    lv_obj_align(_hint_label, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_label_set_text(_hint_label, "SHORT=вперёд  LONG=выбор");

    lv_scr_load(_screen);
    updateUI();
}

void Menu::destroyScreen() {
    if (!_screen) return;
    lv_obj_del(_screen);
    _screen       = nullptr;
    _arc          = nullptr;
    _arc_label    = nullptr;
    _info_label   = nullptr;
    _hint_label   = nullptr;
    _center_name  = nullptr;
    _center_value = nullptr;
    for (int i = 0; i < ITEM_COUNT; i++) _ring_labels[i] = nullptr;
    _open = false;
    Eyes::instance().setRendering(true);
    Eyes::instance().showEyesScreen();
}

void Menu::updateUI() {
    // Ring labels: highlight selected
    for (int i = 0; i < ITEM_COUNT; i++) {
        if (!_ring_labels[i]) continue;
        bool sel = (i == _focus);
        lv_obj_set_style_text_color(_ring_labels[i],
            sel ? lv_color_hex(0xFFAA00) : lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(_ring_labels[i],
            sel ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
        lv_obj_align(_ring_labels[i], LV_ALIGN_CENTER, kRingDx[i], kRingDy[i]);
    }

    Item item    = static_cast<Item>(_focus);
    bool editing = (_state == State::EDITING);

    // Center name
    if (_center_name) lv_label_set_text(_center_name, kNames[_focus]);

    // Arc for brightness/volume in edit mode
    bool showArc = editing && (item == Item::BRIGHTNESS || item == Item::VOLUME);
    if (showArc) {
        lv_obj_clear_flag(_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(_arc_label, LV_OBJ_FLAG_HIDDEN);
        uint8_t val = (item == Item::BRIGHTNESS) ? _brightness : _volume;
        lv_arc_set_value(_arc, val);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(_arc_label, buf);
        if (_center_value) lv_label_set_text(_center_value, "");
    } else {
        lv_obj_add_flag(_arc, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(_arc_label, LV_OBJ_FLAG_HIDDEN);
    }

    // Center value
    if (_center_value && !showArc) {
        if (item == Item::BRIGHTNESS) {
            char buf[8]; snprintf(buf, sizeof(buf), "%d%%", _brightness);
            lv_label_set_text(_center_value, buf);
        } else if (item == Item::VOLUME) {
            char buf[8]; snprintf(buf, sizeof(buf), "%d%%", _volume);
            lv_label_set_text(_center_value, buf);
        } else if (_confirmPending) {
            lv_label_set_text(_center_value, "Подтвердить?");
        } else {
            lv_label_set_text(_center_value, "");
        }
    }

    // Info label
    if (editing && item == Item::INFO) {
        updateInfoText();
        lv_obj_clear_flag(_info_label, LV_OBJ_FLAG_HIDDEN);
        if (_center_value) lv_label_set_text(_center_value, "");
    } else {
        lv_obj_add_flag(_info_label, LV_OBJ_FLAG_HIDDEN);
    }

    // Hint
    if (_hint_label) {
        if (_confirmPending) {
            lv_label_set_text(_hint_label, "SHORT=да  LONG=отмена");
        } else if (editing) {
            lv_label_set_text(_hint_label, "SHORT=менять  LONG=назад");
        } else {
            lv_label_set_text(_hint_label, "SHORT=вперёд  LONG=выбор");
        }
    }
}

void Menu::updateInfoText() {
    if (!_info_label) return;
    char buf[128];
    snprintf(buf, sizeof(buf),
        "CPU: %lu MHz\nHeap: %lu KB\nPSRAM: %lu KB\nUptime: %lus",
        getCpuFrequencyMhz(),
        ESP.getFreeHeap() / 1024,
        ESP.getPsramSize() / 1024,
        millis() / 1000);
    lv_label_set_text(_info_label, buf);
}
