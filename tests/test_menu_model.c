#include "menu_model.h"

#include <assert.h>
#include <stdio.h>

static void test_init_clamps(void)
{
    menu_model_t model;
    menu_model_init(&model, 0, 7, 7, true, true);
    assert(model.selected == MENU_ITEM_HOTSPOT);
    assert(!model.editing);
    assert(model.brightness == 1);
    assert(model.interval_sec == 5);
    assert(model.auto_off_sec == 5);
    assert(model.auto_off_enabled);
    assert(model.ap_enabled);

    menu_model_init(&model, 9, 0, 61, false, false);
    assert(model.brightness == 5);
    assert(model.interval_sec == 0);
    assert(model.auto_off_sec == 60);
    assert(!model.auto_off_enabled);
    assert(!model.ap_enabled);
}

static void test_selection(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, true, true);

    assert(menu_model_move(&model, -1) == MENU_CHANGE_NONE);
    assert(model.selected == MENU_ITEM_HOTSPOT);

    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_BRIGHTNESS);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_REFRESH);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_AUTO_OFF);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_AUTO_OFF_TIME);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_NONE);
    assert(model.selected == MENU_ITEM_AUTO_OFF_TIME);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_AUTO_OFF);
}

static void test_hotspot_toggle(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, true, true);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_HOTSPOT);
    assert(!model.ap_enabled);
    assert(!model.editing);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_HOTSPOT);
    assert(model.ap_enabled);
}

static void test_auto_off_toggle(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, true, true);
    menu_model_move(&model, 3);
    assert(model.selected == MENU_ITEM_AUTO_OFF);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_AUTO_OFF);
    assert(!model.auto_off_enabled);
    assert(!model.editing);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_AUTO_OFF);
    assert(model.auto_off_enabled);
}

static void test_brightness_edit(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, true, true);
    menu_model_move(&model, 1);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_EDIT_BRIGHTNESS);
    assert(model.editing);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_BRIGHTNESS);
    assert(model.brightness == 4);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_BRIGHTNESS);
    assert(model.brightness == 5);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_NONE);
    assert(model.brightness == 5);
    assert(menu_model_move(&model, -10) == MENU_CHANGE_BRIGHTNESS);
    assert(model.brightness == 1);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_CONFIRM);
    assert(!model.editing);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_REFRESH);
}

static void test_refresh_interval_edit(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, true, true);
    menu_model_move(&model, 2);
    assert(model.selected == MENU_ITEM_REFRESH);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_EDIT_REFRESH_INTERVAL);
    assert(model.editing);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 10);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 5);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 3);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 1);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 0);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_NONE);
    assert(model.interval_sec == 0);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 1);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_CONFIRM);
    assert(!model.editing);
}

static void test_refresh_interval_bounds(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 300, 30, true, true);
    menu_model_move(&model, 2);
    menu_model_activate(&model);
    assert(menu_model_move(&model, 10) == MENU_CHANGE_NONE);
    assert(model.interval_sec == 300);
    assert(menu_model_move(&model, -100) == MENU_CHANGE_REFRESH_INTERVAL);
    assert(model.interval_sec == 0);
}

static void test_auto_off_time_edit_steps(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 5, 30, false, true);
    menu_model_move(&model, 4);
    assert(model.selected == MENU_ITEM_AUTO_OFF_TIME);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_EDIT_AUTO_OFF_TIME);
    assert(model.editing);

    assert(menu_model_move(&model, 1) == MENU_CHANGE_AUTO_OFF_TIME);
    assert(model.auto_off_sec == 35);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_AUTO_OFF_TIME);
    assert(model.auto_off_sec == 40);
    assert(menu_model_move(&model, -10) == MENU_CHANGE_AUTO_OFF_TIME);
    assert(model.auto_off_sec == 5);
    assert(menu_model_move(&model, 20) == MENU_CHANGE_AUTO_OFF_TIME);
    assert(model.auto_off_sec == 60);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_NONE);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_CONFIRM);
    assert(!model.editing);
}

static void test_uninitialized_inputs(void)
{
    assert(menu_model_move(NULL, 1) == MENU_CHANGE_NONE);
    assert(menu_model_activate(NULL) == MENU_ACTIVATE_NONE);
    menu_model_init(NULL, 3, 5, 30, true, true);
}

int main(void)
{
    test_init_clamps();
    test_selection();
    test_hotspot_toggle();
    test_auto_off_toggle();
    test_brightness_edit();
    test_refresh_interval_edit();
    test_refresh_interval_bounds();
    test_auto_off_time_edit_steps();
    test_uninitialized_inputs();
    puts("menu model tests: PASS");
    return 0;
}
