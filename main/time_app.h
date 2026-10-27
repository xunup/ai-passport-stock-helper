// main/time_app.h —— SNTP 网络对时(中国标准时间 UTC+8)。
#pragma once

#include <stdbool.h>

#include "esp_err.h"

// 启动后持续按小时重同步;无网络时保持未对时状态。
esp_err_t time_app_start(void);

// 已取得有效时间时返回 true 并输出本地时的时/分。
bool time_app_now_hm(int *hour, int *minute);

// 同上,并可取得秒;任意输出参数可为 NULL。
bool time_app_now_hms(int *hour, int *minute, int *second);
