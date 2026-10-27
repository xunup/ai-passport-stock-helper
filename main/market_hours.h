// main/market_hours.h —— A 股交易时段判定(纯逻辑,便于主机测试)。
#pragma once

#include <stdbool.h>

// 09:15-11:30、13:00-15:00,按分钟判断并含收盘分钟。
bool market_hours_is_trading(int hour, int minute);

// 距下一交易时段开盘的秒数;交易时段内返回 0。
int market_hours_seconds_until_open(int hour, int minute, int second);
