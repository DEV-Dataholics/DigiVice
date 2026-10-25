/**
 * @file lv_conf.h
 * Configuration file for LVGL v9.6.0 (ESP32-2432S028 CYD)
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Use the system heap (malloc) so LVGL can use all free SRAM instead of a fixed pool */
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_BUILTIN

#define LV_COLOR_DEPTH 16
#define LV_DEF_REFR_PERIOD 33
#define LV_USE_OS LV_OS_NONE
#define LV_USE_LOG 0

/* Disable ALL on-screen debug overlays (FPS / CPU / memory monitors) */
#define LV_USE_SYSMON 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_PROFILER 0
#define LV_USE_REFR_DEBUG 0
#define LV_USE_LAYER_DEBUG 0
#define LV_USE_PARALLEL_DRAW_DEBUG 0

#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_DEFAULT &lv_font_montserrat_12

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1

/* Widgets */
#define LV_USE_ANIMIMG    1
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CALENDAR   0
#define LV_USE_CANVAS     1
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMAGE      1
#define LV_USE_IMAGEBUTTON 1
#define LV_USE_KEYBOARD   0
#define LV_USE_LABEL      1
#define LV_USE_LED        1
#define LV_USE_LINE       1
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SCALE      0
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

#endif /* LV_CONF_H */
