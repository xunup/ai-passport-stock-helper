// main/app_controller.c —— 汇总配置/Wi-Fi/行情状态,驱动按键、菜单与页面。
#include "app_controller.h"

#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "config_store.h"
#include "market_hours.h"
#include "menu_model.h"
#include "quote_worker.h"
#include "stock_codes.h"
#include "time_app.h"
#include "ui_app.h"
#include "ui_model.h"
#include "web_server.h"
#include "wifi_app.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

static const char *TAG = "app";

#define CONTROLLER_STACK 5120
#define CONTROLLER_PRIORITY 4
#define BATTERY_PERIOD_US (15 * 1000 * 1000)
#define BRIGHTNESS_LEVELS 5

// 五档背光,1 档最暗、5 档最亮;息屏时直接写 0。
static const uint8_t s_brightness_pct[BRIGHTNESS_LEVELS] = {20, 40, 60, 80, 100};

typedef struct {
    uint8_t btn;
    uint8_t ev;
} key_event_t;

static QueueHandle_t s_keys;
static ui_model_t s_model;
static menu_model_t s_menu;
static stock_config_t s_config;
static bool s_wifi_error;
static int s_battery_pct = -1;
static bool s_battery_ok;
static bool s_menu_open;
static bool s_screen_off;
static bool s_swallow_wake;
static bool s_long_pressed[BSP_BTN_COUNT];
static int s_brightness;
static int s_auto_off_sec;
static bool s_auto_off_enabled;
static bool s_ap_enabled;
static int64_t s_last_activity_us;

static uint8_t brightness_percent(int level)
{
    if (level < 1) level = 1;
    if (level > BRIGHTNESS_LEVELS) level = BRIGHTNESS_LEVELS;
    return s_brightness_pct[level - 1];
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_keys) return;
    const key_event_t event = {.btn = (uint8_t)btn, .ev = (uint8_t)ev};
    (void)xQueueSend(s_keys, &event, 0);
}

static bool stock_config_equal(const stock_config_t *a, const stock_config_t *b)
{
    return a->count == b->count &&
           a->interval_sec == b->interval_sec &&
           strcmp(a->ssid, b->ssid) == 0 &&
           strcmp(a->pass, b->pass) == 0 &&
           memcmp(a->codes, b->codes, sizeof(a->codes)) == 0 &&
           memcmp(a->cost_price, b->cost_price, sizeof(a->cost_price)) == 0 &&
           memcmp(a->lots, b->lots, sizeof(a->lots)) == 0;
}

// 网页与设备菜单共用 config_store;本循环同时收敛亮度和热点等菜单设置。
static bool apply_new_config(void)
{
    stock_config_t next;
    config_store_get(&next);

    bool changed = false;
    if (s_config.brightness != next.brightness) {
        s_brightness = next.brightness;
        if (!s_screen_off) bsp_display_backlight(brightness_percent(s_brightness));
        changed = true;
    }
    if (s_config.auto_off_sec != next.auto_off_sec) {
        s_auto_off_sec = next.auto_off_sec;
        changed = true;
    }
    if (s_config.auto_off_enabled != next.auto_off_enabled) {
        s_auto_off_enabled = next.auto_off_enabled;
        changed = true;
    }
    if (s_config.ap_enabled != next.ap_enabled) {
        s_ap_enabled = next.ap_enabled;
        changed = true;
    }
    if (!stock_config_equal(&s_config, &next)) {
        ui_model_set_count(&s_model, next.count);
        if (next.interval_sec == STOCK_CONFIG_INTERVAL_MANUAL) {
            ESP_LOGI(TAG, "配置更新: %d 只股票, 手动刷新", next.count);
        } else {
            ESP_LOGI(TAG, "配置更新: %d 只股票, %u 秒", next.count, next.interval_sec);
        }
        changed = true;
    }
    if (changed) {
        s_config = next;
        if (s_menu_open) {
            s_menu.brightness = (uint8_t)s_brightness;
            s_menu.interval_sec = s_config.interval_sec;
            s_menu.auto_off_sec = (uint8_t)s_auto_off_sec;
            s_menu.auto_off_enabled = s_auto_off_enabled;
            s_menu.ap_enabled = s_ap_enabled;
        }
    }
    return changed;
}

static void persist_display(void)
{
    const esp_err_t err = config_store_save_display((uint8_t)s_brightness,
                                                    (uint8_t)s_auto_off_sec,
                                                    s_auto_off_enabled);
    if (err != ESP_OK) ESP_LOGW(TAG, "保存显示设置失败: %s", esp_err_to_name(err));
}

static void apply_menu_change(menu_change_t change)
{
    if (change == MENU_CHANGE_BRIGHTNESS) {
        s_brightness = s_menu.brightness;
        bsp_display_backlight(brightness_percent(s_brightness));
        persist_display();
    } else if (change == MENU_CHANGE_REFRESH_INTERVAL) {
        const esp_err_t err = config_store_save_interval(s_menu.interval_sec);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "保存刷新间隔失败: %s", esp_err_to_name(err));
            s_menu.interval_sec = s_config.interval_sec;
            return;
        }
        s_config.interval_sec = s_menu.interval_sec;
        quote_worker_set_interval(s_menu.interval_sec);
    } else if (change == MENU_CHANGE_AUTO_OFF_TIME) {
        s_auto_off_sec = s_menu.auto_off_sec;
        persist_display();
    } else if (change == MENU_CHANGE_AUTO_OFF_ENABLED) {
        s_auto_off_enabled = s_menu.auto_off_enabled;
        persist_display();
    }
}

static void toggle_hotspot(void)
{
    const bool target = s_menu.ap_enabled;
    const esp_err_t err = wifi_app_set_ap_enabled(target);
    if (err != ESP_OK) {
        s_menu.ap_enabled = !target;
        ESP_LOGE(TAG, "切换热点失败: %s", esp_err_to_name(err));
        return;
    }
    s_ap_enabled = target;
    const esp_err_t save_err = config_store_save_ap_enabled(target);
    if (save_err != ESP_OK) {
        ESP_LOGW(TAG, "保存热点开关失败: %s", esp_err_to_name(save_err));
    }
}

static void open_menu(void)
{
    menu_model_init(&s_menu, (uint8_t)s_brightness, s_config.interval_sec,
                    (uint8_t)s_auto_off_sec, s_auto_off_enabled, s_ap_enabled);
    s_menu_open = true;
    s_last_activity_us = esp_timer_get_time();
}

static bool wifi_is_connecting(const wifi_app_info_t *wifi)
{
    return !s_wifi_error && wifi->sta_configured && !wifi->sta_connected;
}

// 与行情任务保持一致:未对时时不做时段限制。
static bool market_is_open(void)
{
    int hour = 0;
    int minute = 0;
    if (!time_app_now_hm(&hour, &minute)) return true;
    return market_hours_is_trading(hour, minute);
}

static void render_setup(const wifi_app_info_t *wifi)
{
    const char *status = "";
    const char *hint = "";
    if (!wifi->ap_enabled) {
        status = "配网热点已关闭";
        hint = "长按 OK 打开菜单";
    } else if (s_wifi_error) {
        status = "Wi-Fi 启动失败";
        hint = "请重启设备";
    } else if (!wifi->sta_configured) {
        status = "等待配网配置";
        hint = "配置完成后自动进入行情";
    } else if (!wifi->sta_connected) {
        status = wifi->fail_count == 0 ? "正在连接 Wi-Fi..."
                                       : "连接失败，请检查密码";
        hint = "长按 OK 打开菜单";
    } else if (s_config.count == 0) {
        status = "网络已连接，请添加自选股";
        hint = "长按 OK 打开菜单";
    } else {
        status = "配网模式";
        hint = "长按 OK 打开菜单";
    }

    ui_setup_view_t view = {
        .ap_ssid = wifi->ap_ssid,
        .status = status,
        .hint = hint,
        .wifi_connected = wifi->sta_connected,
        .wifi_connecting = wifi_is_connecting(wifi),
        .rssi = wifi->rssi,
        .battery_pct = s_battery_pct,
        .battery_ok = s_battery_ok,
    };
    ui_app_show_setup();
    ui_app_update_setup(&view);
}

static void render_list(const wifi_app_info_t *wifi, const quote_snapshot_t *quotes,
                        bool market_open)
{
    ui_list_view_t view = {
        .config = &s_config,
        .quotes = quotes,
        .selected = s_model.selected,
        .offset = s_model.offset,
        .wifi_connected = wifi->sta_connected,
        .wifi_connecting = wifi_is_connecting(wifi),
        .rssi = wifi->rssi,
        .battery_pct = s_battery_pct,
        .battery_ok = s_battery_ok,
        .market_open = market_open,
    };
    ui_app_show_list();
    ui_app_update_list(&view);
}

static void render_menu(const wifi_app_info_t *wifi)
{
    ui_menu_view_t view = {
        .selected = s_menu.selected,
        .editing = s_menu.editing,
        .brightness = s_menu.brightness,
        .interval_sec = s_menu.interval_sec,
        .auto_off_sec = s_menu.auto_off_sec,
        .auto_off_enabled = s_menu.auto_off_enabled,
        .ap_enabled = s_menu.ap_enabled,
        .wifi_connected = wifi->sta_connected,
        .wifi_connecting = wifi_is_connecting(wifi),
        .rssi = wifi->rssi,
        .battery_pct = s_battery_pct,
        .battery_ok = s_battery_ok,
    };
    ui_app_show_menu();
    ui_app_update_menu(&view);
}

static void controller_task(void *arg)
{
    (void)arg;
    uint32_t last_quote_generation = UINT32_MAX;
    bool last_portal = false;
    bool last_connected = false;
    int last_battery = -2;
    int64_t last_battery_us = 0;
    bool dirty = true;
    bool last_market_open = market_is_open();
    int last_rssi = -128;
    s_last_activity_us = esp_timer_get_time();

    for (;;) {
        key_event_t event;
        if (xQueueReceive(s_keys, &event, pdMS_TO_TICKS(200)) == pdTRUE) {
            const bsp_btn_t btn = (bsp_btn_t)event.btn;
            const bsp_btn_ev_t ev = (bsp_btn_ev_t)event.ev;

            if (ev == BSP_BTN_PRESS) s_long_pressed[btn] = false;

            if (s_screen_off) {
                if (ev == BSP_BTN_PRESS) {
                    // 只唤醒,不把这一下按键当成操作;后续事件在松手前也吞掉。
                    s_screen_off = false;
                    s_swallow_wake = true;
                    bsp_display_backlight(brightness_percent(s_brightness));
                    quote_worker_set_idle(false);
                    quote_worker_request_refresh();
                    s_last_activity_us = esp_timer_get_time();
                    dirty = true;
                    if (s_config.interval_sec == STOCK_CONFIG_INTERVAL_MANUAL) {
                        ESP_LOGI(TAG, "屏幕唤醒,恢复手动刷新");
                    } else {
                        ESP_LOGI(TAG, "屏幕唤醒,恢复 %u 秒行情刷新",
                                 (unsigned)s_config.interval_sec);
                    }
                }
                continue;
            }
            if (s_swallow_wake) {
                if (ev == BSP_BTN_RELEASE) s_swallow_wake = false;
                continue;
            }

            s_last_activity_us = esp_timer_get_time();

            if (ev == BSP_BTN_LONG) {
                // 这一下已由长按接管,松手不再当单击处理。
                s_long_pressed[btn] = true;
                if (btn == BSP_BTN_OK) {
                    if (s_menu_open) {
                        s_menu_open = false;
                    } else {
                        open_menu();
                    }
                    dirty = true;
                }
            } else if (ev == BSP_BTN_RELEASE) {
                const bool long_pressed = s_long_pressed[btn];
                s_long_pressed[btn] = false;
                if (!long_pressed && s_menu_open) {
                    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
                        const menu_change_t change =
                            menu_model_move(&s_menu, btn == BSP_BTN_UP ? -1 : 1);
                        apply_menu_change(change);
                        dirty = true;
                    } else if (btn == BSP_BTN_OK) {
                        const menu_activate_t action = menu_model_activate(&s_menu);
                        if (action == MENU_ACTIVATE_TOGGLE_HOTSPOT) toggle_hotspot();
                        dirty = true;
                    }
                } else if (!long_pressed) {
                    if (btn == BSP_BTN_UP) {
                        dirty = ui_model_move(&s_model, -1) || dirty;
                    } else if (btn == BSP_BTN_DOWN) {
                        dirty = ui_model_move(&s_model, 1) || dirty;
                    } else if (btn == BSP_BTN_OK) {
                        quote_worker_request_refresh();
                    }
                }
            }
        }

        dirty = apply_new_config() || dirty;

        quote_snapshot_t snapshot;
        quote_worker_snapshot(&snapshot);
        if (snapshot.generation != last_quote_generation) {
            last_quote_generation = snapshot.generation;
            dirty = true;
        }

        wifi_app_info_t wifi;
        wifi_app_get_info(&wifi);
        if (wifi.rssi != last_rssi) {
            // 漫游任务的 RSSI 每 30 秒刷新一次,信号格随之更新。
            last_rssi = wifi.rssi;
            dirty = true;
        }
        const bool portal = s_wifi_error || wifi.portal || s_config.count == 0;
        const bool just_connected = wifi.sta_connected && !last_connected;
        if (portal != last_portal || wifi.sta_connected != last_connected) {
            last_portal = portal;
            last_connected = wifi.sta_connected;
            dirty = true;
        }
        if (just_connected) {
            // 刚拿到 IP 时立刻拉一次,不必等满一个刷新周期。
            quote_worker_request_refresh();
        }

        const bool market_open = market_is_open();
        if (market_open != last_market_open) {
            last_market_open = market_open;
            dirty = true;
        }

        const int64_t now_us = esp_timer_get_time();
        if (!s_screen_off && s_auto_off_enabled && s_auto_off_sec > 0 &&
            now_us - s_last_activity_us >= (int64_t)s_auto_off_sec * 1000 * 1000) {
            s_screen_off = true;
            bsp_display_backlight(0);
            quote_worker_set_idle(true);
            if (s_config.interval_sec == STOCK_CONFIG_INTERVAL_MANUAL) {
                ESP_LOGI(TAG, "自动息屏(%d 秒),行情保持手动刷新", s_auto_off_sec);
            } else {
                ESP_LOGI(TAG, "自动息屏(%d 秒),行情改为 60 秒刷新", s_auto_off_sec);
            }
        }

        if (now_us - last_battery_us >= BATTERY_PERIOD_US) {
            last_battery_us = now_us;
            const int pct = bsp_battery_soc();
            const bool ok = pct >= 0;
            if (pct != last_battery || ok != s_battery_ok) {
                last_battery = pct;
                s_battery_pct = pct;
                s_battery_ok = ok;
                dirty = true;
            }
        }

        if (dirty && !s_screen_off) {
            if (s_menu_open) {
                render_menu(&wifi);
            } else if (portal) {
                render_setup(&wifi);
            } else {
                render_list(&wifi, &snapshot, market_open);
            }
            dirty = false;
        }
    }
}

esp_err_t app_controller_start(void)
{
    esp_err_t err = config_store_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s", esp_err_to_name(err));
    }
    config_store_get(&s_config);
    s_brightness = s_config.brightness;
    s_auto_off_sec = s_config.auto_off_sec;
    s_auto_off_enabled = s_config.auto_off_enabled;
    s_ap_enabled = s_config.ap_enabled;
    ui_model_init(&s_model, s_config.count);

    s_keys = xQueueCreate(16, sizeof(key_event_t));
    if (!s_keys) return ESP_ERR_NO_MEM;

    err = wifi_app_start(s_config.ssid, s_config.pass, s_ap_enabled);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi 启动失败: %s", esp_err_to_name(err));
        s_wifi_error = true;
    }

    err = time_app_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "网络对时启动失败: %s", esp_err_to_name(err));

    ui_app_init();
    bsp_display_backlight(brightness_percent(s_brightness));

    err = bsp_battery_init();
    s_battery_ok = err == ESP_OK;
    if (s_battery_ok) s_battery_pct = bsp_battery_soc();

    err = bsp_button_init(on_key, NULL);
    if (err != ESP_OK) ESP_LOGE(TAG, "按键初始化失败: %s", esp_err_to_name(err));

    err = web_server_start();
    if (err != ESP_OK) ESP_LOGE(TAG, "配置网页启动失败: %s", esp_err_to_name(err));

    char codes[STOCK_CODES_MAX * STOCK_CODE_LEN + 8];
    if (stock_codes_join(codes, sizeof(codes), s_config.codes, s_config.count) < 0) {
        codes[0] = '\0';
    }
    err = quote_worker_start(codes, s_config.interval_sec);
    if (err != ESP_OK) ESP_LOGE(TAG, "行情任务启动失败: %s", esp_err_to_name(err));

    if (xTaskCreate(controller_task, "app_controller", CONTROLLER_STACK, NULL,
                    CONTROLLER_PRIORITY, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "股票盯盘已启动");
    return ESP_OK;
}
