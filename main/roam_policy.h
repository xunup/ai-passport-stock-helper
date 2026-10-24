// main/roam_policy.h —— Wi-Fi 漫游判定(纯逻辑,便于主机测试)。
#pragma once

#include <stdbool.h>

// 当前信号不差于该值时不做漫游扫描。
#define ROAM_TRIGGER_RSSI (-70)
// 目标 AP 至少要比当前强这么多 dBm 才值得切。
#define ROAM_MIN_IMPROVEMENT_DB 8
// 目标 AP 自身至少要达到该信号,避免在几个弱 AP 之间来回跳。
#define ROAM_MIN_TARGET_RSSI (-72)

bool roam_policy_should_switch(int current_rssi, int candidate_rssi);
