// main/ui_app.h —— 股票盯盘的三个页面:行情列表、配网引导与设置菜单。
#pragma once

#include <stdbool.h>

#include "config_store.h"
#include "quote_worker.h"

typedef struct {
    const stock_config_t *config;
    const quote_snapshot_t *quotes;
    int selected;
    int offset;
    bool wifi_connected;
    bool wifi_connecting;
    int rssi;
    int battery_pct;
    bool battery_ok;
    bool market_open;
} ui_list_view_t;

typedef struct {
    const char *ap_ssid;
    const char *status;
    const char *hint;
    bool wifi_connected;
    bool wifi_connecting;
    int rssi;
    int battery_pct;
    bool battery_ok;
} ui_setup_view_t;

typedef struct {
    int selected;
    bool editing;
    int brightness;
    int interval_sec;
    int auto_off_sec;
    bool auto_off_enabled;
    bool ap_enabled;
    bool wifi_connected;
    bool wifi_connecting;
    int rssi;
    int battery_pct;
    bool battery_ok;
} ui_menu_view_t;

typedef struct {
    const char *code;
    const char *name;             // 列表里的 UTF-8 名称,可为空
    const quote_detail_t *detail; // 五档/市值/市盈率/分时
    const char *status;           // 提示位替代文案(网络/错误),空闲时为 NULL
    bool fetching;                // 刷新进行中,显示旋转指示
    bool wifi_connected;
    bool wifi_connecting;
    int rssi;
    int battery_pct;
    bool battery_ok;
} ui_detail_view_t;

void ui_app_init(void);
void ui_app_show_list(void);
void ui_app_show_setup(void);
void ui_app_show_menu(void);
void ui_app_show_detail(void);
void ui_app_update_list(const ui_list_view_t *view);
void ui_app_update_setup(const ui_setup_view_t *view);
void ui_app_update_menu(const ui_menu_view_t *view);
void ui_app_update_detail(const ui_detail_view_t *view);
