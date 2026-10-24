#include "menu_model.h"

#include <assert.h>
#include <stdio.h>

static void test_init_clamps(void)
{
    menu_model_t model;
    menu_model_init(&model, 0, 7, true);
    assert(model.selected == MENU_ITEM_HOTSPOT);
    assert(!model.editing);
    assert(model.brightness == 1);
    assert(model.auto_off_sec == 5);

    menu_model_init(&model, 9, 61, false);
    assert(model.brightness == 5);
    assert(model.auto_off_sec == 60);
    assert(!model.ap_enabled);
}

static void test_selection(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 30, true);

    assert(menu_model_move(&model, -1) == MENU_CHANGE_NONE);
    assert(model.selected == MENU_ITEM_HOTSPOT);

    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_BRIGHTNESS);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_AUTO_OFF);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_NONE);
    assert(model.selected == MENU_ITEM_AUTO_OFF);
    assert(menu_model_move(&model, -1) == MENU_CHANGE_SELECTION);
    assert(model.selected == MENU_ITEM_BRIGHTNESS);
}

static void test_hotspot_toggle(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 30, true);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_HOTSPOT);
    assert(!model.ap_enabled);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_TOGGLE_HOTSPOT);
    assert(model.ap_enabled);
}

static void test_brightness_edit(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 30, true);
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
    assert(model.selected == MENU_ITEM_AUTO_OFF);
}

static void test_auto_off_edit_steps(void)
{
    menu_model_t model;
    menu_model_init(&model, 3, 30, true);
    menu_model_move(&model, 2);
    assert(menu_model_activate(&model) == MENU_ACTIVATE_EDIT_AUTO_OFF);
    assert(model.editing);

    assert(menu_model_move(&model, 1) == MENU_CHANGE_AUTO_OFF);
    assert(model.auto_off_sec == 35);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_AUTO_OFF);
    assert(model.auto_off_sec == 40);
    assert(menu_model_move(&model, -10) == MENU_CHANGE_AUTO_OFF);
    assert(model.auto_off_sec == 5);
    assert(menu_model_move(&model, 20) == MENU_CHANGE_AUTO_OFF);
    assert(model.auto_off_sec == 60);
    assert(menu_model_move(&model, 1) == MENU_CHANGE_NONE);

    assert(menu_model_activate(&model) == MENU_ACTIVATE_CONFIRM);
    assert(!model.editing);
}

static void test_uninitialized_inputs(void)
{
    assert(menu_model_move(NULL, 1) == MENU_CHANGE_NONE);
    assert(menu_model_activate(NULL) == MENU_ACTIVATE_NONE);
    menu_model_init(NULL, 3, 30, true);
}

int main(void)
{
    test_init_clamps();
    test_selection();
    test_hotspot_toggle();
    test_brightness_edit();
    test_auto_off_edit_steps();
    test_uninitialized_inputs();
    puts("menu model tests: PASS");
    return 0;
}
