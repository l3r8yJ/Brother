#if 1 /* Set this to "1" to enable content */

#ifndef LV_CONF_H
#define LV_CONF_H
#define LV_DRAW_SW_ASM LV_DRAW_SW_ASM_NONE
#ifndef __ASSEMBLY__
#include <stdint.h>
#endif

/* Color depth — RGB565 */
#define LV_COLOR_DEPTH 16

/* Memory */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64 * 1024U)

/* HAL — Arduino millis() as tick source */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Logging */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/* Widgets */
#define LV_USE_LABEL     1
#define LV_USE_IMAGE     1
#define LV_USE_ARC       1
#define LV_USE_CANVAS    1
#define LV_USE_ANIMIMG   0

/* Disable unused widgets */
#define LV_USE_BTNMATRIX  0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_COLORWHEEL 0
#define LV_USE_KEYBOARD   0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_SPINBOX    0
#define LV_USE_TABVIEW    0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

#endif /* LV_CONF_H */
#endif /* Enable content */
