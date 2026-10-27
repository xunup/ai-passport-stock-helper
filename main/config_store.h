// main/config_store.h —— 配网凭证与自选股配置的持久化(NVS)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "refresh_options.h"
#include "settings_limits.h"
#include "stock_codes.h"
#include "stock_positions.h"

#define STOCK_CONFIG_SSID_MAX 33
#define STOCK_CONFIG_PASS_MAX 65
#define STOCK_CONFIG_INTERVAL_MANUAL REFRESH_INTERVAL_MANUAL
#define STOCK_CONFIG_INTERVAL_MIN 1
#define STOCK_CONFIG_INTERVAL_MAX 300
#define STOCK_CONFIG_INTERVAL_DEFAULT 5

#define STOCK_CONFIG_BRIGHTNESS_MIN SETTINGS_BRIGHTNESS_MIN
#define STOCK_CONFIG_BRIGHTNESS_MAX SETTINGS_BRIGHTNESS_MAX
#define STOCK_CONFIG_BRIGHTNESS_DEFAULT SETTINGS_BRIGHTNESS_DEFAULT
#define STOCK_CONFIG_AUTO_OFF_MIN SETTINGS_AUTO_OFF_MIN
#define STOCK_CONFIG_AUTO_OFF_MAX SETTINGS_AUTO_OFF_MAX
#define STOCK_CONFIG_AUTO_OFF_STEP SETTINGS_AUTO_OFF_STEP
#define STOCK_CONFIG_AUTO_OFF_DEFAULT SETTINGS_AUTO_OFF_DEFAULT

typedef struct {
    char ssid[STOCK_CONFIG_SSID_MAX];
    char pass[STOCK_CONFIG_PASS_MAX];
    int count;
    char codes[STOCK_CODES_MAX][STOCK_CODE_LEN];
    // 与 codes 下标对齐;成本价和手数都大于 0 才计算并显示盈亏。
    double cost_price[STOCK_CODES_MAX];
    uint32_t lots[STOCK_CODES_MAX];
    // 0 表示手动刷新,其余为 refresh_options.h 中的档位秒数。
    uint16_t interval_sec;
    // 设备菜单设置:背光档位 1-5、自动息屏开关与秒数(5-60,5 秒一档)、配网热点开关。
    uint8_t brightness;
    uint8_t auto_off_sec;
    bool auto_off_enabled;
    bool ap_enabled;
} stock_config_t;

esp_err_t config_store_init(void);

// 拷贝一份当前配置快照。未初始化时返回全默认值。
void config_store_get(stock_config_t *out);

// 解析并保存自选股与刷新间隔;成功后才更新内存缓存。
esp_err_t config_store_save_config(const char *codes_text, int interval_sec);

// 只更新刷新间隔(设备菜单用),不触碰自选股。
esp_err_t config_store_save_interval(uint16_t interval_sec);

// 更新指定股票的成本价与手数;未列出的股票保持不变,0 表示清除。
esp_err_t config_store_save_positions(const stock_position_t *items, int count);

// 保存 Wi-Fi 凭证;ssid 为空表示清除。成功后才更新内存缓存。
esp_err_t config_store_save_wifi(const char *ssid, const char *pass);
esp_err_t config_store_forget_wifi(void);

// 保存设备菜单设置;越界值会被夹到有效范围,自动息屏按 5 秒对齐。
esp_err_t config_store_save_display(uint8_t brightness, uint8_t auto_off_sec,
                                    bool auto_off_enabled);
esp_err_t config_store_save_ap_enabled(bool enabled);
