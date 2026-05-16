# StarBoy — Settings Menu Design

**Date:** 2026-05-16  
**Status:** Approved

---

## Overview

A settings menu activated via the PWR button (GPIO17). Uses LVGL 9.x widgets for rendering. When the menu is open, the Eyes module pauses rendering to avoid framebuffer conflicts.

---

## Menu Items (circular list)

| # | Label | Type | Behavior |
|---|-------|------|----------|
| 1 | Brightness | Arc slider | 10 steps 0–100%, maps to `setBrightness(0–255)` |
| 2 | Volume | Arc slider | 10 steps 0–100%, writes to ES8311 via I2C |
| 3 | Info | Read-only screen | Shows heap free, uptime, firmware version |
| 4 | Reboot | Action | Short press → red highlight warning, long press → `ESP.restart()` |
| 5 | Shutdown | Action | Short press → red highlight warning, long press → `esp_deep_sleep_start()` |

---

## Button Mapping

| Context | Short PWR press | Long PWR press |
|---------|----------------|----------------|
| Menu closed | Open menu, focus item 1 | — (no action) |
| Menu open, value item | +10% on current value (wraps 100→0%) | Apply value + close menu |
| Menu open, action item | Red highlight (confirmation state) | Execute action |

BOOT button (GPIO0) behavior unchanged: short = cycle emotion, long = reboot (only when menu is closed; ignored when menu is open).

---

## Architecture

### New files

```
src/menu/Menu.h         — MenuState enum, item definitions, public API
src/menu/Menu.cpp       — state machine, PWR debounce, LVGL screen management
src/audio/Audio.h       — ES8311 public API: begin(), setVolume(), setMute()
src/audio/Audio.cpp     — ES8311 I2C init (addr 0x18), volume register writes
```

### Modified files

```
src/display/Display.h/.cpp  — add lvglInit(), LVGL flush callback
src/main.cpp                — add lvglTask (FreeRTOS), PWR button polling, g_menuActive flag
```

### FreeRTOS layout

```
Core 0: eyesTask   priority 5  — Eyes::update() + vTaskDelay(5ms)
Core 1: lvglTask   priority 3  — lv_task_handler() every 5ms (only when g_menuActive)
Core 1: loop()     priority 1  — PWR + BOOT polling, mode switching
```

### Mode switching (no mutex required)

- `g_menuActive` is `volatile bool`, written only from `loop()` on Core 1.
- `eyesTask` on Core 0 reads `_rendering` flag (set via `Eyes::setRendering()`).
- Eyes and LVGL never render simultaneously — Approach A guarantee.

**Open sequence:**
1. `loop()` detects short PWR press
2. `Eyes::setRendering(false)`
3. `g_menuActive = true`
4. `Menu::open()` — clears canvas to black, builds LVGL screen

**Close sequence:**
1. `loop()` detects long PWR press
2. `Menu::applyAndClose()` — applies brightness/volume, destroys LVGL screen
3. `g_menuActive = false`
4. `Eyes::setRendering(true)`

---

## LVGL HAL Integration

```cpp
// Flush callback — called by LVGL after rendering a partial area
void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    auto* gfx = Display::instance().gfx();
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t*)px_map, w, h);
    Display::instance().flush();
    lv_display_flush_ready(disp);
}
```

LVGL draw buffer: two buffers of `466 * 10 * 2` bytes (~9KB each) in internal RAM.

`lv_tick_custom` already set to `millis()` in `lv_conf.h`.

---

## Visual Design

**Theme:** Obsidian Mirror  
- Background: `#000000`  
- Inactive items: `#444444`  
- Active item text: `#FFA500` (amber)  
- Arc/bar fill: `#FFA500`  
- Font: Montserrat 16 (already enabled in `lv_conf.h`)

**Layout (466×466 circle):**

```
         ○  Brightness      75%    ← active (amber)
         ○  Volume          50%
         ○  Info
         ○  Reboot
         ○  Shutdown

         ████████░░░░  75%         ← lv_arc for active item value
```

**Action items (Reboot / Shutdown):**
- Default: amber label
- After short press (confirmation state): text turns red
- Long press while red: execute

**Info screen (replaces arc when Info is active):**
```
Heap:  245 KB free
Up:    00:03:42
FW:    v0.0.1
```

---

## Audio Module (stub scope)

`Audio::begin()` initializes ES8311 over I2C (shared bus GPIO47/48, addr `0x18`).  
`Audio::setVolume(uint8_t pct)` writes to ES8311 DAC volume register.  
`Audio::setMute(bool)` toggles PA_CTRL (GPIO46).  

Full audio pipeline (mic → STT → TTS → speaker) is out of scope for this spec.

---

## Out of Scope

- Touch input for menu navigation (future)
- Persistent settings storage (NVS) — values reset on reboot for now
- Eye animation improvements — separate task
