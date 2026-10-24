// main/ui_model.h —— 自选股列表的选中项与可视窗口(纯逻辑)。
#pragma once

#include <stdbool.h>

#define UI_MODEL_ROWS 4

typedef struct {
    int count;
    int selected;
    int offset;
} ui_model_t;

void ui_model_init(ui_model_t *model, int count);
void ui_model_set_count(ui_model_t *model, int count);
bool ui_model_move(ui_model_t *model, int delta);
