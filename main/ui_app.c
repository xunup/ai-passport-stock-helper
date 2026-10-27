// main/ui_app.c —— 240x320 深色行情界面。与 demo 的像素菜单无共用布局。
#include "ui_app.h"

#include "bsp_display.h"
#include "menu_model.h"
#include "quote_format.h"
#include "refresh_options.h"
#include "stock_positions.h"
#include "time_app.h"

#include "esp_log.h"
#include "esp_system.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cn_12_pnl);
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
#define COLOR_WARN 0xF5C542
#define COLOR_UP 0xFF5A5F
#define COLOR_DOWN 0x2FD07A
#define COLOR_FLAT 0x9AA7B8

#define ROW_COUNT 4
#define ROW_HEIGHT 58
#define ROW_GAP 3
#define ROW_TOP 46
#define ROW_WIDTH 216
#define ROW_LEFT 12
#define RIGHT_LEFT 96
#define RIGHT_WIDTH 120
#define PNL_LEFT 72
#define PNL_WIDTH 144

#define MENU_ROW_COUNT MENU_ITEM_COUNT
#define MENU_VISIBLE_ROWS 4
#define MENU_ROW_TOP 50
#define MENU_ROW_HEIGHT 50
#define MENU_ROW_GAP 5
#define MENU_ROW_WIDTH 216
#define MENU_ROW_LEFT 12
#define MENU_ROW_TEXT_LEFT 16
#define MENU_ROW_VALUE_LEFT 90
#define MENU_ROW_VALUE_WIDTH 110
#define MENU_SCROLL_LEFT 232
#define MENU_SCROLL_WIDTH 4
#define MENU_TRACK_HEIGHT (MENU_VISIBLE_ROWS * MENU_ROW_HEIGHT + \
                           (MENU_VISIBLE_ROWS - 1) * MENU_ROW_GAP)
#define MENU_THUMB_HEIGHT (MENU_TRACK_HEIGHT * MENU_VISIBLE_ROWS / MENU_ROW_COUNT)
#define SWITCH_WIDTH 44
#define SWITCH_HEIGHT 24
#define SWITCH_LEFT 156
#define SWITCH_KNOB 18
#define BRIGHTNESS_BAR_COUNT 5
#define WIFI_BLINK_PERIOD_MS 500
#define CLOCK_PERIOD_MS 1000
#define WIFI_BAR_COUNT 4
#define WIFI_BAR_WIDTH 4
#define WIFI_BAR_GAP 2
#define WIFI_BAR_LEFT 168
#define WIFI_BAR_BOTTOM 27
#define BATTERY_LABEL_LEFT 194
#define BATTERY_LABEL_WIDTH 34

typedef struct {
    lv_obj_t *screen;
    lv_obj_t *clock;
    lv_obj_t *wifi_bars[WIFI_BAR_COUNT];
    bool wifi_connecting;
    lv_obj_t *battery_label;
} chrome_t;

typedef struct {
    lv_obj_t *row;
    lv_obj_t *name;
    lv_obj_t *code;
    lv_obj_t *price;
    lv_obj_t *change;
    lv_obj_t *pnl;
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
static lv_obj_t *s_list_hint;
static lv_obj_t *s_list_refresh;
static lv_obj_t *s_setup_ap;
static lv_obj_t *s_setup_status;
static lv_obj_t *s_setup_hint;
static lv_obj_t *s_menu_hint;
static lv_obj_t *s_menu_scroll_thumb;
static lv_obj_t *s_active;
static int s_menu_offset;
static char s_clock_text[6] = "--:--";

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
    const bool has_title = title && title[0];
    if (has_title) {
        make_label(chrome->screen, &lv_font_cn_22, COLOR_TEXT, 12, 5, 68, 34, title);
    }
    chrome->clock = make_label(chrome->screen, &lv_font_montserrat_20, COLOR_MUTED,
                               has_title ? 80 : 12, 9, 56, 26, "--:--");
    lv_obj_set_style_text_align(chrome->clock,
                                has_title ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);

    // 四格信号:点亮格数按 RSSI 分档;格底对齐时钟字形底部。
    static const int bar_heights[WIFI_BAR_COUNT] = {6, 9, 12, 15};
    for (int i = 0; i < WIFI_BAR_COUNT; i++) {
        chrome->wifi_bars[i] = make_box(chrome->screen,
                                        WIFI_BAR_LEFT + i * (WIFI_BAR_WIDTH + WIFI_BAR_GAP),
                                        WIFI_BAR_BOTTOM - bar_heights[i],
                                        WIFI_BAR_WIDTH, bar_heights[i], COLOR_MUTED, 1);
    }

    chrome->battery_label = make_label(chrome->screen, &lv_font_montserrat_12, COLOR_TEXT,
                                       BATTERY_LABEL_LEFT, 12, BATTERY_LABEL_WIDTH, 15, "--");
    lv_obj_set_style_text_align(chrome->battery_label, LV_TEXT_ALIGN_RIGHT, 0);
    make_box(chrome->screen, 12, 42, 216, 1, COLOR_LINE, 0);
}

// 连接中:表头 Wi-Fi 信号格以琥珀色闪烁,与已连接(常亮蓝)和离线(常亮灰)区分。
static void wifi_blink_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    static bool dim;
    dim = !dim;
    chrome_t *chromes[] = {&s_list_chrome, &s_setup_chrome, &s_menu_chrome};
    for (size_t i = 0; i < sizeof(chromes) / sizeof(chromes[0]); i++) {
        if (!chromes[i]->wifi_connecting) continue;
        for (int bar = 0; bar < WIFI_BAR_COUNT; bar++) {
            lv_obj_set_style_bg_opa(chromes[i]->wifi_bars[bar],
                                    dim ? LV_OPA_30 : LV_OPA_COVER, 0);
        }
    }
}

// 表头时钟每秒检查一次,分钟变化才重绘;未对时显示占位符。
static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    int hour = 0;
    int minute = 0;
    const bool valid = time_app_now_hm(&hour, &minute);

    char text[6];
    if (valid) {
        snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
    } else {
        snprintf(text, sizeof(text), "--:--");
    }
    if (strcmp(text, s_clock_text) == 0) return;
    snprintf(s_clock_text, sizeof(s_clock_text), "%s", text);

    chrome_t *chromes[] = {&s_list_chrome, &s_setup_chrome, &s_menu_chrome};
    for (size_t i = 0; i < sizeof(chromes) / sizeof(chromes[0]); i++) {
        if (!chromes[i]->clock) continue;
        lv_label_set_text(chromes[i]->clock, s_clock_text);
        lv_obj_set_style_text_color(chromes[i]->clock,
                                    lv_color_hex(valid ? COLOR_TEXT : COLOR_MUTED), 0);
    }
}

void ui_app_init(void)
{
    if (!bsp_lvgl_lock(1000)) return;

    s_list_chrome.screen = make_screen();
    s_setup_chrome.screen = make_screen();
    build_header(&s_list_chrome, "");
    build_header(&s_setup_chrome, "配网");

    for (int i = 0; i < ROW_COUNT; i++) {
        row_widgets_t *row = &s_rows[i];
        row->row = make_box(s_list_chrome.screen, ROW_LEFT, ROW_TOP + i * (ROW_HEIGHT + ROW_GAP),
                            ROW_WIDTH, ROW_HEIGHT, COLOR_BG, 8);
        lv_obj_set_style_border_width(row->row, 1, 0);
        lv_obj_set_style_border_color(row->row, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_border_opa(row->row, LV_OPA_TRANSP, 0);
        row->name = make_label(row->row, &lv_font_cn_16, COLOR_TEXT, 10, 3, 82, 24, "");
        lv_label_set_long_mode(row->name, LV_LABEL_LONG_DOT);
        row->code = make_label(row->row, &lv_font_montserrat_12, COLOR_MUTED, 10, 30, 82, 18, "");
        lv_label_set_long_mode(row->code, LV_LABEL_LONG_DOT);
        row->price = make_label(row->row, &lv_font_montserrat_20, COLOR_TEXT,
                                RIGHT_LEFT, 0, RIGHT_WIDTH, 22, "");
        lv_obj_set_style_text_align(row->price, LV_TEXT_ALIGN_RIGHT, 0);
        row->change = make_label(row->row, &lv_font_montserrat_12, COLOR_FLAT,
                                 RIGHT_LEFT, 22, RIGHT_WIDTH, 15, "");
        lv_obj_set_style_text_align(row->change, LV_TEXT_ALIGN_RIGHT, 0);
        row->pnl = make_label(row->row, &lv_font_cn_12_pnl, COLOR_FLAT,
                              PNL_LEFT, 35, PNL_WIDTH, 18, "");
        lv_obj_set_style_text_align(row->pnl, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_long_mode(row->pnl, LV_LABEL_LONG_DOT);
        lv_label_set_recolor(row->pnl, true);
        lv_obj_add_flag(row->pnl, LV_OBJ_FLAG_HIDDEN);
    }

    s_list_status = make_label(s_list_chrome.screen, &lv_font_cn_16, COLOR_MUTED,
                               30, 291, 94, 24, "等待行情");
    lv_label_set_long_mode(s_list_status, LV_LABEL_LONG_DOT);

    // 刷新中的旋转指示:仅行情页显示,由 snapshot.fetching 驱动。
    s_list_refresh = lv_spinner_create(s_list_chrome.screen);
    lv_obj_set_pos(s_list_refresh, 10, 296);
    lv_obj_set_size(s_list_refresh, 16, 16);
    lv_obj_set_style_arc_width(s_list_refresh, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_list_refresh, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_list_refresh, lv_color_hex(COLOR_LINE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_list_refresh, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    lv_spinner_set_anim_params(s_list_refresh, 900, 270);
    lv_obj_add_flag(s_list_refresh, LV_OBJ_FLAG_HIDDEN);

    s_list_hint = make_label(s_list_chrome.screen, &lv_font_cn_16, COLOR_ACCENT,
                             124, 291, 104, 24, "长按 OK 设置");
    lv_obj_set_style_text_align(s_list_hint, LV_TEXT_ALIGN_RIGHT, 0);

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

    // 设置菜单:五行卡片,可视区四行,选中项驱动滚动;开关行用拨动开关。
    s_menu_chrome.screen = make_screen();
    build_header(&s_menu_chrome, "设置");

    static const char *const menu_names[MENU_ROW_COUNT] = {
        "热点开关", "屏幕亮度", "刷新间隔", "自动息屏", "息屏时间",
    };
    static const char *const menu_subs[MENU_ROW_COUNT] = {
        "192.168.4.1", "", "手动 / 1-300 秒", "屏幕到时自动熄灭", "5-60 秒",
    };
    for (int i = 0; i < MENU_ROW_COUNT; i++) {
        menu_row_widgets_t *row = &s_menu_rows[i];
        row->row = make_box(s_menu_chrome.screen, MENU_ROW_LEFT,
                            MENU_ROW_TOP + i * (MENU_ROW_HEIGHT + MENU_ROW_GAP),
                            MENU_ROW_WIDTH, MENU_ROW_HEIGHT, COLOR_PANEL, 10);
        lv_obj_set_style_border_width(row->row, 2, 0);
        lv_obj_set_style_border_color(row->row, lv_color_hex(COLOR_LINE), 0);
        lv_obj_set_style_border_opa(row->row, LV_OPA_COVER, 0);
        if (i >= MENU_VISIBLE_ROWS) lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);

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

    lv_obj_t *scroll_track = make_box(s_menu_chrome.screen, MENU_SCROLL_LEFT, MENU_ROW_TOP,
                                      MENU_SCROLL_WIDTH, MENU_TRACK_HEIGHT, COLOR_LINE,
                                      MENU_SCROLL_WIDTH / 2);
    s_menu_scroll_thumb = make_box(scroll_track, 0, 0, MENU_SCROLL_WIDTH, MENU_THUMB_HEIGHT,
                                   COLOR_ACCENT, MENU_SCROLL_WIDTH / 2);

    make_label(s_menu_chrome.screen, &lv_font_cn_16, COLOR_MUTED, 12, 270, 216, 20,
               "长按 OK 关闭菜单");
    s_menu_hint = make_label(s_menu_chrome.screen, &lv_font_cn_16, COLOR_ACCENT,
                             12, 292, 216, 22, "上下选择  OK 调整");

    s_active = s_list_chrome.screen;
    lv_screen_load(s_active);
    lv_timer_create(wifi_blink_timer_cb, WIFI_BLINK_PERIOD_MS, NULL);
    lv_timer_create(clock_timer_cb, CLOCK_PERIOD_MS, NULL);

    // 池耗尽时 LVGL 的 LV_ASSERT_MALLOC 只会死等,日志是唯一外部可见的余量依据。
    lv_mem_monitor_t mem;
    lv_mem_monitor(&mem);
    ESP_LOGI("ui_app", "LVGL 内存: 已用 %u/%u 字节,峰值 %u,碎片 %u%%,系统堆 %u 字节",
             (unsigned)(mem.total_size - mem.free_size), (unsigned)mem.total_size,
             (unsigned)mem.max_used, (unsigned)mem.frag_pct,
             (unsigned)esp_get_free_heap_size());
    bsp_lvgl_unlock();
}

static int wifi_signal_level(int rssi)
{
    if (rssi >= -55) return 4;   // 0 dBm(读数未知)也按满格
    if (rssi >= -67) return 3;
    if (rssi >= -78) return 2;
    return 1;
}

static void set_wifi(chrome_t *chrome, bool connected, bool connecting, int rssi)
{
    const bool link_connecting = connecting && !connected;
    chrome->wifi_connecting = link_connecting;
    const int level = connected ? wifi_signal_level(rssi) : 0;
    for (int i = 0; i < WIFI_BAR_COUNT; i++) {
        uint32_t color = COLOR_MUTED;
        if (connected) {
            color = i < level ? COLOR_ACCENT : COLOR_LINE;
        } else if (link_connecting) {
            color = COLOR_WARN;
        }
        lv_obj_set_style_bg_color(chrome->wifi_bars[i], lv_color_hex(color), 0);
        // 离线时整体压暗,避免四格灰条被误读成满格。
        lv_obj_set_style_bg_opa(chrome->wifi_bars[i],
                                (connected || link_connecting) ? LV_OPA_COVER : LV_OPA_50, 0);
    }
}

static void set_battery(chrome_t *chrome, int pct, bool ok)
{
    if (!ok || pct < 0) {
        lv_label_set_text(chrome->battery_label, "--");
        return;
    }
    if (pct > 100) pct = 100;
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

static uint32_t trend_color(quote_trend_t trend)
{
    return trend == QUOTE_TREND_UP ? COLOR_UP :
           (trend == QUOTE_TREND_DOWN ? COLOR_DOWN : COLOR_FLAT);
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
        bool show_pnl = false;

        if (quote && quote->valid) {
            const int decimals = quote_price_decimals(code);
            char amount[16];
            char pct[16];
            quote_format_price(price_text, sizeof(price_text), code, quote->price);
            quote_format_signed(amount, sizeof(amount), quote->change, decimals);
            quote_format_percent(pct, sizeof(pct), quote->change_pct);
            snprintf(change_text, sizeof(change_text), "%s %s", amount, pct);
            color = trend_color(quote_trend(quote->change));

            const double cost = view->config->cost_price[index];
            const uint32_t lots = view->config->lots[index];
            if (stock_position_is_set(cost, lots)) {
                const double profit = stock_position_profit(quote->price, cost, lots);
                const double today = stock_position_today_profit(quote->change, lots);
                char profit_text[24];
                char today_text[24];
                char pnl_text[96];
                stock_position_format_amount(profit_text, sizeof(profit_text), profit);
                stock_position_format_amount(today_text, sizeof(today_text), today);
                snprintf(pnl_text, sizeof(pnl_text), "#%06X 总:%s# #%06X 今:%s#",
                         (unsigned)trend_color(quote_trend(profit)), profit_text,
                         (unsigned)trend_color(quote_trend(today)), today_text);
                lv_label_set_text(row->pnl, pnl_text);
                show_pnl = true;
            }
        }

        const char *name = quote && quote->name[0] ? quote->name : "等待行情";
        lv_label_set_text(row->name, view->config->count == 0 ? "尚未添加自选股" : name);
        lv_label_set_text(row->code, code);
        lv_label_set_text(row->price, price_text);
        lv_obj_set_style_text_color(row->price, lv_color_hex(quote && quote->valid ? color : COLOR_MUTED), 0);
        lv_label_set_text(row->change, change_text);
        lv_obj_set_style_text_color(row->change, lv_color_hex(quote && quote->valid ? color : COLOR_MUTED), 0);
        if (show_pnl) {
            lv_obj_clear_flag(row->pnl, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(row->pnl, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!view->wifi_connected) {
        lv_label_set_text(s_list_status, "连接网络...");
    } else if (snapshot->error[0]) {
        lv_label_set_text(s_list_status, snapshot->error);
    } else if (snapshot->fetching && !snapshot->has_data) {
        lv_label_set_text(s_list_status, "获取行情...");
    } else if (!view->market_open) {
        lv_label_set_text(s_list_status, "休市");
    } else if (view->config->interval_sec == REFRESH_INTERVAL_MANUAL) {
        lv_label_set_text(s_list_status, "按OK刷新");
    } else {
        lv_label_set_text_fmt(s_list_status, "%u秒刷新", (unsigned)view->config->interval_sec);
    }
    lv_label_set_text(s_list_hint, "长按 OK 设置");
    if (snapshot->fetching) {
        lv_obj_remove_flag(s_list_refresh, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_list_refresh, LV_OBJ_FLAG_HIDDEN);
    }
    set_wifi(&s_list_chrome, view->wifi_connected, view->wifi_connecting, view->rssi);
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
    set_wifi(&s_setup_chrome, view->wifi_connected, view->wifi_connecting, view->rssi);
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

    int selected = view->selected;
    if (selected < 0) selected = 0;
    if (selected >= MENU_ROW_COUNT) selected = MENU_ROW_COUNT - 1;
    const int max_offset = MENU_ROW_COUNT - MENU_VISIBLE_ROWS;
    if (selected < s_menu_offset) s_menu_offset = selected;
    if (selected >= s_menu_offset + MENU_VISIBLE_ROWS) {
        s_menu_offset = selected - MENU_VISIBLE_ROWS + 1;
    }
    if (s_menu_offset > max_offset) s_menu_offset = max_offset;
    if (s_menu_offset < 0) s_menu_offset = 0;

    for (int i = 0; i < MENU_ROW_COUNT; i++) {
        menu_row_widgets_t *row = &s_menu_rows[i];
        const int slot = i - s_menu_offset;
        if (slot < 0 || slot >= MENU_VISIBLE_ROWS) {
            lv_obj_add_flag(row->row, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(row->row, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_y(row->row, MENU_ROW_TOP + slot * (MENU_ROW_HEIGHT + MENU_ROW_GAP));
        }

        const bool is_selected = i == selected;
        const bool editing = is_selected && view->editing;
        set_menu_row_selected(i, is_selected, editing);

        uint32_t value_color = is_selected ? COLOR_TEXT : COLOR_MUTED;
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
        } else if (i == MENU_ITEM_REFRESH) {
            if (view->interval_sec == REFRESH_INTERVAL_MANUAL) {
                snprintf(value, sizeof(value), "手动");
            } else {
                snprintf(value, sizeof(value), "%d 秒", view->interval_sec);
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

    const int thumb_travel = MENU_TRACK_HEIGHT - MENU_THUMB_HEIGHT;
    lv_obj_set_y(s_menu_scroll_thumb,
                 max_offset > 0 ? (thumb_travel * s_menu_offset) / max_offset : 0);

    lv_label_set_text(s_menu_hint, view->editing ? "上下调整  OK 完成"
                                                 : "上下选择  OK 调整");
    set_wifi(&s_menu_chrome, view->wifi_connected, view->wifi_connecting, view->rssi);
    set_battery(&s_menu_chrome, view->battery_pct, view->battery_ok);
    bsp_lvgl_unlock();
}
