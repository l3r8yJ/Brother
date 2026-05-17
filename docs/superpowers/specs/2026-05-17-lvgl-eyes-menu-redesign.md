# LVGL Eyes & Menu Redesign

**Date:** 2026-05-17  
**Status:** Approved  
**Display:** 466×466 round AMOLED (CO5300), center=(233,233)

---

## 1. Goals

1. Fix menu items clipping outside round bezel
2. Replace procedural Arduino_GFX eye drawing with LVGL canvas
3. Anime/kawaii eye style with proper eyelids and eyebrows
4. Single LVGL rendering path — no FreeRTOS eye task

**Fallback:** If lv_canvas performance is unsatisfactory, revert eyes to Arduino_GFX and keep only menu fix.

---

## 2. Architecture

### Before
```
Core 0: eyesTask  → Arduino_GFX canvas → flush → CO5300
Core 1: loop()    → lv_task_handler() → LVGL flush → CO5300
```
Two rendering paths, potential SPI contention, not thread-safe.

### After
```
Core 1 loop():
  handleBoot / handlePwr
  lv_task_handler()
    ├── lv_timer (16ms) → Eyes::drawFrame() → lv_canvas → LVGL flush
    └── Menu screen when open → LVGL flush
  telemetry
```

### Key changes
- `lv_conf.h`: `LV_USE_CANVAS 1`
- Remove `xTaskCreatePinnedToCore(eyesTask...)` from `main.cpp`
- `Eyes::begin()` creates `lv_canvas` on `_eyes_screen`, registers `lv_timer_t` at 60fps
- `Eyes::drawFrame()` replaces `Eyes::update()` — draws directly to canvas
- Two LVGL screens: `_eyes_screen` (default) and `_menu_screen`
- `Menu::open()` calls `lv_scr_load(_menu_screen)`
- `Menu::destroyScreen()` calls `Eyes::instance().showEyesScreen()` — Eyes owns its screen
- `g_menuActive` flag retained; Eyes timer checks `_rendering` before drawing

### Canvas buffer
```cpp
// Allocated in PSRAM via ps_malloc in Eyes::begin()
// 466 * 466 * 2 bytes = 434 KB
lv_color_t* _canvas_buf;  // member of Eyes

// In begin():
_canvas_buf = (lv_color_t*)ps_malloc(466 * 466 * sizeof(lv_color_t));
_eyes_screen = lv_obj_create(NULL);
_canvas = lv_canvas_create(_eyes_screen);
lv_canvas_set_buffer(_canvas, _canvas_buf, 466, 466, LV_COLOR_FORMAT_RGB565);
lv_obj_center(_canvas);
lv_scr_load(_eyes_screen);
```

---

## 3. Eyes: Anime/Kawaii Style

### Layout constants (unchanged)
```cpp
EYE_LEFT_X  = 133
EYE_RIGHT_X = 333
EYE_Y       = 233
IRIS_R      = 90
PUPIL_R     = 36
MAX_GAZE    = 25
```

### Updated EyeState
```cpp
struct EyeState {
    float lidTop;       // 1.0=fully open, 0.0=fully closed from top
    float lidBot;       // 0.0=no lower coverage, 1.0=crescent (happy squint)
    float pupilScale;   // 1.0=normal, 1.6=surprised
    float browInnerDY;  // brow inner-edge Y offset (negative=up)
    float browOuterDY;  // brow outer-edge Y offset (negative=up)
};
```

### Emotion targets

| Emotion   | lidTop | lidBot | pupil | brow_inner | brow_outer |
|-----------|--------|--------|-------|------------|------------|
| IDLE      | 1.00   | 0.00   | 1.0   |  0         |  0         |
| HAPPY     | 0.50   | 0.40   | 0.8   | −8         | −12        |
| ANGRY     | 0.70   | 0.00   | 0.7   | +12        | −8         |
| SAD       | 0.75   | 0.00   | 0.75  | −10        | +8         |
| SLEEPY    | 0.20   | 0.00   | 0.6   | +4         | +8         |
| SURPRISED | 1.00   | 0.00   | 1.6   | −20        | −20        |

Animation: lerp k=0.12 per frame on all 5 fields.

### Per-frame draw order (per eye)

```
lv_canvas_fill_bg(black)   — once per frame for full canvas

For each eye at (cx, cy):

1. IRIS
   draw filled circle: amber (#FFAA00), center=(cx,cy), r=IRIS_R

2. UPPER EYELID (clip from top)
   upper_h = (1.0f - lidTop) * (IRIS_R * 2 + 4)
   draw filled rect: black, x=cx-IRIS_R-2, y=cy-IRIS_R-2, w=IRIS_R*2+4, h=upper_h

3. LOWER EYELID (clip from bottom, for happy squint)
   lower_h = lidBot * IRIS_R
   draw filled rect: black, x=cx-IRIS_R-2, y=cy+IRIS_R+2-lower_h, w=IRIS_R*2+4, h=lower_h

4. PUPIL
   pr = PUPIL_R * pupilScale, clamped inside iris
   draw filled circle: black, center=(cx+gaze_dx, cy+gaze_dy), r=pr

5. GLINTS
   draw filled circle: white, offset=(-25,-25) from pupil, r=7  (main)
   draw filled circle: white, offset=(+15,-30) from pupil, r=3
   draw filled circle: white, offset=(-30,+10) from pupil, r=3

6. EYEBROW
   brow_base_y = cy - IRIS_R - 22
   inner_x = cx + (eye==LEFT ? +35 : -35)   (toward nose)
   outer_x = cx + (eye==LEFT ? -40 : +40)
   inner_y = brow_base_y + browInnerDY
   outer_y = brow_base_y + browOuterDY
   draw line: amber (#FFAA00), width=5px, points=[inner, outer]
```

### Blink
Same mechanism: `triggerBlink()` overrides `lidTop` via `_blinkState`. After blink completes, `lidTop` returns to `_target.lidTop`. Auto-blink every 2–5s.

### Touch tracking
`lookAt(normX, normY)` unchanged — sets `_lookX/_lookY`, applied as gaze offset to pupil.

### Public API changes
```cpp
// Removed:
void update();

// Added:
void drawFrame();  // called by lv_timer_t internally
```
`setEmotion()`, `lookAt()`, `triggerBlink()`, `setRendering()` — unchanged.

---

## 4. Menu: Radial Ring

### Ring positions (dx, dy from center 233,233), R=160px

| Item       | dx   | dy   | Angle |
|------------|------|------|-------|
| ЯРКОСТЬ    |    0 | −160 | −90°  |
| ГРОМКОСТЬ  | +152 |  −49 | −18°  |
| ИНФО       |  +94 | +129 |  +54° |
| ПЕРЕЗАГР.  |  −94 | +129 | +126° |
| ВЫКЛЮЧИТЬ  | −152 |  −49 | +198° |

All positions verified to be within the 466×466 circle (distance from center = 160px < 233px bezel).

### LVGL widgets

```
_menu_screen        lv_obj, black bg, 466x466
  _ring_labels[5]   lv_label, aligned with LV_ALIGN_CENTER + ring dx/dy
  _center_name      lv_label, LV_ALIGN_CENTER, 0, -20  — item name, font_24, amber
  _center_value     lv_label, LV_ALIGN_CENTER, 0, +20  — value/hint, font_20, white
  _arc              lv_arc, LV_ALIGN_CENTER, size=160   — editing mode
  _arc_label        lv_label, LV_ALIGN_CENTER           — arc value text
  _hint_label       lv_label, LV_ALIGN_BOTTOM_MID, 0,-12 — font_14, dark gray
```

`_infoLabel` (multiline) placed at `LV_ALIGN_CENTER, 0, +30` with width=260.

### Ring label styling
- Not selected: color=#666666, font_montserrat_14
- Selected: color=#FFAA00, font_montserrat_20

Center `_center_name` shows selected item name. `_center_value` shows:
- Brightness: "NN%"
- Volume: "NN%"
- Info: hidden (info text in `_infoLabel`)
- Reboot/Shutdown: "LONG=подтвердить" when confirmPending

### Navigation logic (unchanged)
- SHORT in NAVIGATING: advance focus, auto-close after last item
- LONG in NAVIGATING: enter EDITING
- SHORT in EDITING: change value (Brightness/Volume +25%), confirm (Reboot/Shutdown)
- LONG in EDITING: back to NAVIGATING

---

## 5. Files Changed

| File | Change |
|------|--------|
| `include/lv_conf.h` | `LV_USE_CANVAS 1` |
| `src/eyes/Eyes.h` | Update EyeState, add `_eyes_screen`, `_canvas`, `_canvas_buf`, `_timer` |
| `src/eyes/Eyes.cpp` | Rewrite rendering: lv_canvas draw, remove Arduino_GFX dep |
| `src/menu/Menu.cpp` | Radial layout, add `_center_name`, `_center_value`, fix positioning |
| `src/menu/Menu.h` | Add new widget members |
| `src/main.cpp` | Remove `eyesTask`, remove FPS counter (no longer needed) |

---

## 6. LVGL 9.1 Canvas API Note

LVGL 9.x uses layer-based canvas drawing. During implementation verify exact API:
```cpp
// LVGL 9.x style:
lv_layer_t layer;
lv_canvas_init_layer(canvas, &layer);
lv_draw_rect(&layer, &rect_dsc, &coords);
lv_canvas_finish_layer(canvas, &layer);
```
If `lv_canvas_init_layer` not available in installed version, fall back to `lv_canvas_draw_rect` (LVGL 8.x compat shim). Verify against installed headers before coding.

---

## 7. Risk & Rollback

**Risk:** `lv_canvas` fill+draw for two eyes each frame may be too slow for 60fps on ESP32-S3.  
**Mitigation:** If <30fps, reduce to 30fps timer. If still unsatisfactory, revert Eyes.cpp to Arduino_GFX and keep only Menu redesign.

**Rollback trigger:** `[Eyes] fps < 20` in serial output over 5 seconds.
