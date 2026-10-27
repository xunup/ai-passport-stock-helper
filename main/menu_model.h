// main/menu_model.h —— 设置菜单的选中项、编辑态与数值调整(纯逻辑)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define MENU_ITEM_COUNT 5

typedef enum {
    MENU_ITEM_HOTSPOT = 0,
    MENU_ITEM_BRIGHTNESS,
    MENU_ITEM_REFRESH,
    MENU_ITEM_AUTO_OFF,
    MENU_ITEM_AUTO_OFF_TIME,
} menu_item_t;

typedef enum {
    MENU_CHANGE_NONE = 0,
    MENU_CHANGE_SELECTION,
    MENU_CHANGE_BRIGHTNESS,
    MENU_CHANGE_REFRESH_INTERVAL,
    MENU_CHANGE_AUTO_OFF_TIME,
    MENU_CHANGE_AUTO_OFF_ENABLED,
} menu_change_t;

typedef enum {
    MENU_ACTIVATE_NONE = 0,
    MENU_ACTIVATE_TOGGLE_HOTSPOT,
    MENU_ACTIVATE_EDIT_BRIGHTNESS,
    MENU_ACTIVATE_EDIT_REFRESH_INTERVAL,
    MENU_ACTIVATE_TOGGLE_AUTO_OFF,
    MENU_ACTIVATE_EDIT_AUTO_OFF_TIME,
    MENU_ACTIVATE_CONFIRM,
} menu_activate_t;

typedef struct {
    uint8_t selected;
    bool editing;
    uint8_t brightness;
    uint16_t interval_sec;
    uint8_t auto_off_sec;
    bool auto_off_enabled;
    bool ap_enabled;
} menu_model_t;

void menu_model_init(menu_model_t *model, uint8_t brightness, uint16_t interval_sec,
                     uint8_t auto_off_sec, bool auto_off_enabled, bool ap_enabled);

// 非编辑态切换选中项;编辑态按方向调整数值。返回是否发生变化及变化类型。
menu_change_t menu_model_move(menu_model_t *model, int delta);

// OK 键:热点/自动息屏行切换开关,亮度/刷新间隔/息屏时间行进入或退出编辑态。
menu_activate_t menu_model_activate(menu_model_t *model);
