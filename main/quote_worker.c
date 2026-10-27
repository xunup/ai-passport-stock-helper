// main/quote_worker.c —— 新浪 hq.sinajs.cn 轮询任务。
#include "quote_worker.h"

#include "market_hours.h"
#include "refresh_options.h"
#include "time_app.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

#include "wifi_app.h"

static const char *TAG = "quote_worker";

#define QUOTE_URL_MAX 160
#define QUOTE_BODY_MAX 4096
#define QUOTE_TASK_STACK 6144
#define QUOTE_TASK_PRIORITY 5
#define QUOTE_RETRY_MS 1500
#define QUOTE_IDLE_INTERVAL_SEC 60
#define QUOTE_CLOSED_CHECK_SEC 60

static SemaphoreHandle_t s_lock;
static TaskHandle_t s_task;
static char s_codes[STOCK_CODES_MAX * STOCK_CODE_LEN + 8];
static uint16_t s_interval_sec;
static bool s_idle;
static quote_t s_quotes[STOCK_CODES_MAX];
static int s_count;
static bool s_has_data;
static bool s_last_ok;
static bool s_fetching;
static char s_error[48];
static int64_t s_last_success_ms;
static uint32_t s_generation;

static char s_body[QUOTE_BODY_MAX];

static void set_state(bool last_ok, const char *error)
{
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
    const char *next_error = error ? error : "";
    const bool changed = s_last_ok != last_ok || strcmp(s_error, next_error) != 0;
    s_last_ok = last_ok;
    snprintf(s_error, sizeof(s_error), "%s", next_error);
    if (last_ok) s_last_success_ms = esp_timer_get_time() / 1000;
    if (changed) s_generation++;
    xSemaphoreGive(s_lock);
}

static void set_fetching(bool fetching)
{
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
    if (s_fetching != fetching) {
        s_fetching = fetching;
        s_generation++;
    }
    xSemaphoreGive(s_lock);
}

static void copy_config(char *codes, size_t codes_size, uint16_t *interval)
{
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        snprintf(codes, codes_size, "%s", s_codes);
        *interval = s_interval_sec;
        xSemaphoreGive(s_lock);
    } else {
        codes[0] = '\0';
        *interval = 5;
    }
}

static esp_err_t fetch_body(const char *url, char *body, size_t body_size, size_t *body_len)
{
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 8000,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_ERR_NO_MEM;

    esp_http_client_set_header(client, "Referer", "https://finance.sina.com.cn");
    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0 (FoloToy Stock Ticker)");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }
    (void)esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }

    size_t used = 0;
    while (used + 1 < body_size) {
        const int read = esp_http_client_read(client, body + used, (int)(body_size - 1 - used));
        if (read < 0) {
            err = ESP_ERR_INVALID_RESPONSE;
            break;
        }
        if (read == 0) break;
        used += (size_t)read;
    }
    body[used] = '\0';
    *body_len = used;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return err;
}

static void poll_once(void)
{
    char codes[sizeof(s_codes)];
    uint16_t interval = 5;
    copy_config(codes, sizeof(codes), &interval);

    if (codes[0] == '\0') {
        set_state(false, "未设置自选股");
        vTaskDelay(pdMS_TO_TICKS(2000));
        return;
    }
    if (!wifi_app_sta_connected()) {
        set_state(false, "Wi-Fi 未连接");
        vTaskDelay(pdMS_TO_TICKS(QUOTE_RETRY_MS));
        return;
    }

    char url[QUOTE_URL_MAX];
    const int url_len = snprintf(url, sizeof(url), "http://hq.sinajs.cn/list=%s", codes);
    if (url_len < 0 || (size_t)url_len >= sizeof(url)) {
        set_state(false, "股票代码过长");
        return;
    }

    set_fetching(true);
    size_t body_len = 0;
    esp_err_t err = fetch_body(url, s_body, sizeof(s_body), &body_len);
    if (err != ESP_OK) {
        set_fetching(false);
        ESP_LOGW(TAG, "行情请求失败: %s", esp_err_to_name(err));
        set_state(false, "行情服务器无响应");
        return;
    }

    quote_t parsed[STOCK_CODES_MAX];
    const size_t parsed_count =
        quote_parse_response(s_body, body_len, parsed, STOCK_CODES_MAX);
    if (parsed_count == 0) {
        set_fetching(false);
        ESP_LOGW(TAG, "行情解析失败 (%u 字节)", (unsigned)body_len);
        set_state(false, "行情解析失败");
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        memcpy(s_quotes, parsed, parsed_count * sizeof(parsed[0]));
        s_count = (int)parsed_count;
        s_has_data = true;
        s_fetching = false;
        s_last_ok = true;
        s_error[0] = '\0';
        s_last_success_ms = esp_timer_get_time() / 1000;
        s_generation++;
        xSemaphoreGive(s_lock);
    }
    ESP_LOGI(TAG, "已更新 %u 条行情", (unsigned)parsed_count);
}

// 未对时时不拦截自动刷新,避免时间无效导致行情永远不更新。
static bool market_is_open(void)
{
    int hour = 0;
    int minute = 0;
    if (!time_app_now_hm(&hour, &minute)) return true;
    return market_hours_is_trading(hour, minute);
}

static uint32_t market_closed_wait_ms(void)
{
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!time_app_now_hms(&hour, &minute, &second)) {
        return QUOTE_CLOSED_CHECK_SEC * 1000U;
    }

    int wait_sec = market_hours_seconds_until_open(hour, minute, second);
    if (wait_sec < 1) wait_sec = 1;
    if (wait_sec > QUOTE_CLOSED_CHECK_SEC) wait_sec = QUOTE_CLOSED_CHECK_SEC;
    return (uint32_t)wait_sec * 1000U;
}

static void quote_task(void *arg)
{
    (void)arg;
    // 首次启动先拉一次,休市开机也能显示最近收盘行情。
    bool requested = true;
    for (;;) {
        const bool trading = market_is_open();
        if (requested || trading) poll_once();

        uint16_t interval = 5;
        bool idle = false;
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
            interval = s_interval_sec;
            idle = s_idle;
            xSemaphoreGive(s_lock);
        }

        if (interval == REFRESH_INTERVAL_MANUAL) {
            // 手动模式:不自动轮询,等 OK 键或配置变更通知。
            requested = ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0;
            continue;
        }
        if (!trading) {
            // 非交易时段不自动刷新,只按开盘时刻唤醒做时间检查。
            requested = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(market_closed_wait_ms())) > 0;
            continue;
        }
        if (idle) interval = QUOTE_IDLE_INTERVAL_SEC;
        requested = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS((uint32_t)interval * 1000U)) > 0;
    }
}

esp_err_t quote_worker_start(const char *codes_text, int interval_sec)
{
    if (s_task) return ESP_OK;
    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
        if (!s_lock) return ESP_ERR_NO_MEM;
    }
    if (!codes_text) codes_text = "";
    snprintf(s_codes, sizeof(s_codes), "%s", codes_text);
    s_interval_sec = (uint16_t)(interval_sec >= 0 ? interval_sec : 5);
    s_error[0] = '\0';
    s_error[sizeof(s_error) - 1] = '\0';

    if (xTaskCreate(quote_task, "quote_worker", QUOTE_TASK_STACK, NULL,
                    QUOTE_TASK_PRIORITY, &s_task) != pdPASS) {
        s_task = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void quote_worker_reload(const char *codes_text, int interval_sec)
{
    if (!s_lock) return;
    if (!codes_text) codes_text = "";
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        snprintf(s_codes, sizeof(s_codes), "%s", codes_text);
        s_interval_sec = (uint16_t)(interval_sec >= 0 ? interval_sec : 5);
        s_count = 0;
        s_has_data = false;
        s_last_ok = false;
        s_fetching = false;
        s_error[0] = '\0';
        s_generation++;
        xSemaphoreGive(s_lock);
    }
    quote_worker_request_refresh();
}

void quote_worker_set_interval(uint16_t interval_sec)
{
    if (!s_lock) return;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) == pdTRUE) {
        s_interval_sec = interval_sec;
        xSemaphoreGive(s_lock);
    }
    quote_worker_request_refresh();
}

void quote_worker_request_refresh(void)
{
    if (s_task) xTaskNotifyGive(s_task);
}

void quote_worker_set_idle(bool idle)
{
    if (!s_lock) return;
    bool changed = false;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        changed = s_idle != idle;
        s_idle = idle;
        xSemaphoreGive(s_lock);
    }
    if (changed && !idle) quote_worker_request_refresh();
}

void quote_worker_snapshot(quote_snapshot_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!s_lock) {
        snprintf(out->error, sizeof(out->error), "%s", "行情服务未启动");
        return;
    }
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(200)) != pdTRUE) {
        snprintf(out->error, sizeof(out->error), "%s", "行情忙");
        return;
    }
    out->count = s_count;
    memcpy(out->quotes, s_quotes, sizeof(out->quotes));
    out->last_ok = s_last_ok;
    out->has_data = s_has_data;
    out->fetching = s_fetching;
    out->generation = s_generation;
    snprintf(out->error, sizeof(out->error), "%s", s_error);
    if (s_has_data) {
        const int64_t age_ms = esp_timer_get_time() / 1000 - s_last_success_ms;
        out->age_sec = age_ms > 0 ? (uint32_t)(age_ms / 1000) : 0;
    }
    xSemaphoreGive(s_lock);
}
