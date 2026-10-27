// main/refresh_options.h —— 行情刷新档位:0 表示手动(按 OK 刷新),其余为秒数。
#pragma once

#include <stdint.h>

#define REFRESH_INTERVAL_MANUAL 0
#define REFRESH_INTERVAL_PRESET_COUNT 10
extern const uint16_t kRefreshIntervalPresets[REFRESH_INTERVAL_PRESET_COUNT];

// 把任意秒数收敛到最近档位;0 保持手动模式。
uint16_t refresh_interval_normalize(uint16_t interval_sec);

// 在档位间上下移动;越界时停在首尾档位。
uint16_t refresh_interval_step(uint16_t interval_sec, int delta);
