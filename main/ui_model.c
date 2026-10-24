// main/ui_model.c —— 4 行窗口跟随选中项滚动。
#include "ui_model.h"

static void clamp_window(ui_model_t *model)
{
    if (!model) return;
    if (model->count <= 0) {
        model->count = 0;
        model->selected = 0;
        model->offset = 0;
        return;
    }
    if (model->selected < 0) model->selected = 0;
    if (model->selected >= model->count) model->selected = model->count - 1;

    if (model->count <= UI_MODEL_ROWS) {
        model->offset = 0;
        return;
    }
    if (model->offset > model->count - UI_MODEL_ROWS) {
        model->offset = model->count - UI_MODEL_ROWS;
    }
    if (model->offset < 0) model->offset = 0;
    if (model->selected < model->offset) {
        model->offset = model->selected;
    } else if (model->selected >= model->offset + UI_MODEL_ROWS) {
        model->offset = model->selected - UI_MODEL_ROWS + 1;
    }
}

void ui_model_init(ui_model_t *model, int count)
{
    if (!model) return;
    model->count = count > 0 ? count : 0;
    model->selected = 0;
    model->offset = 0;
    clamp_window(model);
}

void ui_model_set_count(ui_model_t *model, int count)
{
    if (!model) return;
    model->count = count > 0 ? count : 0;
    clamp_window(model);
}

bool ui_model_move(ui_model_t *model, int delta)
{
    if (!model || model->count <= 0 || delta == 0) return false;
    const int before = model->selected;
    model->selected += delta;
    clamp_window(model);
    return model->selected != before;
}
