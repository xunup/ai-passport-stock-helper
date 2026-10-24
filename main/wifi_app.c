// main/wifi_app.c —— ESP32-C3 的可开关 AP + STA,并在弱信号时自动漫游。
#include "wifi_app.h"

#include "roam_policy.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "wifi_app";

#define AP_PREFIX "FoloStock-"
#define AP_CHANNEL 6
#define AP_MAX_CLIENTS 4
#define RETRY_DELAY_US (5 * 1000 * 1000)
#define PORTAL_FAIL_THRESHOLD 3
#define ROAM_PERIOD_MS (30 * 1000)
#define ROAM_TASK_STACK 4096
#define ROAM_TASK_PRIORITY 3
#define ROAM_SCAN_MAX 8

static wifi_app_info_t s_info;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static esp_netif_t *s_ap_netif;
static esp_netif_t *s_sta_netif;
static esp_timer_handle_t s_retry_timer;
static bool s_started;
static bool s_portal_forced;
static bool s_portal_dismissed;
static bool s_roam_in_progress;
static bool s_roam_target_valid;

static void lock_state(void) { taskENTER_CRITICAL(&s_mux); }
static void unlock_state(void) { taskEXIT_CRITICAL(&s_mux); }

static bool utf8_valid_at(const uint8_t *text, size_t remaining, size_t *sequence_len)
{
    const uint8_t lead = text[0];
    size_t len = 0;
    if (lead < 0x80) {
        len = 1;
    } else if ((lead & 0xE0) == 0xC0) {
        len = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        len = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        len = 4;
    } else {
        return false;
    }
    if (len > remaining) return false;
    for (size_t i = 1; i < len; i++) {
        if ((text[i] & 0xC0) != 0x80) return false;
    }
    *sequence_len = len;
    return true;
}

// SSID 是任意字节;JSON 必须是合法 UTF-8,所以非法字节替换成 '?'。
static void sanitize_ssid(const uint8_t *src, size_t len, char *out, size_t out_size)
{
    size_t used = 0;
    size_t i = 0;
    while (i < len && used + 1 < out_size) {
        size_t sequence_len = 0;
        if (src[i] >= 0x20 && src[i] < 0x7F) {
            out[used++] = (char)src[i++];
            continue;
        }
        if (src[i] != 0x7F && utf8_valid_at(src + i, len - i, &sequence_len) &&
            sequence_len > 1 && used + sequence_len < out_size) {
            memcpy(out + used, src + i, sequence_len);
            used += sequence_len;
            i += sequence_len;
            continue;
        }
        out[used++] = '?';
        i++;
    }
    out[used] = '\0';
}

static void snapshot(wifi_app_info_t *out)
{
    lock_state();
    *out = s_info;
    unlock_state();
}

static bool retry_due(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    return s_started && info.sta_configured && !info.sta_connected;
}

static void retry_timer_cb(void *arg)
{
    (void)arg;
    if (retry_due()) esp_wifi_connect();
}

static void schedule_retry(void)
{
    if (s_retry_timer) esp_timer_start_once(s_retry_timer, RETRY_DELAY_US);
}

static void publish_portal_state(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    const bool portal = !info.sta_configured || s_portal_forced ||
                        (!info.sta_connected && info.fail_count >= PORTAL_FAIL_THRESHOLD &&
                         !s_portal_dismissed);
    lock_state();
    s_info.portal = portal;
    unlock_state();
}

static void set_sta_credentials(const char *ssid, const char *pass)
{
    wifi_config_t cfg = {0};
    if (ssid && ssid[0]) {
        snprintf((char *)cfg.sta.ssid, sizeof(cfg.sta.ssid), "%s", ssid);
        snprintf((char *)cfg.sta.password, sizeof(cfg.sta.password), "%s", pass ? pass : "");
        cfg.sta.threshold.authmode = WIFI_AUTH_OPEN;
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
    }
}

static esp_err_t connect_sta(void)
{
    esp_wifi_disconnect();
    const esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_STARTED) {
        ESP_LOGW(TAG, "STA 连接请求失败: %s", esp_err_to_name(err));
    }
    return err;
}

// 漫游成功后解除 BSSID 锁定,后续仍由驱动正常选网。
static void clear_bssid_pin(void)
{
    wifi_config_t cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK || !cfg.sta.bssid_set) return;
    cfg.sta.bssid_set = false;
    cfg.sta.channel = 0;
    memset(cfg.sta.bssid, 0, sizeof(cfg.sta.bssid));
    const esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) ESP_LOGW(TAG, "解除 BSSID 锁定失败: %s", esp_err_to_name(err));
}

static void cancel_roam(bool clear_pin)
{
    lock_state();
    s_roam_in_progress = false;
    s_roam_target_valid = false;
    unlock_state();
    if (clear_pin) clear_bssid_pin();
}

// 周期性检查 STA 信号;弱于门限时扫描同名 SSID,明显更强才切 BSSID。
static void roam_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ROAM_PERIOD_MS));

        wifi_app_info_t info;
        snapshot(&info);
        if (!s_started || !info.sta_connected || info.portal) continue;

        bool busy = false;
        lock_state();
        busy = s_roam_in_progress;
        unlock_state();
        if (busy) continue;

        wifi_ap_record_t current;
        if (esp_wifi_sta_get_ap_info(&current) != ESP_OK) continue;
        lock_state();
        s_info.rssi = current.rssi;
        unlock_state();
        if (current.rssi >= ROAM_TRIGGER_RSSI) continue;

        wifi_scan_config_t scan = {0};
        scan.ssid = (uint8_t *)info.sta_ssid;
        scan.show_hidden = false;
        esp_err_t err = esp_wifi_scan_start(&scan, true);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "漫游扫描跳过: %s", esp_err_to_name(err));
            continue;
        }

        uint16_t number = ROAM_SCAN_MAX;
        wifi_ap_record_t records[ROAM_SCAN_MAX];
        memset(records, 0, sizeof(records));
        err = esp_wifi_scan_get_ap_records(&number, records);
        if (err != ESP_OK) {
            esp_wifi_clear_ap_list();
            continue;
        }
        if (number == 0) {
            esp_wifi_clear_ap_list();
            continue;
        }

        int best = -1;
        for (int i = 0; i < number; i++) {
            if (memcmp(records[i].bssid, current.bssid, sizeof(current.bssid)) == 0) continue;
            if (records[i].authmode != current.authmode) continue;
            if (!roam_policy_should_switch(current.rssi, records[i].rssi)) continue;
            if (best < 0 || records[i].rssi > records[best].rssi) best = i;
        }
        if (best < 0) continue;

        wifi_config_t cfg;
        if (esp_wifi_get_config(WIFI_IF_STA, &cfg) != ESP_OK) continue;
        memcpy(cfg.sta.bssid, records[best].bssid, sizeof(cfg.sta.bssid));
        cfg.sta.bssid_set = true;
        cfg.sta.channel = records[best].primary;

        ESP_LOGI(TAG, "漫游: 信号 %d dBm,切换到 %d dBm 的 BSSID(信道 %u)",
                 current.rssi, records[best].rssi, records[best].primary);

        lock_state();
        s_roam_in_progress = true;
        s_roam_target_valid = true;
        unlock_state();

        err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
        if (err == ESP_OK) err = esp_wifi_disconnect();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "漫游切换失败: %s", esp_err_to_name(err));
            cancel_roam(true);
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT) {
        switch (id) {
        case WIFI_EVENT_STA_START:
            if (retry_due()) connect_sta();
            break;
        case WIFI_EVENT_STA_DISCONNECTED: {
            const wifi_event_sta_disconnected_t *event = data;
            bool roaming = false;
            bool pinned = false;
            lock_state();
            roaming = s_roam_in_progress;
            pinned = s_roam_target_valid;
            s_roam_in_progress = false;
            s_info.sta_connected = false;
            if (!roaming && s_info.fail_count < 255) s_info.fail_count++;
            unlock_state();
            publish_portal_state();

            if (roaming) {
                // BSSID 已锁到目标 AP,立刻重连,不把这次主动断开算作失败。
                ESP_LOGI(TAG, "漫游断开,连接目标 AP");
                const esp_err_t err = esp_wifi_connect();
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "漫游重连失败: %s", esp_err_to_name(err));
                    cancel_roam(true);
                    schedule_retry();
                }
                break;
            }
            if (pinned) {
                // 目标 AP 也没连上,解除锁定,回到正常重试。
                cancel_roam(true);
            }
            ESP_LOGI(TAG, "STA 断开 (reason=%d),5 秒后重试", event ? event->reason : -1);
            schedule_retry();
            break;
        }
        case WIFI_EVENT_AP_START: {
            lock_state();
            s_info.ap_started = true;
            unlock_state();
            wifi_app_info_t info;
            snapshot(&info);
            ESP_LOGI(TAG, "配网热点已启动: %s", info.ap_ssid);
            break;
        }
        case WIFI_EVENT_AP_STOP:
            lock_state();
            s_info.ap_started = false;
            unlock_state();
            ESP_LOGI(TAG, "配网热点已关闭");
            break;
        default:
            break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = data;
        char ip[16] = {0};
        esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
        wifi_ap_record_t ap = {0};
        const bool have_ap = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
        lock_state();
        s_info.sta_connected = true;
        s_info.fail_count = 0;
        snprintf(s_info.ip, sizeof(s_info.ip), "%s", ip);
        s_info.rssi = have_ap ? ap.rssi : 0;
        unlock_state();
        s_portal_forced = false;
        s_portal_dismissed = false;
        publish_portal_state();
        bool pinned = false;
        lock_state();
        pinned = s_roam_target_valid;
        s_roam_target_valid = false;
        unlock_state();
        if (pinned) {
            clear_bssid_pin();
            ESP_LOGI(TAG, "漫游完成,已切到信号更好的 AP");
        }
        ESP_LOGI(TAG, "STA 已连接,IP=%s", ip);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        lock_state();
        s_info.sta_connected = false;
        s_info.ip[0] = '\0';
        unlock_state();
        publish_portal_state();
    }
}

static void make_ap_ssid(void)
{
    uint8_t mac[6] = {0};
    if (esp_wifi_get_mac(WIFI_IF_AP, mac) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    }
    snprintf(s_info.ap_ssid, sizeof(s_info.ap_ssid), AP_PREFIX "%02X%02X", mac[4], mac[5]);
}

static esp_err_t configure_ap(void)
{
    wifi_config_t cfg = {0};
    size_t ssid_len = strlen(s_info.ap_ssid);
    if (ssid_len > sizeof(cfg.ap.ssid)) ssid_len = sizeof(cfg.ap.ssid);
    memcpy(cfg.ap.ssid, s_info.ap_ssid, ssid_len);
    cfg.ap.ssid_len = (uint8_t)ssid_len;
    cfg.ap.channel = AP_CHANNEL;
    cfg.ap.max_connection = AP_MAX_CLIENTS;
    cfg.ap.authmode = WIFI_AUTH_OPEN;
    cfg.ap.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_AP, &cfg);
}

esp_err_t wifi_app_start(const char *ssid, const char *pass, bool ap_enabled)
{
    if (s_started) return ESP_OK;

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    s_ap_netif = esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_ap_netif || !s_sta_netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init_cfg);
    if (err != ESP_OK) return err;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              wifi_event_handler, NULL, NULL);
    if (err == ESP_OK) {
        err = esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                                  wifi_event_handler, NULL, NULL);
    }
    if (err != ESP_OK) return err;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;

    lock_state();
    s_info.ap_enabled = ap_enabled;
    s_info.sta_configured = ssid && ssid[0];
    snprintf(s_info.sta_ssid, sizeof(s_info.sta_ssid), "%s", ssid ? ssid : "");
    s_info.portal = !s_info.sta_configured;
    unlock_state();

    make_ap_ssid();

    err = esp_wifi_set_mode(ap_enabled ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    if (err != ESP_OK) return err;
    if (ap_enabled) {
        err = configure_ap();
        if (err != ESP_OK) return err;
    }
    if (s_info.sta_configured) {
        set_sta_credentials(ssid, pass);
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_WIFI_INIT_STATE) return err;
    s_started = true;

    const esp_timer_create_args_t timer_args = {
        .callback = retry_timer_cb,
        .name = "wifi_retry",
    };
    if (esp_timer_create(&timer_args, &s_retry_timer) != ESP_OK) s_retry_timer = NULL;

    if (xTaskCreate(roam_task, "wifi_roam", ROAM_TASK_STACK, NULL,
                    ROAM_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGW(TAG, "漫游任务创建失败,继续使用当前 AP");
    }

    wifi_app_info_t info;
    snapshot(&info);
    if (info.sta_configured) connect_sta();
    publish_portal_state();
    ESP_LOGI(TAG, "AP SSID=%s(%s), STA=%s", info.ap_ssid,
             info.ap_enabled ? "开" : "关",
             info.sta_configured ? info.sta_ssid : "(未配置)");
    return ESP_OK;
}

esp_err_t wifi_app_set_ap_enabled(bool enabled)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;

    wifi_app_info_t info;
    snapshot(&info);
    if (info.ap_enabled == enabled) return ESP_OK;

    lock_state();
    s_info.ap_enabled = enabled;
    s_info.ap_started = false;
    unlock_state();

    esp_err_t err = esp_wifi_set_mode(enabled ? WIFI_MODE_APSTA : WIFI_MODE_STA);
    if (err == ESP_OK && enabled) {
        err = configure_ap();
    }
    if (err != ESP_OK) {
        if (enabled) (void)esp_wifi_set_mode(WIFI_MODE_STA);
        lock_state();
        s_info.ap_enabled = !enabled;
        unlock_state();
        ESP_LOGE(TAG, "切换热点失败: %s", esp_err_to_name(err));
        return err;
    }

    publish_portal_state();
    if (info.sta_configured) {
        // 模式切换可能重置 STA;已连接时该调用无副作用。
        const esp_err_t conn = esp_wifi_connect();
        if (conn != ESP_OK) {
            ESP_LOGD(TAG, "热点切换后 STA 重连: %s", esp_err_to_name(conn));
        }
    }
    ESP_LOGI(TAG, "配网热点%s", enabled ? "已开启" : "已关闭");
    return ESP_OK;
}

bool wifi_app_ap_enabled(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    return info.ap_enabled;
}

esp_err_t wifi_app_apply(const char *ssid, const char *pass)
{
    if (!s_started) return ESP_ERR_INVALID_STATE;
    if (!ssid) ssid = "";
    if (!pass) pass = "";

    cancel_roam(true);

    lock_state();
    s_info.sta_configured = ssid[0] != '\0';
    snprintf(s_info.sta_ssid, sizeof(s_info.sta_ssid), "%s", ssid);
    s_info.fail_count = 0;
    s_info.sta_connected = false;
    s_info.ip[0] = '\0';
    unlock_state();
    s_portal_forced = false;
    s_portal_dismissed = false;
    publish_portal_state();

    if (!ssid[0]) {
        esp_wifi_disconnect();
        return ESP_OK;
    }
    set_sta_credentials(ssid, pass);
    return connect_sta();
}

esp_err_t wifi_app_forget(void)
{
    return wifi_app_apply("", "");
}

void wifi_app_dismiss_portal(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    if (!info.sta_configured) return;
    s_portal_forced = false;
    s_portal_dismissed = true;
    publish_portal_state();
}

bool wifi_app_toggle_portal(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    if (!info.sta_configured) return true;
    if (info.portal) {
        wifi_app_dismiss_portal();
    } else {
        s_portal_forced = true;
        s_portal_dismissed = false;
        publish_portal_state();
    }
    snapshot(&info);
    return info.portal;
}

bool wifi_app_portal_active(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    return info.portal;
}

bool wifi_app_sta_connected(void)
{
    wifi_app_info_t info;
    snapshot(&info);
    return info.sta_connected;
}

void wifi_app_get_info(wifi_app_info_t *out)
{
    if (out) snapshot(out);
}

int wifi_app_scan(wifi_app_ap_t *out, int max)
{
    if (!out || max <= 0) return -1;
    if (!s_started) return -1;

    wifi_scan_config_t scan_cfg = {0};
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "扫描失败: %s", esp_err_to_name(err));
        return -1;
    }

    uint16_t number = 0;
    esp_wifi_scan_get_ap_num(&number);
    if (number > WIFI_APP_SCAN_MAX) number = WIFI_APP_SCAN_MAX;

    wifi_ap_record_t records[WIFI_APP_SCAN_MAX];
    uint16_t returned = number;
    memset(records, 0, sizeof(records));
    if (returned > 0) {
        err = esp_wifi_scan_get_ap_records(&returned, records);
        if (err != ESP_OK) returned = 0;
    } else {
        esp_wifi_clear_ap_list();
    }
    if (returned > max) returned = (uint16_t)max;
    for (int i = 0; i < returned; i++) {
        sanitize_ssid(records[i].ssid, strnlen((const char *)records[i].ssid,
                                               sizeof(records[i].ssid)),
                      out[i].ssid, sizeof(out[i].ssid));
        out[i].rssi = records[i].rssi;
        out[i].secure = records[i].authmode != WIFI_AUTH_OPEN;
    }
    return returned;
}
