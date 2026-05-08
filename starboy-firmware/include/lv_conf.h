#if 1 /* Set this to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color depth: 1, 8, 16, 32 */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color — needed for most SPI/QSPI displays */
#define LV_COLOR_16_SWAP 1

/* Memory settings */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (48 * 1024U) /* 48 KB internal LVGL heap */

/* HAL */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Display resolution (max, actual set at runtime) */
#define LV_HOR_RES_MAX 466
#define LV_VER_RES_MAX 466

/* Logging */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Fonts — enable what we need */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Widgets we'll use */
#define LV_USE_LABEL  1
#define LV_USE_IMG    1
#define LV_USE_ARC    1
#define LV_USE_ANIMIMG 1

/* Animation */
#define LV_USE_ANIMATION 1

/* Disable unused widgets to save flash */
#define LV_USE_BTNMATRIX 0
#define LV_USE_CALENDAR  0
#define LV_USE_CHART     0
#define LV_USE_COLORWHEEL 0
#define LV_USE_KEYBOARD  0
#define LV_USE_LIST      0
#define LV_USE_MENU      0
#define LV_USE_METER     0
#define LV_USE_MSGBOX    0
#define LV_USE_SPINBOX   0
#define LV_USE_TABVIEW   0
#define LV_USE_TILEVIEW  0
#define LV_USE_WIN       0

#endif /* LV_CONF_H */
#endif /* Enable content */
