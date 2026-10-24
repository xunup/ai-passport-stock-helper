// main/menu_model.c —— 三行设置菜单的纯逻辑,不依赖 LVGL/ESP-IDF。
#include "menu_model.h"

#include "settings_limits.h"

static uint8_t clamp_level(int value)
{
    if (value < SETTINGS_BRIGHTNESS_MIN) return SETTINGS_BRIGHTNESS_MIN;
    if (value > SETTINGS_BRIGHTNESS_MAX) return SETTINGS_BRIGHTNESS_MAX;
    return (uint8_t)value;
}

static uint8_t clamp_auto_off(int value)
{
    if (value < SETTINGS_AUTO_OFF_MIN) return SETTINGS_AUTO_OFF_MIN;
    if (value > SETTINGS_AUTO_OFF_MAX) return SETTINGS_AUTO_OFF_MAX;
    return (uint8_t)((value / SETTINGS_AUTO_OFF_STEP) * SETTINGS_AUTO_OFF_STEP);
}

void menu_model_init(menu_model_t *model, uint8_t brightness, uint8_t auto_off_sec,
                     bool ap_enabled)
{
    if (!model) return;
    model->selected = MENU_ITEM_HOTSPOT;
    model->editing = false;
    model->brightness = clamp_level(brightness);
    model->auto_off_sec = clamp_auto_off(auto_off_sec);
    model->ap_enabled = ap_enabled;
}

menu_change_t menu_model_move(menu_model_t *model, int delta)
{
    if (!model || delta == 0) return MENU_CHANGE_NONE;

    if (model->editing) {
        if (model->selected == MENU_ITEM_BRIGHTNESS) {
            const uint8_t before = model->brightness;
            model->brightness = clamp_level((int)model->brightness + delta);
            return model->brightness != before ? MENU_CHANGE_BRIGHTNESS : MENU_CHANGE_NONE;
        }
        if (model->selected == MENU_ITEM_AUTO_OFF) {
            const uint8_t before = model->auto_off_sec;
            model->auto_off_sec = clamp_auto_off(
                (int)model->auto_off_sec + delta * SETTINGS_AUTO_OFF_STEP);
            return model->auto_off_sec != before ? MENU_CHANGE_AUTO_OFF : MENU_CHANGE_NONE;
        }
        return MENU_CHANGE_NONE;
    }

    int next = (int)model->selected + delta;
    if (next < 0) next = 0;
    if (next >= MENU_ITEM_COUNT) next = MENU_ITEM_COUNT - 1;
    if (next == model->selected) return MENU_CHANGE_NONE;
    model->selected = (uint8_t)next;
    return MENU_CHANGE_SELECTION;
}

menu_activate_t menu_model_activate(menu_model_t *model)
{
    if (!model) return MENU_ACTIVATE_NONE;
    if (model->editing) {
        model->editing = false;
        return MENU_ACTIVATE_CONFIRM;
    }
    if (model->selected == MENU_ITEM_HOTSPOT) {
        model->ap_enabled = !model->ap_enabled;
        return MENU_ACTIVATE_TOGGLE_HOTSPOT;
    }
    if (model->selected == MENU_ITEM_BRIGHTNESS) {
        model->editing = true;
        return MENU_ACTIVATE_EDIT_BRIGHTNESS;
    }
    if (model->selected == MENU_ITEM_AUTO_OFF) {
        model->editing = true;
        return MENU_ACTIVATE_EDIT_AUTO_OFF;
    }
    return MENU_ACTIVATE_NONE;
}
