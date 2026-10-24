// main/wifi_app.h —— 可开关的配网热点 + STA 联网 + 弱信号漫游。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define WIFI_APP_SSID_MAX 33
#define WIFI_APP_SCAN_MAX 20

typedef struct {
    bool ap_enabled;
    bool ap_started;
    bool sta_configured;
    bool sta_connected;
    bool portal;
    uint8_t fail_count;
    char ap_ssid[WIFI_APP_SSID_MAX];
    char sta_ssid[WIFI_APP_SSID_MAX];
    char ip[16];
    int8_t rssi;
} wifi_app_info_t;

typedef struct {
    char ssid[WIFI_APP_SSID_MAX];
    int8_t rssi;
    bool secure;
} wifi_app_ap_t;

// ap_enabled=false 时以纯 STA 模式启动,热点需之后由设备菜单开启。
esp_err_t wifi_app_start(const char *ssid, const char *pass, bool ap_enabled);
esp_err_t wifi_app_apply(const char *ssid, const char *pass);
esp_err_t wifi_app_forget(void);

// 运行时开启/关闭配网热点。切换模式可能短暂断开 STA,随后自动重连。
esp_err_t wifi_app_set_ap_enabled(bool enabled);
bool wifi_app_ap_enabled(void);

// 用户在网页或按键上关闭配网页;网络恢复前不再自动弹出。
void wifi_app_dismiss_portal(void);
// 当前显示配网页则收起,否则强制显示。返回切换后的状态。
bool wifi_app_toggle_portal(void);

bool wifi_app_portal_active(void);
bool wifi_app_sta_connected(void);
void wifi_app_get_info(wifi_app_info_t *out);

// 同步扫描。返回 AP 数量,失败返回 -1。
int wifi_app_scan(wifi_app_ap_t *out, int max);
