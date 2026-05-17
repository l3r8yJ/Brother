# LVGL Eyes & Menu Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace procedural Arduino_GFX eye rendering with LVGL canvas (anime/kawaii style + proper eyelids/brows), and fix menu layout for round 466×466 display with radial ring positioning.

**Architecture:** Single LVGL rendering path — remove FreeRTOS eye task, drive eyes via `lv_timer_t` inside `lv_task_handler()`. Eyes render to `lv_canvas` (434KB PSRAM buffer). Menu uses `lv_label` widgets at calculated ring positions. Two LVGL screens switch on menu open/close.

**Tech Stack:** LVGL 9.1, ESP32-S3 Arduino, PlatformIO. Canvas API: `lv_canvas_init_layer` / `lv_canvas_finish_layer` + `lv_draw_rect` / `lv_draw_line` / `lv_draw_arc`.

---

## File Map

| File | Action | Responsibility |
|------|--------|---------------|
| `include/lv_conf.h` | Modify | Enable `LV_USE_CANVAS` |
| `src/eyes/Eyes.h` | Modify | Updated `EyeState` (5 fields), new `lv_canvas` members, `showEyesScreen()` |
| `src/eyes/Eyes.cpp` | Rewrite | `lv_canvas` draw loop, layer-based API, eyebrows, proper eyelids |
| `src/menu/Menu.h` | Modify | New widgets: `_center_name`, `_center_value` labels |
| `src/menu/Menu.cpp` | Rewrite | Radial ring positions, center display |
| `src/main.cpp` | Modify | Remove `eyesTask`, `Eyes::showEyesScreen()` on menu close |

---

## Build / Flash Commands

```bash
# Build only (from starboy-firmware/ directory):
pio run

# Build + flash:
pio run -t upload

# Serial monitor:
pio device monitor --baud 115200
```

All "verify build" steps run `pio run`. All "flash + observe" steps run `pio run -t upload` then monitor.

---

## Task 1: Enable LV_USE_CANVAS

**Files:**
- Modify: `include/lv_conf.h`

- [ ] **Step 1: Enable canvas widget**

In `include/lv_conf.h`, find and change:
```c
// Before:
#define LV_USE_ANIMIMG   0
// (LV_USE_CANVAS not present or set to 0)

// Add/change — place near LV_USE_LABEL, LV_USE_ARC:
#define LV_USE_CANVAS    1
```

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: compiles without errors. If "LV_USE_CANVAS not defined" — search `lv_conf.h` for `CANVAS` and set that line to `1`.

- [ ] **Step 3: Commit**

```bash
git add include/lv_conf.h
git commit -m "feat(lvgl): enable LV_USE_CANVAS"
```

---

## Task 2: Update Eyes.h

**Files:**
- Modify: `src/eyes/Eyes.h`

- [ ] **Step 1: Replace EyeState and update class**

Replace the entire `Eyes.h` content:

```cpp
#pragma once
#include <Arduino.h>
#include <lvgl.h>

enum class Emotion { IDLE, HAPPY, ANGRY, SAD, SLEEPY, SURPRISED };

struct EyeState {
    float lidTop;       // 1.0=fully open, 0.0=fully closed from top
    float lidBot;       // 0.0=no lower clip, 1.0=full lower squint (happy)
    float pupilScale;   // 1.0=normal, 1.6=surprised
    float browInnerDY;  // brow inner edge vertical offset (negative=up)
    float browOuterDY;  // brow outer edge vertical offset (negative=up)
};

class Eyes {
public:
    static Eyes& instance();

    void setRendering(bool enabled) { _rendering = enabled; }
    void showEyesScreen();
    void begin();
    void setEmotion(Emotion e);
    void lookAt(float normX, float normY);
    void triggerBlink();

private:
    Eyes() = default;

    static void timerCb(lv_timer_t* t);
    void drawFrame();
    void drawEye(lv_layer_t* layer, int16_t cx, int16_t cy, float gazeDx, float gazeDy);
    void animateEmotion();
    void animateBlink();
    static EyeState emotionTarget(Emotion e);
    static float lerp(float a, float b, float t) { return a + (b - a) * t; }

    bool _rendering = true;

    Emotion  _emotion = Emotion::IDLE;
    EyeState _state   = {1.0f, 0.0f, 1.0f, 0.0f, 0.0f};
    EyeState _target  = {1.0f, 0.0f, 1.0f, 0.0f, 0.0f};

    float _lookX = 0.0f;
    float _lookY = 0.0f;

    bool     _blinking     = false;
    uint32_t _blinkStartMs = 0;
    uint32_t _nextBlinkMs  = 3000;

    static constexpr uint32_t BLINK_DOWN_MS = 300;
    static constexpr uint32_t BLINK_HOLD_MS = 50;
    static constexpr uint32_t BLINK_UP_MS   = 150;

    // Layout — tuned for 466×466 round AMOLED
    static constexpr int16_t EYE_LEFT_X  = 133;
    static constexpr int16_t EYE_RIGHT_X = 333;
    static constexpr int16_t EYE_Y       = 233;
    static constexpr int16_t IRIS_R      = 90;
    static constexpr int16_t PUPIL_R     = 36;
    static constexpr int16_t MAX_GAZE    = 25;
    static constexpr int16_t BROW_BASE_Y = 22;  // pixels above iris top
    static constexpr int16_t BROW_INNER  = 35;  // x offset toward nose
    static constexpr int16_t BROW_OUTER  = 40;  // x offset away from nose
    static constexpr int16_t BROW_WIDTH  = 5;

    // Colors
    static constexpr uint32_t C_IRIS  = 0xFFAA00;
    static constexpr uint32_t C_BG    = 0x000000;
    static constexpr uint32_t C_PUPIL = 0x000000;
    static constexpr uint32_t C_GLINT = 0xFFFFFF;
    static constexpr uint32_t C_BROW  = 0xFFAA00;

    lv_obj_t*   _eyes_screen = nullptr;
    lv_obj_t*   _canvas      = nullptr;
    lv_color_t* _canvas_buf  = nullptr;
    lv_timer_t* _timer       = nullptr;
};
```

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: compiles (Eyes.cpp still has old implementation — linker errors for removed `update()` expected). If so, proceed to Task 3.

---

## Task 3: Rewrite Eyes.cpp

**Files:**
- Modify: `src/eyes/Eyes.cpp`

- [ ] **Step 1: Replace Eyes.cpp entirely**

```cpp
#include "Eyes.h"
#include <lvgl.h>

Eyes& Eyes::instance() {
    static Eyes inst;
    return inst;
}

void Eyes::begin() {
    _state  = emotionTarget(Emotion::IDLE);
    _target = _state;
    _nextBlinkMs = millis() + 2000 + random(3000);

    // Allocate canvas buffer in PSRAM
    _canvas_buf = (lv_color_t*)ps_malloc(466 * 466 * sizeof(lv_color_t));
    if (!_canvas_buf) {
        Serial.println("[Eyes] FATAL: ps_malloc failed");
        return;
    }

    _eyes_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_eyes_screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_eyes_screen, LV_OPA_COVER, 0);

    _canvas = lv_canvas_create(_eyes_screen);
    lv_canvas_set_buffer(_canvas, _canvas_buf, 466, 466, LV_COLOR_FORMAT_NATIVE);
    lv_obj_set_pos(_canvas, 0, 0);

    lv_scr_load(_eyes_screen);

    _timer = lv_timer_create(timerCb, 16, nullptr);

    Serial.println("[Eyes] OK (lv_canvas 434KB PSRAM)");
}

void Eyes::showEyesScreen() {
    if (_eyes_screen) lv_scr_load(_eyes_screen);
}

void Eyes::setEmotion(Emotion e) {
    _emotion = e;
    _target  = emotionTarget(e);
}

void Eyes::lookAt(float normX, float normY) {
    _lookX = constrain(normX, -1.0f, 1.0f);
    _lookY = constrain(normY, -1.0f, 1.0f);
}

void Eyes::triggerBlink() {
    if (_blinking) return;
    _blinking     = true;
    _blinkStartMs = millis();
}

// ── Timer callback ────────────────────────────────────────────────────────────

void Eyes::timerCb(lv_timer_t* t) {
    Eyes::instance().drawFrame();
}

void Eyes::drawFrame() {
    if (!_rendering || !_canvas) return;

    animateEmotion();
    animateBlink();

    // Auto-blink
    if (!_blinking && millis() >= _nextBlinkMs) triggerBlink();

    lv_canvas_fill_bg(_canvas, lv_color_black(), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(_canvas, &layer);

    float gazeDx = _lookX * MAX_GAZE;
    float gazeDy = _lookY * MAX_GAZE;

    drawEye(&layer, EYE_LEFT_X,  EYE_Y, gazeDx, gazeDy);
    drawEye(&layer, EYE_RIGHT_X, EYE_Y, gazeDx, gazeDy);

    lv_canvas_finish_layer(_canvas, &layer);
}

// ── Draw one eye ──────────────────────────────────────────────────────────────

void Eyes::drawEye(lv_layer_t* layer, int16_t cx, int16_t cy, float gazeDx, float gazeDy) {
    lv_draw_rect_dsc_t rDsc;
    lv_draw_rect_dsc_init(&rDsc);

    // 1. IRIS — always full circle; eyelid rects clip it visually
    rDsc.bg_color = lv_color_hex(C_IRIS);
    rDsc.bg_opa   = LV_OPA_COVER;
    rDsc.radius   = LV_RADIUS_CIRCLE;
    lv_area_t iris_area = {
        cx - IRIS_R,
        cy - IRIS_R,
        cx + IRIS_R - 1,
        cy + IRIS_R - 1
    };
    lv_draw_rect(layer, &rDsc, &iris_area);

    // 2. UPPER EYELID — black rect clips top of iris
    int16_t upper_h = (int16_t)((1.0f - _state.lidTop) * (IRIS_R * 2 + 4));
    if (upper_h > 0) {
        rDsc.bg_color = lv_color_black();
        rDsc.radius   = 0;
        lv_area_t lid_area = {
            cx - IRIS_R - 2,
            cy - IRIS_R - 2,
            cx + IRIS_R + 1,
            cy - IRIS_R - 2 + upper_h
        };
        lv_draw_rect(layer, &rDsc, &lid_area);
    }

    // 3. LOWER EYELID — black rect clips bottom (happy squint)
    int16_t lower_h = (int16_t)(_state.lidBot * IRIS_R);
    if (lower_h > 0) {
        rDsc.bg_color = lv_color_black();
        rDsc.radius   = 0;
        lv_area_t bot_area = {
            cx - IRIS_R - 2,
            cy + IRIS_R + 2 - lower_h,
            cx + IRIS_R + 1,
            cy + IRIS_R + 2
        };
        lv_draw_rect(layer, &rDsc, &bot_area);
    }

    // 4. PUPIL
    float dist, maxDist;
    int16_t pdx = (int16_t)gazeDx;
    int16_t pdy = (int16_t)gazeDy;
    int16_t pr  = (int16_t)(PUPIL_R * _state.pupilScale);
    pr = min<int16_t>(pr, IRIS_R - 4);

    dist    = sqrtf((float)(pdx * pdx + pdy * pdy));
    maxDist = IRIS_R - pr - 4;
    if (dist > maxDist && dist > 0) {
        pdx = (int16_t)(pdx * maxDist / dist);
        pdy = (int16_t)(pdy * maxDist / dist);
    }
    int16_t pcx = cx + pdx;
    int16_t pcy = cy + pdy;

    rDsc.bg_color = lv_color_black();
    rDsc.radius   = LV_RADIUS_CIRCLE;
    lv_area_t pupil_area = { pcx - pr, pcy - pr, pcx + pr - 1, pcy + pr - 1 };
    lv_draw_rect(layer, &rDsc, &pupil_area);

    // 5. GLINTS (only when eye is mostly open)
    if (_state.lidTop > 0.3f) {
        rDsc.bg_color = lv_color_white();
        rDsc.radius   = LV_RADIUS_CIRCLE;

        // Main glint
        int16_t gx = pcx - 20, gy = pcy - 20;
        lv_area_t g1 = { gx - 7, gy - 7, gx + 6, gy + 6 };
        lv_draw_rect(layer, &rDsc, &g1);

        // Small glints
        gx = pcx + 12; gy = pcy - 28;
        lv_area_t g2 = { gx - 3, gy - 3, gx + 2, gy + 2 };
        lv_draw_rect(layer, &rDsc, &g2);

        gx = pcx - 28; gy = pcy + 8;
        lv_area_t g3 = { gx - 3, gy - 3, gx + 2, gy + 2 };
        lv_draw_rect(layer, &rDsc, &g3);
    }

    // 6. EYEBROW — two-point line
    lv_draw_line_dsc_t lDsc;
    lv_draw_line_dsc_init(&lDsc);
    lDsc.color       = lv_color_hex(C_BROW);
    lDsc.width       = BROW_WIDTH;
    lDsc.round_start = 1;
    lDsc.round_end   = 1;

    int16_t brow_base = cy - IRIS_R - BROW_BASE_Y;
    // Inner = toward nose, outer = away from nose
    bool isLeft = (cx == EYE_LEFT_X);
    lv_point_precise_t p_inner = {
        (float)(cx + (isLeft ? +BROW_INNER : -BROW_INNER)),
        (float)(brow_base + _state.browInnerDY)
    };
    lv_point_precise_t p_outer = {
        (float)(cx + (isLeft ? -BROW_OUTER : +BROW_OUTER)),
        (float)(brow_base + _state.browOuterDY)
    };
    lDsc.p1 = p_inner;
    lDsc.p2 = p_outer;
    lv_draw_line(layer, &lDsc);
}

// ── Animation ─────────────────────────────────────────────────────────────────

void Eyes::animateEmotion() {
    constexpr float k = 0.12f;
    _state.lidTop      = lerp(_state.lidTop,      _target.lidTop,      k);
    _state.lidBot      = lerp(_state.lidBot,       _target.lidBot,      k);
    _state.pupilScale  = lerp(_state.pupilScale,   _target.pupilScale,  k);
    _state.browInnerDY = lerp(_state.browInnerDY,  _target.browInnerDY, k);
    _state.browOuterDY = lerp(_state.browOuterDY,  _target.browOuterDY, k);
}

void Eyes::animateBlink() {
    if (!_blinking) return;
    uint32_t t = millis() - _blinkStartMs;

    if (t < BLINK_DOWN_MS) {
        _state.lidTop = _target.lidTop * (1.0f - (float)t / BLINK_DOWN_MS);
    } else if (t < BLINK_DOWN_MS + BLINK_HOLD_MS) {
        _state.lidTop = 0.0f;
    } else {
        uint32_t upT = t - BLINK_DOWN_MS - BLINK_HOLD_MS;
        _state.lidTop = _target.lidTop * ((float)upT / BLINK_UP_MS);
        if (_state.lidTop >= _target.lidTop) {
            _state.lidTop = _target.lidTop;
            _blinking     = false;
            _nextBlinkMs  = millis() + 2000 + random(3000);
        }
    }
}

EyeState Eyes::emotionTarget(Emotion e) {
    // { lidTop, lidBot, pupilScale, browInnerDY, browOuterDY }
    switch (e) {
        case Emotion::HAPPY:     return { 0.50f, 0.40f, 0.80f,  -8.0f, -12.0f };
        case Emotion::ANGRY:     return { 0.70f, 0.00f, 0.70f,  12.0f,  -8.0f };
        case Emotion::SAD:       return { 0.75f, 0.00f, 0.75f, -10.0f,   8.0f };
        case Emotion::SLEEPY:    return { 0.20f, 0.00f, 0.60f,   4.0f,   8.0f };
        case Emotion::SURPRISED: return { 1.00f, 0.00f, 1.60f, -20.0f, -20.0f };
        default:                 return { 1.00f, 0.00f, 1.00f,   0.0f,   0.0f };
    }
}
```

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: compiles cleanly. Common issues:
- `ps_malloc` undefined → add `#include <esp_heap_caps.h>` at top
- `lv_color_format_t` enum → check `lv_conf.h` sets `LV_COLOR_DEPTH 16`
- `lv_point_precise_t` not found → use `lv_point_t` and cast fields

- [ ] **Step 3: Commit**

```bash
git add src/eyes/Eyes.h src/eyes/Eyes.cpp
git commit -m "feat(eyes): rewrite to LVGL canvas, anime/kawaii style"
```

---

## Task 4: Update main.cpp

**Files:**
- Modify: `src/main.cpp`

- [ ] **Step 1: Remove eyesTask, update menu close, remove FPS counter**

In `src/main.cpp`:

**Remove** the entire `eyesTask` function (lines ~52–60):
```cpp
// DELETE THIS ENTIRE FUNCTION:
static void eyesTask(void*) {
    for (;;) {
        if (!g_menuActive) {
            Eyes::instance().update();
            s_frameCount = s_frameCount + 1;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
```

**Remove** the FPS counter variable:
```cpp
// DELETE:
static volatile uint32_t s_frameCount = 0;
```

**In `setup()`**, remove the `xTaskCreatePinnedToCore` call:
```cpp
// DELETE:
xTaskCreatePinnedToCore(eyesTask, "eyes", 8192, nullptr, 5, nullptr, 0);
```

**In `handlePwr()`** and **`pollPwrLong()`**, find all occurrences of:
```cpp
Eyes::instance().setRendering(true);
```
Replace each with:
```cpp
Eyes::instance().setRendering(true);
Eyes::instance().showEyesScreen();
```

**In `loop()`**, the `lv_tick_inc` + `lv_task_handler` block — remove the `if (g_menuActive)` guard so LVGL always runs:

```cpp
// BEFORE:
if (g_menuActive) {
    lv_tick_inc(5);
    lv_task_handler();
}

// AFTER:
lv_tick_inc(5);
lv_task_handler();
```

**Remove** FPS telemetry line in loop():
```cpp
// DELETE: s_frameCount = 0;
```

Update telemetry printf to remove FPS:
```cpp
// BEFORE:
Serial.printf("[loop] heap=%lu KB menu=%d emo=%d\n",
              ESP.getFreeHeap() / 1024, (int)g_menuActive, s_emotionIdx);
// AFTER (same, just no s_frameCount):
Serial.printf("[loop] heap=%lu KB menu=%d emo=%d\n",
              ESP.getFreeHeap() / 1024, (int)g_menuActive, s_emotionIdx);
```
(No change needed if FPS wasn't in this printf — just remove `s_frameCount = 0`)

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: no `eyesTask` references, no `s_frameCount` references. Fix any remaining linker errors.

- [ ] **Step 3: Flash and verify eyes work**

```bash
pio run -t upload
pio device monitor --baud 115200
```
Expected on serial:
```
StarBoy boot v0.0.2
[Eyes] OK (lv_canvas 434KB PSRAM)
[Audio] ES8311 OK
Boot OK
```
Visual: amber anime eyes appear on screen, blinking every 2–5s, brows visible above eyes.

If eyes don't appear: check `lv_task_handler()` is being called unconditionally in `loop()`.
If crash: "eyes FATAL ps_malloc" → PSRAM not initialized; check `platformio.ini` has `-DBOARD_HAS_PSRAM` and `board_build.arduino.memory_type = qio_opi`.

- [ ] **Step 4: Test emotion cycling**

Press BOOT button — cycles through IDLE→HAPPY→ANGRY→SAD→SLEEPY→SURPRISED.
Verify each emotion changes brow angle and eyelid shape visibly.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp
git commit -m "refactor(main): remove eyesTask, drive eyes via LVGL timer"
```

---

## Task 5: Update Menu.h

**Files:**
- Modify: `src/menu/Menu.h`

- [ ] **Step 1: Replace Menu.h content**

```cpp
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
    static const char* const kNames[5];

    void buildScreen();
    void destroyScreen();
    void updateUI();
    void updateInfoText();

    bool  _open           = false;
    State _state          = State::NAVIGATING;
    int   _focus          = 0;
    bool  _confirmPending = false;
    uint8_t _brightness   = 75;
    uint8_t _volume       = 50;

    lv_obj_t* _screen       = nullptr;
    lv_obj_t* _ring_labels[ITEM_COUNT] = {};
    lv_obj_t* _center_name  = nullptr;
    lv_obj_t* _center_value = nullptr;
    lv_obj_t* _arc          = nullptr;
    lv_obj_t* _arc_label    = nullptr;
    lv_obj_t* _info_label   = nullptr;
    lv_obj_t* _hint_label   = nullptr;
};
```

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: compiles (Menu.cpp still uses old members — will break in Task 6).

---

## Task 6: Rewrite Menu.cpp

**Files:**
- Modify: `src/menu/Menu.cpp`

- [ ] **Step 1: Replace Menu.cpp entirely**

```cpp
#include "Menu.h"
#include "display/Display.h"
#include "eyes/Eyes.h"
#include "audio/Audio.h"
#include <Arduino.h>
#include <esp_sleep.h>

const char* const Menu::kNames[5] = {
    "Яркость", "Громкость", "Инфо", "Перезагр.", "Выкл."
};

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
        _brightness = (_brightness + 25) % 125;
        if (_brightness > 100) _brightness = 0;
        Display::instance().setBrightness(_brightness);
        updateUI();
    } else if (item == Item::VOLUME) {
        _volume = (_volume + 25) % 125;
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

    // Ring labels
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

    // Center: value / hint
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

    // Info multi-line label (hidden by default)
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
        bool selected = (i == _focus);
        lv_obj_set_style_text_color(_ring_labels[i],
            selected ? lv_color_hex(0xFFAA00) : lv_color_hex(0x666666), 0);
        lv_obj_set_style_text_font(_ring_labels[i],
            selected ? &lv_font_montserrat_20 : &lv_font_montserrat_14, 0);
        // Re-align after font change (size may differ)
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

    // Center value (non-arc items)
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
```

- [ ] **Step 2: Verify build**

```bash
pio run
```
Expected: clean build. Common issues:
- `lv_font_montserrat_24` not enabled → add `#define LV_FONT_MONTSERRAT_24 1` to `lv_conf.h`
- `kRingDx` / `kRingDy` linker error → add definitions to Menu.cpp above `kNames`:
  ```cpp
  constexpr int16_t Menu::kRingDx[5];
  constexpr int16_t Menu::kRingDy[5];
  ```

- [ ] **Step 3: Flash and verify menu**

```bash
pio run -t upload
pio device monitor --baud 115200
```
Visual checks:
1. Eyes show on boot ✅
2. SHORT PWR button → menu appears with 5 labels on ring, Яркость highlighted at top
3. SHORT again → Громкость highlighted on right side
4. SHORT again → Инфо at bottom-right
5. No items clipped by round bezel ✅
6. LONG on any item → center shows arc (Яркость/Громкость) or info text
7. SHORT in editing → value changes
8. After last item (Выкл.) → menu closes, eyes resume

- [ ] **Step 4: Verify touch tracking still works**

Touch finger to screen → eyes follow. Remove finger → eyes center.

- [ ] **Step 5: Commit**

```bash
git add src/menu/Menu.h src/menu/Menu.cpp
git commit -m "feat(menu): radial ring layout for 466x466 round display"
```

---

## Task 7: Final Integration

- [ ] **Step 1: Test all emotions**

Press BOOT to cycle: IDLE → HAPPY → ANGRY → SAD → SLEEPY → SURPRISED  
For each: verify brows visible and correctly positioned, eyelids correct.

- [ ] **Step 2: Test blink**

Wait 2–5 seconds on IDLE. Verify smooth blink animation (down → hold → up). No flicker.

- [ ] **Step 3: Heap check**

```bash
pio device monitor --baud 115200
```
Look at `[loop] heap=XXX KB`. Must stay > 50KB. If < 30KB, canvas buffer may be overflowing heap — confirm PSRAM is available (`PSRAM: 8192 KB` in boot message).

- [ ] **Step 4: FPS check (optional)**

Add temporary FPS counter to `Eyes::drawFrame()`:
```cpp
static uint32_t frames = 0, last = 0;
frames++;
if (millis() - last > 1000) {
    Serial.printf("[Eyes] fps=%lu\n", frames);
    frames = 0; last = millis();
}
```
Expected: 55–62 fps. If < 20fps → reduce timer to 33ms (30fps), or investigate flush_cb bottleneck.

- [ ] **Step 5: Remove temp FPS counter if added, commit**

```bash
git add -u
git commit -m "feat: LVGL canvas eyes + radial menu — integration complete"
```

---

## Rollback

If eyes performance is insufficient (< 20fps after tuning):
1. Revert `src/eyes/Eyes.h` and `src/eyes/Eyes.cpp` to Arduino_GFX version
2. Restore `eyesTask` in `main.cpp`
3. Keep Menu.cpp/Menu.h radial layout (Task 5–6 unaffected)
4. Remove `LV_USE_CANVAS` from `lv_conf.h`
5. Free `_canvas_buf` and null `_canvas` / `_eyes_screen` in `Eyes`
