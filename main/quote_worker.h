// main/quote_worker.h —— 后台轮询新浪行情并维护一份线程安全快照。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "quote_parse.h"
#include "stock_codes.h"

typedef struct {
    quote_t quotes[STOCK_CODES_MAX];
    int count;
    bool last_ok;
    bool has_data;
    bool fetching;
    uint32_t age_sec;
    uint32_t generation;
    char error[48];
} quote_snapshot_t;

esp_err_t quote_worker_start(const char *codes_text, int interval_sec);
void quote_worker_reload(const char *codes_text, int interval_sec);
// 只改刷新间隔并立即按新间隔重新排程;0 表示手动,不再自动轮询。
void quote_worker_set_interval(uint16_t interval_sec);
void quote_worker_request_refresh(void);
// 息屏时降到每 60 秒一次;唤醒后恢复用户设置的间隔并立即刷新。
void quote_worker_set_idle(bool idle);
void quote_worker_snapshot(quote_snapshot_t *out);
