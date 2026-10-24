#include "ui_model.h"

#include <assert.h>
#include <stdio.h>

static void test_short_list(void)
{
    ui_model_t model;
    ui_model_init(&model, 3);
    assert(model.selected == 0 && model.offset == 0);
    assert(!ui_model_move(&model, -1));
    assert(ui_model_move(&model, 1));
    assert(model.selected == 1 && model.offset == 0);
    assert(ui_model_move(&model, 10));
    assert(model.selected == 2 && model.offset == 0);
    assert(!ui_model_move(&model, 1));
}

static void test_scroll_window(void)
{
    ui_model_t model;
    ui_model_init(&model, 8);
    assert(model.selected == 0 && model.offset == 0);
    ui_model_move(&model, 3);
    assert(model.selected == 3 && model.offset == 0);
    ui_model_move(&model, 1);
    assert(model.selected == 4 && model.offset == 1);
    ui_model_move(&model, 10);
    assert(model.selected == 7 && model.offset == 4);
    ui_model_move(&model, -10);
    assert(model.selected == 0 && model.offset == 0);

    model.selected = 7;
    model.offset = 0;
    ui_model_set_count(&model, 8);
    assert(model.offset == 4);
}

static void test_shrink(void)
{
    ui_model_t model;
    ui_model_init(&model, 8);
    ui_model_move(&model, 7);
    assert(model.selected == 7 && model.offset == 4);
    ui_model_set_count(&model, 2);
    assert(model.selected == 1 && model.offset == 0);
    ui_model_set_count(&model, 0);
    assert(model.selected == 0 && model.offset == 0);
    assert(!ui_model_move(&model, 1));
}

int main(void)
{
    test_short_list();
    test_scroll_window();
    test_shrink();
    puts("ui model tests: PASS");
    return 0;
}
