// main/time_app.c —— SNTP 对时,时间按 UTC+8 解释。
#include "time_app.h"

#include "esp_log.h"
#include "esp_netif_sntp.h"

#include <stdlib.h>
#include <time.h>

static const char *TAG = "time_app";

#define NTP_SERVER "ntp.aliyun.com"
// 早于 2024-01-01 说明尚未对时,避免把开机计时当成真实时间。
#define VALID_EPOCH 1704067200LL

esp_err_t time_app_start(void)
{
    setenv("TZ", "CST-8", 1);
    tzset();

    const esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    const esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SNTP 启动失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "SNTP 对时已启动: %s (UTC+8)", NTP_SERVER);
    return ESP_OK;
}

bool time_app_now_hm(int *hour, int *minute)
{
    return time_app_now_hms(hour, minute, NULL);
}

bool time_app_now_hms(int *hour, int *minute, int *second)
{
    const time_t now = time(NULL);
    if (now < VALID_EPOCH) return false;

    struct tm local;
    if (!localtime_r(&now, &local)) return false;
    if (hour) *hour = local.tm_hour;
    if (minute) *minute = local.tm_min;
    if (second) *second = local.tm_sec;
    return true;
}
