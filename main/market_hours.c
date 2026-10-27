// main/market_hours.c —— 交易时段纯计算,不依赖 ESP-IDF 与系统时钟。
#include "market_hours.h"

// 收盘分钟整体计入,便于取到 11:30 与 15:00 的收盘价。
#define OPEN_MORNING (9 * 3600 + 15 * 60)
#define CLOSE_MORNING (11 * 3600 + 31 * 60)
#define OPEN_AFTERNOON (13 * 3600)
#define CLOSE_AFTERNOON (15 * 3600 + 60)
#define SECONDS_PER_DAY (24 * 3600)

bool market_hours_is_trading(int hour, int minute)
{
    const int second_of_day = hour * 3600 + minute * 60;
    return (second_of_day >= OPEN_MORNING && second_of_day < CLOSE_MORNING) ||
           (second_of_day >= OPEN_AFTERNOON && second_of_day < CLOSE_AFTERNOON);
}

int market_hours_seconds_until_open(int hour, int minute, int second)
{
    if (market_hours_is_trading(hour, minute)) return 0;

    const int now = hour * 3600 + minute * 60 + second;
    if (now < OPEN_MORNING) return OPEN_MORNING - now;
    if (now < OPEN_AFTERNOON) return OPEN_AFTERNOON - now;
    return SECONDS_PER_DAY - now + OPEN_MORNING;
}
