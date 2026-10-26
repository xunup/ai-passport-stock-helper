// main/config_store.c —— NVS 键值:stock_cfg/{ssid,pass,codes,interval}。
#include "config_store.h"

#include "nvs.h"
#include "nvs_flash.h"

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define CONFIG_NAMESPACE "stock_cfg"
#define KEY_SSID "ssid"
#define KEY_PASS "pass"
#define KEY_CODES "codes"
#define KEY_INTERVAL "interval"
#define KEY_BRIGHTNESS "brightness"
#define KEY_AUTO_OFF "auto_off"
#define KEY_AUTO_OFF_ENABLED "auto_off_on"
#define KEY_AP_ENABLED "ap_enabled"

static SemaphoreHandle_t s_lock;
static stock_config_t s_config;
static bool s_ready;

static void lock(void)
{
    if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    if (s_lock) xSemaphoreGive(s_lock);
}

static uint16_t clamp_interval(int value)
{
    if (value < STOCK_CONFIG_INTERVAL_MIN) return STOCK_CONFIG_INTERVAL_MIN;
    if (value > STOCK_CONFIG_INTERVAL_MAX) return STOCK_CONFIG_INTERVAL_MAX;
    return (uint16_t)value;
}

static uint8_t clamp_brightness(int value)
{
    if (value < STOCK_CONFIG_BRIGHTNESS_MIN) return STOCK_CONFIG_BRIGHTNESS_MIN;
    if (value > STOCK_CONFIG_BRIGHTNESS_MAX) return STOCK_CONFIG_BRIGHTNESS_MAX;
    return (uint8_t)value;
}

static uint8_t clamp_auto_off(int value)
{
    if (value < STOCK_CONFIG_AUTO_OFF_MIN) return STOCK_CONFIG_AUTO_OFF_MIN;
    if (value > STOCK_CONFIG_AUTO_OFF_MAX) return STOCK_CONFIG_AUTO_OFF_MAX;
    return (uint8_t)((value / STOCK_CONFIG_AUTO_OFF_STEP) * STOCK_CONFIG_AUTO_OFF_STEP);
}

static void set_defaults(stock_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->interval_sec = STOCK_CONFIG_INTERVAL_DEFAULT;
    cfg->brightness = STOCK_CONFIG_BRIGHTNESS_DEFAULT;
    cfg->auto_off_sec = STOCK_CONFIG_AUTO_OFF_DEFAULT;
    cfg->auto_off_enabled = true;
    cfg->ap_enabled = true;
}

static void load_from_nvs(stock_config_t *cfg)
{
    nvs_handle_t handle;
    set_defaults(cfg);
    if (nvs_open(CONFIG_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    size_t len = sizeof(cfg->ssid);
    if (nvs_get_str(handle, KEY_SSID, cfg->ssid, &len) != ESP_OK) cfg->ssid[0] = '\0';

    len = sizeof(cfg->pass);
    if (nvs_get_str(handle, KEY_PASS, cfg->pass, &len) != ESP_OK) cfg->pass[0] = '\0';

    char codes_text[STOCK_CODES_MAX * STOCK_CODE_LEN + 8] = {0};
    len = sizeof(codes_text);
    if (nvs_get_str(handle, KEY_CODES, codes_text, &len) == ESP_OK) {
        int count = 0;
        if (stock_codes_parse(codes_text, cfg->codes, STOCK_CODES_MAX, &count, NULL) ==
            STOCK_CODES_OK) {
            cfg->count = count;
        }
    }

    uint16_t interval = 0;
    if (nvs_get_u16(handle, KEY_INTERVAL, &interval) == ESP_OK) {
        cfg->interval_sec = clamp_interval(interval);
    }

    uint8_t value = 0;
    if (nvs_get_u8(handle, KEY_BRIGHTNESS, &value) == ESP_OK) {
        cfg->brightness = clamp_brightness(value);
    }
    if (nvs_get_u8(handle, KEY_AUTO_OFF, &value) == ESP_OK) {
        cfg->auto_off_sec = clamp_auto_off(value);
    }
    if (nvs_get_u8(handle, KEY_AUTO_OFF_ENABLED, &value) == ESP_OK) {
        cfg->auto_off_enabled = value != 0;
    }
    if (nvs_get_u8(handle, KEY_AP_ENABLED, &value) == ESP_OK) {
        cfg->ap_enabled = value != 0;
    }

    nvs_close(handle);
}

static esp_err_t write_string(nvs_handle_t handle, const char *key, const char *value)
{
    if (value[0] == '\0') {
        const esp_err_t err = nvs_erase_key(handle, key);
        return err == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : err;
    }
    return nvs_set_str(handle, key, value);
}

esp_err_t config_store_init(void)
{
    if (s_ready) return ESP_OK;
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
        if (!s_lock) return ESP_ERR_NO_MEM;
    }

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        err = nvs_flash_erase();
        if (err == ESP_OK) err = nvs_flash_init();
    }
    if (err != ESP_OK) return err;

    lock();
    load_from_nvs(&s_config);
    s_ready = true;
    unlock();
    return ESP_OK;
}

void config_store_get(stock_config_t *out)
{
    if (!out) return;
    lock();
    if (s_ready) {
        *out = s_config;
    } else {
        set_defaults(out);
    }
    unlock();
}

static bool code_text_valid(const char *codes_text)
{
    return codes_text && codes_text[0] != '\0';
}

esp_err_t config_store_save_config(const char *codes_text, int interval_sec)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (!code_text_valid(codes_text)) return ESP_ERR_INVALID_ARG;

    stock_config_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    int count = 0;
    const stock_codes_status_t status =
        stock_codes_parse(codes_text, parsed.codes, STOCK_CODES_MAX, &count, NULL);
    if (status != STOCK_CODES_OK) return ESP_ERR_INVALID_ARG;

    lock();
    stock_config_t next = s_config;
    memcpy(next.codes, parsed.codes, sizeof(next.codes));
    next.count = count;
    next.interval_sec = clamp_interval(interval_sec);

    char joined[STOCK_CODES_MAX * STOCK_CODE_LEN + 8];
    if (stock_codes_join(joined, sizeof(joined), next.codes, next.count) < 0) {
        unlock();
        return ESP_ERR_INVALID_SIZE;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = write_string(handle, KEY_CODES, joined);
        if (err == ESP_OK) {
            err = nvs_set_u16(handle, KEY_INTERVAL, next.interval_sec);
        }
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_config = next;
    unlock();
    return err;
}

esp_err_t config_store_save_wifi(const char *ssid, const char *pass)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;
    if (!ssid) ssid = "";
    if (!pass) pass = "";
    if (strlen(ssid) >= STOCK_CONFIG_SSID_MAX || strlen(pass) >= STOCK_CONFIG_PASS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    lock();
    stock_config_t next = s_config;
    snprintf(next.ssid, sizeof(next.ssid), "%s", ssid);
    snprintf(next.pass, sizeof(next.pass), "%s", pass);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = write_string(handle, KEY_SSID, next.ssid);
        if (err == ESP_OK) err = write_string(handle, KEY_PASS, next.pass);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_config = next;
    unlock();
    return err;
}

esp_err_t config_store_forget_wifi(void)
{
    return config_store_save_wifi("", "");
}

esp_err_t config_store_save_display(uint8_t brightness, uint8_t auto_off_sec,
                                    bool auto_off_enabled)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    lock();
    stock_config_t next = s_config;
    next.brightness = clamp_brightness(brightness);
    next.auto_off_sec = clamp_auto_off(auto_off_sec);
    next.auto_off_enabled = auto_off_enabled;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, KEY_BRIGHTNESS, next.brightness);
        if (err == ESP_OK) err = nvs_set_u8(handle, KEY_AUTO_OFF, next.auto_off_sec);
        if (err == ESP_OK) {
            err = nvs_set_u8(handle, KEY_AUTO_OFF_ENABLED, auto_off_enabled ? 1 : 0);
        }
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_config = next;
    unlock();
    return err;
}

esp_err_t config_store_save_ap_enabled(bool enabled)
{
    if (!s_ready) return ESP_ERR_INVALID_STATE;

    lock();
    stock_config_t next = s_config;
    next.ap_enabled = enabled;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(CONFIG_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_u8(handle, KEY_AP_ENABLED, enabled ? 1 : 0);
        if (err == ESP_OK) err = nvs_commit(handle);
        nvs_close(handle);
    }
    if (err == ESP_OK) s_config = next;
    unlock();
    return err;
}
