// main/ui_app.c —— 240x320 深色行情界面。与 demo 的像素菜单无共用布局。
#include "ui_app.h"

#include "bsp_display.h"
#include "menu_model.h"
#include "quote_format.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cn_16);
LV_FONT_DECLARE(lv_font_cn_22);
LV_FONT_DECLARE(lv_font_montserrat_12);
LV_FONT_DECLARE(lv_font_montserrat_20);

#define COLOR_BG 0x070B12
#define COLOR_PANEL 0x101824
#define COLOR_PANEL_SELECTED 0x1A2B45
#define COLOR_MENU_ROW_SELECTED 0x1E4C7D
#define COLOR_MENU_ROW_EDITING 0x14532D
#define COLOR_EDIT 0x2FD07A
#define COLOR_SWITCH_OFF 0x2A3A50
#define COLOR_LINE 0x1E2A3A
#define COLOR_TEXT 0xE6EDF3
#define COLOR_MUTED 0x8A97A8
#define COLOR_ACCENT 0x4A9EFF
#define COLOR_UP 0xFF5A5F
#define COLOR_DOWN 0x2FD07A
#define COLOR_FLAT 0x9AA7B8

#define ROW_COUNT 4
#define ROW_HEIGHT 54
#define ROW_GAP 4
#define ROW_TOP 48
#define ROW_WIDTH 216
#define ROW_LEFT 12
#define RIGHT_LEFT 104
#define RIGHT_WIDTH 108

#define MENU_ROW_COUNT MENU_ITEM_COUNT
#define MENU_ROW_TOP 50
#define MENU_ROW_HEIGHT 50
#define MENU_ROW_GAP 5
#define MENU_ROW_WIDTH 216
#define MENU_ROW_LEFT 12
#define MENU_ROW_TEXT_LEFT 16
#define MENU_ROW_VALUE_LEFT 90
#define MENU_ROW_VALUE_WIDTH 110
#define SWITCH_WIDTH 44
#define SWITCH_HEIGHT 24
#define SWITCH_LEFT 156
#define SWITCH_KNOB 18
#define BRIGHTNESS_BAR_COUNT 5

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *wifi;
    lv_obj_t *battery_body;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_label;
} chrome_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *name;
    lv_obj_t *code;
    lv_obj_t *price;
    lv_obj_t *change;
} row_widgets_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *marker;
    lv_obj_t *name;
    lv_obj_t *sub;
    lv_obj_t *value;
    lv_obj_t *track;
    lv_obj_t *knob;
    lv_obj_t *bars[BRIGHTNESS_BAR_COUNT];
} menu_row_widgets_t;

static chrome_t s_list_chrome;
static chrome_t s_setup_chrome;
static chrome_t s_menu_chrome;
static row_widgets_t s_rows[ROW_COUNT];
static menu_row_widgets_t s_menu_rows[MENU_ROW_COUNT];
static lv_obj_t *s_list_status;
static lv_obj_t *s_setup_ap;
static lv_obj_t *s_setup_status;
static lv_obj_t *s_setup_hint;
static lv_obj_t *s_menu_hint;
static lv_obj_t *s_active;

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                            int x, int y, int width, int height, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(label, text ? text : "");
    return label;
}

static lv_obj_t *make_box(lv_obj_t *parent, int x, int y, int width, int height,
                          uint32_t color, int radius)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_pos(box, x, y);
    lv_obj_set_size(box, width, height);
    lv_obj_set_style_bg_color(box, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(box, radius, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    return box;
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    return screen;
}

static void build_header(chrome_t *chrome, const char *title)
{
    make_label(chrome->screen, &lv_font_cn_22, COLOR_TEXT, 12, 5, 120, 34, title);
    chrome->wifi = make_label(chrome->screen, &lv_font_montserrat_20, COLOR_MUTED,
                              136, 9, 32, 26, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_align(chrome->wifi, LV_TEXT_ALIGN_CENTER, 0);
    chrome->battery_body = make_box(chrome->screen, 174, 17, 22, 12, COLOR_BG, 2);
    lv_obj_set_style_border_width(chrome->battery_body, 1, 0);
    lv_obj_set_style_border_color(chrome->battery_body, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_border_opa(chrome->battery_body, LV_OPA_COVER, 0);
    make_box(chrome->screen, 197, 20, 3, 6, COLOR_MUTED, 1);
    chrome->battery_fill = make_box(chrome->battery_body, 2, 2, 18, 8, COLOR_DOWN, 1);
    chrome->battery_label = make_label(chrome->screen, &lv_font_montserrat_12, COLOR_TEXT,
                                       200, 11, 28, 18, "--");
    lv_obj_set_style_text_align(chrome->battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    make_box(chrome->screen, 12, 42, 216, 1, COLOR_LINE, 0);
}

void ui_app_init(void)
{
    if (!bsp_lvgl_lock(1000)) return;

    s_list_chrome.screen = make_screen();
    s_setup_chrome.screen = make_screen();
    build_header(&s_list_chrome, "自选股");
    build_header(&s_setup_chrome, "股票盯盘");

    for (int i = 0; i < ROW_COUNT; i++) {
        row_widgets_t *row = &s_rows[i];
        row->row = make_box(s_list_chrome.screen, ROW_LEFT, ROW_TOP + i * (ROW_HEIGHT + ROW_GAP),
                            ROW_WIDTH, ROW_HEIGHT, COLOR_BG, 8);
        lv_obj_set_style_border_width(row->row, 1, 0);
        lv_obj_set_style_border_color(row->row, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_border_opa(row->row, LV_OPA_TRANSP, 0);
        row->name = make_label(row->row, &lv_font_cn_16, COLOR_TEXT, 10, 2, 88, 24, "");
        lv_label_set_long_mode(row->name, LV_LABEL_LONG_DOT);
        row->code = make_label(row->row, &lv_font_montserrat_12, COLOR_MUTED, 10, 28, 88, 18, "");
        lv_label_set_long_mode(row->code, LV_LABEL_LONG_DOT);
        row->price = make_label(row->row, &lv_font_montserrat_20, COLOR_TEXT,
                                RIGHT_LEFT, 1, RIGHT_WIDTH, 24, "");
        lv_obj_set_style_text_align(row->price, LV_TEXT_ALIGN_RIGHT, 0);
        row->change = make_label(row->row, &lv_font_montserrat_12, COLOR_FLAT,
                                 RIGHT_LEFT, 30, RIGHT_WIDTH, 16, "");
        lv_obj_set_style_text_align(row->change, LV_TEXT_ALIGN_RIGHT, 0);
    }

    s_list_status = make_label(s_list_chrome.screen, &lv_font_cn_16, COLOR_MUTED,
                               12, 286, 112, 24, "等待行情");
    lv_label_set_long_mode(s_list_status, LV_LABEL_LONG_DOT);
    lv_obj_t *settings_hint = make_label(s_list_chrome.screen, &lv_font_cn_16, COLOR_ACCENT,
                                         124, 286, 104, 24, "长按 OK 设置");
    lv_obj_set_style_text_align(settings_hint, LV_TEXT_ALIGN_RIGHT, 0);

    // 配网引导卡:只讲三步,不复制 demo 的菜单/像素风。
    lv_obj_t *card = make_box(s_setup_chrome.screen, 12, 50, 216, 200, COLOR_PANEL, 12);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_LINE), 0);
    lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);

    make_label(card, &lv_font_cn_16, COLOR_MUTED, 12, 10, 192, 24, "连接热点");
    s_setup_ap = make_label(card, &lv_font_montserrat_20, COLOR_TEXT, 12, 32, 192, 26, "--");
    make_box(card, 12, 64, 192, 1, COLOR_LINE, 0);
    make_label(card, &lv_font_cn_16, COLOR_MUTED, 12, 72, 192, 24, "打开网页");
    make_label(card, &lv_font_montserrat_20, COLOR_ACCENT, 12, 94, 192, 26, "192.168.4.1");
    lv_obj_t *steps = make_label(card, &lv_font_cn_16, COLOR_TEXT, 12, 124, 192, 76,
                                 "选择 Wi-Fi、填写密码并添加自选股，保存后自动刷新行情。");
    lv_label_set_long_mode(steps, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(steps, 2, 0);

    s_setup_status = make_label(s_setup_chrome.screen, &lv_font_cn_16, COLOR_MUTED,
                                12, 258, 216, 24, "等待配置");
    s_setup_hint = make_label(s_setup_chrome.screen, &lv_font_cn_16, COLOR_ACCENT,
                              12, 284, 216, 24, "");
    lv_obj_set_style_text_align(s_setup_hint, LV_TEXT_ALIGN_LEFT, 0);

    // 设置菜单:四行卡片,选中行整块高亮并带左侧强调条;开关行用拨动开关。
    s_menu_chrome.screen = make_screen();
    build_header(&s_menu_chrome, "设置");

    static const char *const menu_names[MENU_ROW_COUNT] = {
        "热点开关", "屏幕亮度", "自动息屏", "息屏时间",
    };
    static const char *const menu_subs[MENU_ROW_COUNT] = {
        "192.168.4.1", "", "屏幕到时自动熄灭", "5-60 秒",
    };
    for (int i = 0; i < MENU_ROW_COUNT; i++) {
        menu_row_widgets_t *row = &s_menu_rows[i];
        row->row = make_box(s_menu_chrome.screen, MENU_ROW_LEFT,
                            MENU_ROW_TOP + i * (MENU_ROW_HEIGHT + MENU_ROW_GAP),
                            MENU_ROW_WIDTH, MENU_ROW_HEIGHT, COLOR_PANEL, 10);
        lv_obj_set_style_border_width(row->row, 2, 0);
        lv_obj_set_style_border_color(row->row, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_border_opa(row->row, LV_OPA_COVER, 0);

        row->marker = make_box(row->row, 6, 10, 4, 30, COLOR_ACCENT, 2);
        lv_obj_add_flag(row->marker, LV_OBJ_FLAG_HIDDEN);

        row->name = make_label(row->row, &lv_font_cn_16, COLOR_MUTED, MENU_ROW_TEXT_LEFT, 4,
                               120, 22, menu_names[i]);
        row->sub = make_label(row->row, &lv_font_cn_16, COLOR_MUTED, MENU_ROW_TEXT_LEFT, 26,
                              180, 18, menu_subs[i]);
        row->value = make_label(row->row, &lv_font_cn_16, COLOR_TEXT, MENU_ROW_VALUE_LEFT, 4,
                                MENU_ROW_VALUE_WIDTH, 22, "");
        lv_obj_set_style_text_align(row->value, LV_TEXT_ALIGN_RIGHT, 0);

        if (i == MENU_ITEM_BRIGHTNESS) {
            for (int bar = 0; bar < BRIGHTNESS_BAR_COUNT; bar++) {
                row->bars[bar] = make_box(row->row, MENU_ROW_TEXT_LEFT + bar * 20, 36, 16, 6,
                                          COLOR_LINE, 3);
            }
        } else if (i == MENU_ITEM_HOTSPOT || i == MENU_ITEM_AUTO_OFF) {
            row->track = make_box(row->row, SWITCH_LEFT, (MENU_ROW_HEIGHT - SWITCH_HEIGHT) / 2,
                                  SWITCH_WIDTH, SWITCH_HEIGHT, COLOR_SWITCH_OFF,
                                  SWITCH_HEIGHT / 2);
            row->knob = make_box(row->track, 3, 3, SWITCH_KNOB, SWITCH_KNOB,
                                 COLOR_MUTED, SWITCH_KNOB / 2);
        }
    }

    make_label(s_menu_chrome.screen, &lv_font_cn_16, COLOR_MUTED, 12, 270, 216, 20,
               "长按 OK 关闭菜单");
    s_menu_hint = make_label(s_menu_chrome.screen, &lv_font_cn_16, COLOR_ACCENT,
                             12, 292, 216, 22, "上下选择  OK 调整");

    s_active = s_list_chrome.screen;
    lv_screen_load(s_active);
    bsp_lvgl_unlock();
}

static void set_wifi(chrome_t *chrome, bool connected)
{
    lv_obj_set_style_text_color(chrome->wifi,
                                lv_color_hex(connected ? COLOR_ACCENT : COLOR_MUTED), 0);
}

static void set_battery(chrome_t *chrome, int pct, bool ok)
{
    if (!ok || pct < 0) {
        lv_obj_set_style_bg_color(chrome->battery_fill, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_width(chrome->battery_fill, 18);
        lv_label_set_text(chrome->battery_label, "--");
        return;
    }
    if (pct > 100) pct = 100;
    lv_obj_set_width(chrome->battery_fill, pct > 0 ? (18 * pct) / 100 + 1 : 0);
    const uint32_t color = pct <= 20 ? COLOR_UP : (pct <= 50 ? 0xF5C542 : COLOR_DOWN);
    lv_obj_set_style_bg_color(chrome->battery_fill, lv_color_hex(color), 0);
    lv_label_set_text_fmt(chrome->battery_label, "%d%%", pct);
}

void ui_app_show_list(void)
{
    if (!s_list_chrome.screen) return;
    if (!bsp_lvgl_lock(200)) return;
    if (s_active != s_list_chrome.screen) {
        s_active = s_list_chrome.screen;
        lv_screen_load(s_active);
    }
    bsp_lvgl_unlock();
}

void ui_app_show_setup(void)
{
    if (!s_setup_chrome.screen) return;
    if (!bsp_lvgl_lock(200)) return;
    if (s_active != s_setup_chrome.screen) {
        s_active = s_setup_chrome.screen;
        lv_screen_load(s_active);
    }
    bsp_lvgl_unlock();
}

void ui_app_show_menu(void)
{
    if (!s_menu_chrome.screen) return;
    if (!bsp_lvgl_lock(200)) return;
    if (s_active != s_menu_chrome.screen) {
        s_active = s_menu_chrome.screen;
        lv_screen_load(s_active);
    }
    bsp_lvgl_unlock();
}

static void set_row_selected(int index, bool selected)
{
    lv_obj_set_style_bg_color(s_rows[index].row,
                              lv_color_hex(selected ? COLOR_PANEL_SELECTED : COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_rows[index].row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(s_rows[index].row, selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(s_rows[index].row, lv_color_hex(COLOR_ACCENT), 0);
}

void ui_app_update_list(const ui_list_view_t *view)
{
    if (!view || !view->config || !view->quotes) return;
    if (!bsp_lvgl_lock(300)) return;

    const quote_snapshot_t *snapshot = view->quotes;
    for (int i = 0; i < ROW_COUNT; i++) {
        const int index = view->offset + i;
        row_widgets_t *row = &s_rows[i];
        if (index >= view->config->count) {
            lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
            set_row_selected(i, false);
            continue;
        }
        lv_obj_clear_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        set_row_selected(i, index == view->selected);

        const char *code = view->config->codes[index];
        const quote_t *quote = quote_list_find(snapshot->quotes, (size_t)snapshot->count, code);
        char price_text[24] = "--";
        char change_text[40] = "--";
        uint32_t color = COLOR_MUTED;

        if (quote && quote->valid) {
            const int decimals = quote_price_decimals(code);
            char amount[16];
            char pct[16];
            quote_format_price(price_text, sizeof(price_text), code, quote->price);
            quote_format_signed(amount, sizeof(amount), quote->change, decimals);
            quote_format_percent(pct, sizeof(pct), quote->change_pct);
            snprintf(change_text, sizeof(change_text), "%s %s", amount, pct);
            const quote_trend_t trend = quote_trend(quote->change);
            color = trend == QUOTE_TREND_UP ? COLOR_UP :
                    (trend == QUOTE_TREND_DOWN ? COLOR_DOWN : COLOR_FLAT);
        }

        const char *name = quote && quote->name[0] ? quote->name : "等待行情";
        lv_label_set_text(row->name, view->config->count == 0 ? "尚未添加自选股" : name);
        lv_label_set_text(row->code, code);
        lv_label_set_text(row->price, price_text);
        lv_obj_set_style_text_color(row->price, lv_color_hex(quote && quote->valid ? color : COLOR_MUTED), 0);
        lv_label_set_text(row->change, change_text);
        lv_obj_set_style_text_color(row->change, lv_color_hex(quote && quote->valid ? color : COLOR_MUTED), 0);
    }

    if (!view->wifi_connected) {
        lv_label_set_text(s_list_status, "连接网络...");
    } else if (snapshot->error[0]) {
        lv_label_set_text(s_list_status, snapshot->error);
    } else if (snapshot->fetching && !snapshot->has_data) {
        lv_label_set_text(s_list_status, "获取行情...");
    } else {
        lv_label_set_text_fmt(s_list_status, "%u秒刷新", (unsigned)view->config->interval_sec);
    }
    set_wifi(&s_list_chrome, view->wifi_connected);
    set_battery(&s_list_chrome, view->battery_pct, view->battery_ok);
    bsp_lvgl_unlock();
}

void ui_app_update_setup(const ui_setup_view_t *view)
{
    if (!view) return;
    if (!bsp_lvgl_lock(300)) return;
    lv_label_set_text(s_setup_ap, view->ap_ssid ? view->ap_ssid : "--");
    lv_label_set_text(s_setup_status, view->status ? view->status : "");
    lv_label_set_text(s_setup_hint, view->hint ? view->hint : "");
    set_wifi(&s_setup_chrome, view->wifi_connected);
    set_battery(&s_setup_chrome, view->battery_pct, view->battery_ok);
    bsp_lvgl_unlock();
}

static void set_menu_switch(menu_row_widgets_t *row, bool on)
{
    if (!row->track) return;
    lv_obj_set_style_bg_color(row->track,
                              lv_color_hex(on ? COLOR_ACCENT : COLOR_SWITCH_OFF), 0);
    lv_obj_set_style_bg_color(row->knob, lv_color_hex(on ? 0xFFFFFF : COLOR_MUTED), 0);
    lv_obj_set_pos(row->knob, on ? SWITCH_WIDTH - SWITCH_KNOB - 3 : 3, 3);
}

static void set_menu_row_selected(int index, bool selected, bool editing)
{
    menu_row_widgets_t *row = &s_menu_rows[index];
    const uint32_t bg = editing ? COLOR_MENU_ROW_EDITING
                                : (selected ? COLOR_MENU_ROW_SELECTED : COLOR_PANEL);
    lv_obj_set_style_bg_color(row->row, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(row->row,
                                  lv_color_hex(editing ? COLOR_EDIT
                                                       : (selected ? COLOR_ACCENT : COLOR_LINE)), 0);
    lv_obj_set_style_bg_color(row->marker,
                              lv_color_hex(editing ? COLOR_EDIT : COLOR_ACCENT), 0);
    if (selected) {
        lv_obj_clear_flag(row->marker, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(row->marker, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_text_color(row->name,
                                lv_color_hex(selected ? COLOR_TEXT : COLOR_MUTED), 0);
}

void ui_app_update_menu(const ui_menu_view_t *view)
{
    if (!view) return;
    if (!bsp_lvgl_lock(300)) return;

    int brightness = view->brightness;
    if (brightness < 0) brightness = 0;
    if (brightness > BRIGHTNESS_BAR_COUNT) brightness = BRIGHTNESS_BAR_COUNT;

    for (int i = 0; i < MENU_ROW_COUNT; i++) {
        menu_row_widgets_t *row = &s_menu_rows[i];
        const bool selected = i == view->selected;
        const bool editing = selected && view->editing;
        set_menu_row_selected(i, selected, editing);

        uint32_t value_color = selected ? COLOR_TEXT : COLOR_MUTED;
        bool show_value = true;
        char value[16] = "";

        if (i == MENU_ITEM_HOTSPOT) {
            show_value = false;
            set_menu_switch(row, view->ap_enabled);
            lv_label_set_text(row->sub, view->ap_enabled ? "192.168.4.1" : "网页配置不可用");
        } else if (i == MENU_ITEM_BRIGHTNESS) {
            snprintf(value, sizeof(value), "%d / %d", brightness, BRIGHTNESS_BAR_COUNT);
            const uint32_t bar_on = editing ? COLOR_EDIT : COLOR_ACCENT;
            for (int bar = 0; bar < BRIGHTNESS_BAR_COUNT; bar++) {
                lv_obj_set_style_bg_color(row->bars[bar],
                                          lv_color_hex(bar < brightness ? bar_on : COLOR_LINE), 0);
            }
        } else if (i == MENU_ITEM_AUTO_OFF) {
            show_value = false;
            set_menu_switch(row, view->auto_off_enabled);
            lv_label_set_text(row->sub, view->auto_off_enabled ? "屏幕到时自动熄灭"
                                                               : "屏幕保持常亮");
        } else {
            snprintf(value, sizeof(value), "%d 秒", view->auto_off_sec);
            if (!view->auto_off_enabled) value_color = COLOR_MUTED;
        }

        lv_label_set_text(row->value, show_value ? value : "");
        lv_obj_set_style_text_color(row->value,
                                    lv_color_hex(editing ? COLOR_EDIT : value_color), 0);
    }

    lv_label_set_text(s_menu_hint, view->editing ? "上下调整  OK 完成"
                                                 : "上下选择  OK 调整");
    set_wifi(&s_menu_chrome, view->wifi_connected);
    set_battery(&s_menu_chrome, view->battery_pct, view->battery_ok);
    bsp_lvgl_unlock();
}
