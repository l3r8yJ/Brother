# Display Driver Research — CO5300 + Arduino_GFX

> Исследование проведено до получения платы. Источник: официальное репо Waveshare
> ESP32-S3-Touch-AMOLED-1.75 (тот же CO5300 466×466) + Arduino_GFX upstream.

---

## Библиотечный стек

| Слой | Библиотека | Версия |
|------|-----------|--------|
| Дисплей (HAL) | GFX_Library_for_Arduino (Arduino_GFX) | v1.6.4 |
| GUI framework | LVGL | **8.3.11** (не смешивать с v9!) |
| Тач | SensorLib (от Waveshare) или TouchLib (mmMicky) | — |
| Аудио | ES8311 через I2S | — |

**Arduino ESP32 core:** минимум v3.3.0

---

## Инициализация дисплея

```cpp
#include "Arduino_GFX_Library.h"

// Шина QSPI — наши пины (1.32" отличаются от 1.75"!)
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS,   // GPIO10
    LCD_SCL,  // GPIO11
    LCD_D0,   // GPIO12
    LCD_D1,   // GPIO13
    LCD_D2,   // GPIO14
    LCD_D3    // GPIO15
);

// Дисплей CO5300
// ВАЖНО: col_offset1 = 6 — без этого картинка сдвинута!
Arduino_CO5300 *gfx = new Arduino_CO5300(
    bus,
    LCD_RESET,  // GPIO8
    0,          // rotation
    466,        // width
    466,        // height
    6,          // col_offset1 — ОБЯЗАТЕЛЬНО
    0, 0, 0     // остальные offsets = 0
);

void setup() {
    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    gfx->setBrightness(200); // 0–255
}
```

### Что делает col_offset1 = 6
CO5300 — контроллер на 480×480, но физический дисплей 466×466. Сдвиг 6 пикселей
компенсирует разницу в адресации столбцов. Без него изображение будет смещено влево.

---

## LVGL — буферная стратегия

Waveshare использует **два буфера по 1/4 экрана** в DMA-памяти (внутренняя SRAM):

```cpp
// 466 * 466 / 4 = 54,289 пикселей = ~106 KB на буфер
lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(
    466 * 466 / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);
lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(
    466 * 466 / 4 * sizeof(lv_color_t), MALLOC_CAP_DMA);

lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 466 * 466 / 4);
```

**Почему DMA, а не PSRAM?**
`MALLOC_CAP_DMA` выделяет из внутренней SRAM (она DMA-capable).
PSRAM медленнее для frame buffer — DMA из PSRAM добавляет латентность.
Внутренней SRAM у ESP32-S3 320 KB, из них ~212 KB свободно под буферы.

**Flush callback:**
```cpp
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}
```

---

## Ключевые отличия 1.32" от 1.75"

| Параметр | 1.75" (референс) | **Наш 1.32"** |
|----------|------------------|---------------|
| LCD_RESET | GPIO39 | **GPIO8** |
| LCD_CS | GPIO12 | **GPIO10** |
| LCD_SCLK | GPIO38 | **GPIO11** |
| LCD_SDIO0 | GPIO4 | **GPIO12** |
| LCD_SDIO1 | GPIO5 | **GPIO13** |
| LCD_SDIO2 | GPIO6 | **GPIO14** |
| LCD_SDIO3 | GPIO7 | **GPIO15** |
| Touch SDA | GPIO15 | **GPIO47** |
| Touch SCL | GPIO14 | **GPIO48** |
| Touch INT | GPIO11 | **GPIO6** |
| Touch RST | GPIO40 | **GPIO7** |

→ **Нельзя копировать pin_config.h 1:1. Пины разные.**
→ col_offset1 = 6 и размер 466×466 — общие, скопировать можно.

---

## Примеры в официальном репо 1.32"

Waveshare поставляет с платой:
1. `01_ADC_Test` — напряжение батареи
2. `02_WIFI_AP` / `03_WIFI_STA` — Wi-Fi
3. `04_BATT_PWR_Test` — кнопка питания
4. `05_Audio_Test` — микрофон и динамик
5. `06_LVGL_V8_Test` — UI с LVGL 8
6. `07_LVGL_V9_Test` — UI с LVGL 9

→ Когда плата приедет — **сначала прошить 01 и 05**, убедиться что UART и аудио живые.

---

## Решение по стеку для StarBoy

**Принято:** Arduino_GFX + LVGL 8.3.11 для UI-слоя.

**Для глазок — прямой рендер, не LVGL:**
LVGL хорош для кнопок/текста, но для плавной 30fps анимации глазок
лучше рисовать напрямую через `gfx->draw16bitRGBBitmap()` из PSRAM-буфера.
LVGL оставляем для вспомогательного UI (статус Wi-Fi, заряд батареи).

**Буферная стратегия для глазок:**
- Один полный кадр 466×466×2 = 434 KB — в PSRAM (для рендера)
- DMA-буфер 1/4 экрана (~106 KB) — в SRAM (для LVGL / flush)
- FreeRTOS задача рендера на Core 1, flush на Core 0
