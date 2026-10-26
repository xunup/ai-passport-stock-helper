// main/web_server.c —— httpd:配置页 + 状态/配置/扫描/Wi-Fi API。
#include "web_server.h"

#include "config_store.h"
#include "quote_format.h"
#include "quote_parse.h"
#include "quote_worker.h"
#include "stock_codes.h"
#include "wifi_app.h"

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_wifi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "web_server";

#define AP_IP_TEXT "192.168.4.1"
#define MAX_BODY 1024

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

static httpd_handle_t s_server;

static esp_err_t send_json(httpd_req_t *req, cJSON *root, const char *status)
{
    char *text = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!text) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON encode failed");
        return ESP_ERR_NO_MEM;
    }
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    if (status) httpd_resp_set_status(req, status);
    const esp_err_t err = httpd_resp_sendstr(req, text);
    free(text);
    return err;
}

static esp_err_t send_error(httpd_req_t *req, const char *status, const char *message)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", false);
    cJSON_AddStringToObject(root, "error", message);
    return send_json(req, root, status);
}

static esp_err_t read_body(httpd_req_t *req, char *buffer, size_t capacity)
{
    const size_t length = req->content_len;
    if (length == 0 || length >= capacity) return ESP_ERR_INVALID_SIZE;
    size_t received = 0;
    while (received < length) {
        const int ret = httpd_req_recv(req, buffer + received, length - received);
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (ret <= 0) return ESP_FAIL;
        received += (size_t)ret;
    }
    buffer[received] = '\0';
    return ESP_OK;
}

static void add_config(cJSON *parent, const char *name, const stock_config_t *cfg)
{
    char joined[STOCK_CODES_MAX * STOCK_CODE_LEN + 8];
    if (stock_codes_join(joined, sizeof(joined), cfg->codes, cfg->count) < 0) {
        joined[0] = '\0';
    }
    cJSON *object = cJSON_AddObjectToObject(parent, name);
    if (!object) return;
    cJSON_AddStringToObject(object, "codes_text", joined);
    cJSON_AddNumberToObject(object, "interval", cfg->interval_sec);
    cJSON_AddNumberToObject(object, "brightness", cfg->brightness);
    cJSON_AddNumberToObject(object, "auto_off_sec", cfg->auto_off_sec);
    cJSON_AddBoolToObject(object, "auto_off_enabled", cfg->auto_off_enabled);
    cJSON_AddStringToObject(object, "ssid", cfg->ssid);
    cJSON_AddBoolToObject(object, "configured", cfg->ssid[0] != '\0' && cfg->count > 0);
    cJSON *codes = cJSON_AddArrayToObject(object, "codes");
    for (int i = 0; codes && i < cfg->count; i++) {
        cJSON_AddItemToArray(codes, cJSON_CreateString(cfg->codes[i]));
    }
}

static esp_err_t root_get(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    httpd_resp_set_hdr(req, "Content-Security-Policy",
                       "default-src 'self'; style-src 'unsafe-inline'; "
                       "script-src 'unsafe-inline'; img-src 'self' data:");
    const size_t length = (size_t)(index_html_end - index_html_start);
    return httpd_resp_send(req, (const char *)index_html_start,
                           length > 0 ? length - 1 : 0);
}

static esp_err_t status_get(httpd_req_t *req)
{
    stock_config_t cfg;
    config_store_get(&cfg);
    wifi_app_info_t wifi;
    wifi_app_get_info(&wifi);
    quote_snapshot_t quotes;
    quote_worker_snapshot(&quotes);

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "configured", cfg.ssid[0] != '\0' && cfg.count > 0);

    cJSON *device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "ap_ssid",
                            wifi.ap_ssid[0] ? wifi.ap_ssid : "FoloStock");
    cJSON_AddStringToObject(device, "ap_ip", AP_IP_TEXT);
    cJSON_AddBoolToObject(device, "ap_enabled", wifi.ap_enabled);

    cJSON *wifi_json = cJSON_AddObjectToObject(root, "wifi");
    cJSON_AddBoolToObject(wifi_json, "configured", wifi.sta_configured);
    cJSON_AddBoolToObject(wifi_json, "connected", wifi.sta_connected);
    cJSON_AddStringToObject(wifi_json, "ssid", wifi.sta_ssid);
    cJSON_AddStringToObject(wifi_json, "ip", wifi.ip);
    cJSON_AddNumberToObject(wifi_json, "rssi", wifi.rssi);
    cJSON_AddBoolToObject(wifi_json, "portal", wifi.portal);
    cJSON_AddNumberToObject(wifi_json, "fail_count", wifi.fail_count);

    add_config(root, "config", &cfg);

    cJSON *quote_json = cJSON_AddObjectToObject(root, "quotes");
    cJSON_AddStringToObject(quote_json, "error", quotes.error);
    cJSON_AddBoolToObject(quote_json, "last_ok", quotes.last_ok);
    cJSON_AddBoolToObject(quote_json, "has_data", quotes.has_data);
    cJSON_AddNumberToObject(quote_json, "updated_age", quotes.age_sec);
    cJSON *items = cJSON_AddArrayToObject(quote_json, "items");
    for (int i = 0; items && i < cfg.count; i++) {
        const char *code = cfg.codes[i];
        const quote_t *quote = quote_list_find(quotes.quotes, (size_t)quotes.count, code);
        cJSON *item = cJSON_CreateObject();
        if (!item) break;
        cJSON_AddStringToObject(item, "code", code);
        cJSON_AddStringToObject(item, "name", quote ? quote->name : "");
        cJSON_AddBoolToObject(item, "valid", quote && quote->valid);

        char price_text[24] = "--";
        char change_text[24] = "--";
        char pct_text[16] = "--";
        const char *trend = "flat";
        if (quote && quote->valid) {
            quote_format_price(price_text, sizeof(price_text), code, quote->price);
            quote_format_signed(change_text, sizeof(change_text), quote->change,
                                quote_price_decimals(code));
            quote_format_percent(pct_text, sizeof(pct_text), quote->change_pct);
            const quote_trend_t direction = quote_trend(quote->change);
            trend = direction == QUOTE_TREND_UP ? "up" :
                    (direction == QUOTE_TREND_DOWN ? "down" : "flat");
        }
        cJSON_AddStringToObject(item, "price", price_text);
        cJSON_AddStringToObject(item, "change", change_text);
        cJSON_AddStringToObject(item, "pct", pct_text);
        cJSON_AddStringToObject(item, "trend", trend);
        cJSON_AddItemToArray(items, item);
    }

    return send_json(req, root, NULL);
}

static esp_err_t config_get(httpd_req_t *req)
{
    stock_config_t cfg;
    config_store_get(&cfg);
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    add_config(root, "config", &cfg);
    return send_json(req, root, NULL);
}

static esp_err_t config_post(httpd_req_t *req)
{
    char body[MAX_BODY];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        return send_error(req, "400 Bad Request", "请求体过大或为空");
    }
    cJSON *json = cJSON_Parse(body);
    if (!json) return send_error(req, "400 Bad Request", "JSON 格式不正确");

    const cJSON *codes_item = cJSON_GetObjectItemCaseSensitive(json, "codes");
    if (!cJSON_IsString(codes_item) || !codes_item->valuestring) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "缺少股票代码");
    }

    stock_config_t current;
    config_store_get(&current);
    int interval = current.interval_sec;
    const cJSON *interval_item = cJSON_GetObjectItemCaseSensitive(json, "interval");
    if (cJSON_IsNumber(interval_item)) {
        interval = interval_item->valueint;
    } else if (interval_item) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "刷新间隔必须是数字");
    }
    if (interval < STOCK_CONFIG_INTERVAL_MIN || interval > STOCK_CONFIG_INTERVAL_MAX) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "刷新间隔需在 3 到 300 秒之间");
    }

    char codes[STOCK_CODES_MAX][STOCK_CODE_LEN];
    int count = 0;
    int bad_index = 0;
    const stock_codes_status_t parse_status =
        stock_codes_parse(codes_item->valuestring, codes, STOCK_CODES_MAX, &count, &bad_index);
    if (parse_status != STOCK_CODES_OK) {
        char message[96];
        if (parse_status == STOCK_CODES_ERR_INVALID) {
            snprintf(message, sizeof(message), "第 %d 项代码不正确", bad_index);
        } else {
            snprintf(message, sizeof(message), "%s", stock_codes_status_text(parse_status));
        }
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", message);
    }

    const esp_err_t err = config_store_save_config(codes_item->valuestring, interval);
    cJSON_Delete(json);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "保存自选股失败: %s", esp_err_to_name(err));
        return send_error(req, "500 Internal Server Error", "保存失败，请重试");
    }

    stock_config_t saved;
    config_store_get(&saved);
    char joined[STOCK_CODES_MAX * STOCK_CODE_LEN + 8];
    stock_codes_join(joined, sizeof(joined), saved.codes, saved.count);
    quote_worker_reload(joined, saved.interval_sec);

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", true);
    add_config(root, "config", &saved);
    return send_json(req, root, NULL);
}

static esp_err_t settings_post(httpd_req_t *req)
{
    char body[MAX_BODY];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        return send_error(req, "400 Bad Request", "请求体过大或为空");
    }
    cJSON *json = cJSON_Parse(body);
    if (!json) return send_error(req, "400 Bad Request", "JSON 格式不正确");

    stock_config_t current;
    config_store_get(&current);
    int brightness = current.brightness;
    int auto_off_sec = current.auto_off_sec;
    bool auto_off_enabled = current.auto_off_enabled;
    bool ap_enabled = current.ap_enabled;

    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, "brightness");
    if (item) {
        if (!cJSON_IsNumber(item) || item->valueint < STOCK_CONFIG_BRIGHTNESS_MIN ||
            item->valueint > STOCK_CONFIG_BRIGHTNESS_MAX) {
            cJSON_Delete(json);
            return send_error(req, "400 Bad Request", "亮度需在 1 到 5 档之间");
        }
        brightness = item->valueint;
    }
    item = cJSON_GetObjectItemCaseSensitive(json, "auto_off_sec");
    if (item) {
        if (!cJSON_IsNumber(item) || item->valueint < STOCK_CONFIG_AUTO_OFF_MIN ||
            item->valueint > STOCK_CONFIG_AUTO_OFF_MAX ||
            item->valueint % STOCK_CONFIG_AUTO_OFF_STEP != 0) {
            cJSON_Delete(json);
            return send_error(req, "400 Bad Request", "息屏时间需为 5 到 60 秒之间的 5 秒倍数");
        }
        auto_off_sec = item->valueint;
    }
    item = cJSON_GetObjectItemCaseSensitive(json, "auto_off_enabled");
    if (item) {
        if (!cJSON_IsBool(item)) {
            cJSON_Delete(json);
            return send_error(req, "400 Bad Request", "自动息屏开关必须是布尔值");
        }
        auto_off_enabled = cJSON_IsTrue(item);
    }
    item = cJSON_GetObjectItemCaseSensitive(json, "ap_enabled");
    if (item) {
        if (!cJSON_IsBool(item)) {
            cJSON_Delete(json);
            return send_error(req, "400 Bad Request", "热点开关必须是布尔值");
        }
        ap_enabled = cJSON_IsTrue(item);
    }
    cJSON_Delete(json);

    const bool ap_changed = ap_enabled != current.ap_enabled;
    if (ap_changed) {
        const esp_err_t ap_err = wifi_app_set_ap_enabled(ap_enabled);
        if (ap_err != ESP_OK) {
            ESP_LOGW(TAG, "网页切换热点失败: %s", esp_err_to_name(ap_err));
            return send_error(req, "500 Internal Server Error", "无法切换配网热点");
        }
    }

    esp_err_t err = config_store_save_display((uint8_t)brightness, (uint8_t)auto_off_sec,
                                              auto_off_enabled);
    if (err == ESP_OK && ap_changed) {
        err = config_store_save_ap_enabled(ap_enabled);
    }
    if (err != ESP_OK) {
        if (ap_changed) (void)wifi_app_set_ap_enabled(current.ap_enabled);
        ESP_LOGE(TAG, "网页保存设备设置失败: %s", esp_err_to_name(err));
        return send_error(req, "500 Internal Server Error", "保存设备设置失败，请重试");
    }
    ESP_LOGI(TAG, "网页设置: 亮度 %d, 自动息屏 %s, %d 秒, 热点 %s", brightness,
             auto_off_enabled ? "开" : "关", auto_off_sec, ap_enabled ? "开" : "关");

    stock_config_t saved;
    config_store_get(&saved);
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddBoolToObject(root, "ap_enabled", wifi_app_ap_enabled());
    add_config(root, "config", &saved);
    return send_json(req, root, NULL);
}

static esp_err_t wifi_post(httpd_req_t *req)
{
    char body[MAX_BODY];
    if (read_body(req, body, sizeof(body)) != ESP_OK) {
        return send_error(req, "400 Bad Request", "请求体过大或为空");
    }
    cJSON *json = cJSON_Parse(body);
    if (!json) return send_error(req, "400 Bad Request", "JSON 格式不正确");

    const cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
    const cJSON *pass_item = cJSON_GetObjectItemCaseSensitive(json, "pass");
    if (!cJSON_IsString(ssid_item) || !ssid_item->valuestring ||
        strlen(ssid_item->valuestring) == 0) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "请填写 Wi-Fi 名称");
    }
    if (strlen(ssid_item->valuestring) > 31) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "Wi-Fi 名称过长");
    }
    const char *pass = cJSON_IsString(pass_item) && pass_item->valuestring
                           ? pass_item->valuestring : "";
    if (strlen(pass) > 63) {
        cJSON_Delete(json);
        return send_error(req, "400 Bad Request", "Wi-Fi 密码过长");
    }

    char ssid[32];
    char password[64];
    snprintf(ssid, sizeof(ssid), "%s", ssid_item->valuestring);
    snprintf(password, sizeof(password), "%s", pass);
    cJSON_Delete(json);

    esp_err_t err = config_store_save_wifi(ssid, password);
    if (err != ESP_OK) {
        return send_error(req, "500 Internal Server Error", "保存 Wi-Fi 失败");
    }
    err = wifi_app_apply(ssid, password);
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "应用 Wi-Fi 失败: %s", esp_err_to_name(err));
        return send_error(req, "500 Internal Server Error", "无法启动 Wi-Fi 连接");
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", true);
    return send_json(req, root, NULL);
}

static esp_err_t wifi_forget_post(httpd_req_t *req)
{
    (void)req;
    const esp_err_t err = config_store_forget_wifi();
    if (err != ESP_OK) {
        return send_error(req, "500 Internal Server Error", "清除 Wi-Fi 失败");
    }
    wifi_app_forget();
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", true);
    return send_json(req, root, NULL);
}

static esp_err_t portal_close_post(httpd_req_t *req)
{
    stock_config_t cfg;
    config_store_get(&cfg);
    if (cfg.ssid[0] == '\0' || cfg.count == 0) {
        return send_error(req, "409 Conflict", "尚未完成 Wi-Fi 和自选股配置");
    }
    wifi_app_dismiss_portal();
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON_AddBoolToObject(root, "ok", true);
    return send_json(req, root, NULL);
}

static esp_err_t scan_get(httpd_req_t *req)
{
    wifi_app_ap_t networks[WIFI_APP_SCAN_MAX];
    const int count = wifi_app_scan(networks, WIFI_APP_SCAN_MAX);
    if (count < 0) {
        return send_error(req, "503 Service Unavailable", "扫描失败，请稍后重试");
    }
    cJSON *root = cJSON_CreateObject();
    if (!root) return ESP_ERR_NO_MEM;
    cJSON *items = cJSON_AddArrayToObject(root, "networks");
    for (int i = 0; items && i < count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) break;
        cJSON_AddStringToObject(item, "ssid", networks[i].ssid);
        cJSON_AddNumberToObject(item, "rssi", networks[i].rssi);
        cJSON_AddBoolToObject(item, "secure", networks[i].secure);
        cJSON_AddItemToArray(items, item);
    }
    return send_json(req, root, NULL);
}

static esp_err_t register_handler(httpd_handle_t server, const char *uri,
                                  httpd_method_t method,
                                  esp_err_t (*handler)(httpd_req_t *))
{
    httpd_uri_t item = {
        .uri = uri,
        .method = method,
        .handler = handler,
        .user_ctx = NULL,
    };
    return httpd_register_uri_handler(server, &item);
}

esp_err_t web_server_start(void)
{
    if (s_server) return ESP_OK;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务启动失败: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    err = register_handler(s_server, "/", HTTP_GET, root_get);
    if (err == ESP_OK) err = register_handler(s_server, "/api/status", HTTP_GET, status_get);
    if (err == ESP_OK) err = register_handler(s_server, "/api/config", HTTP_GET, config_get);
    if (err == ESP_OK) err = register_handler(s_server, "/api/config", HTTP_POST, config_post);
    if (err == ESP_OK) err = register_handler(s_server, "/api/settings", HTTP_POST,
                                              settings_post);
    if (err == ESP_OK) err = register_handler(s_server, "/api/scan", HTTP_GET, scan_get);
    if (err == ESP_OK) err = register_handler(s_server, "/api/wifi", HTTP_POST, wifi_post);
    if (err == ESP_OK) err = register_handler(s_server, "/api/wifi/forget", HTTP_POST,
                                              wifi_forget_post);
    if (err == ESP_OK) err = register_handler(s_server, "/api/portal/close", HTTP_POST,
                                              portal_close_post);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "注册 HTTP 路由失败: %s", esp_err_to_name(err));
        web_server_stop();
        return err;
    }
    ESP_LOGI(TAG, "配置页已启动: http://%s/", AP_IP_TEXT);
    return ESP_OK;
}

void web_server_stop(void)
{
    if (!s_server) return;
    httpd_stop(s_server);
    s_server = NULL;
}
