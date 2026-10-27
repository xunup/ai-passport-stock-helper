// main/refresh_options.c —— 刷新档位的纯逻辑,不依赖 LVGL/ESP-IDF。
#include "refresh_options.h"

const uint16_t kRefreshIntervalPresets[REFRESH_INTERVAL_PRESET_COUNT] = {
    0, 1, 3, 5, 10, 15, 30, 60, 120, 300,
};

uint16_t refresh_interval_normalize(uint16_t interval_sec)
{
    if (interval_sec == REFRESH_INTERVAL_MANUAL) return REFRESH_INTERVAL_MANUAL;

    uint16_t best = kRefreshIntervalPresets[1];
    int best_diff = 65535;
    for (int i = 1; i < REFRESH_INTERVAL_PRESET_COUNT; i++) {
        const uint16_t preset = kRefreshIntervalPresets[i];
        const int diff = interval_sec > preset ? interval_sec - preset : preset - interval_sec;
        if (diff < best_diff) {
            best = preset;
            best_diff = diff;
        }
    }
    return best;
}

uint16_t refresh_interval_step(uint16_t interval_sec, int delta)
{
    const uint16_t current = refresh_interval_normalize(interval_sec);
    int index = 0;
    for (int i = 0; i < REFRESH_INTERVAL_PRESET_COUNT; i++) {
        if (kRefreshIntervalPresets[i] == current) {
            index = i;
            break;
        }
    }

    index += delta;
    if (index < 0) index = 0;
    if (index >= REFRESH_INTERVAL_PRESET_COUNT) index = REFRESH_INTERVAL_PRESET_COUNT - 1;
    return kRefreshIntervalPresets[index];
}
