// main/quote_format.h —— 行情数字的显示格式(纯逻辑)。
#pragma once

#include <stddef.h>

typedef enum {
    QUOTE_TREND_FLAT = 0,
    QUOTE_TREND_UP,
    QUOTE_TREND_DOWN,
} quote_trend_t;

int quote_price_decimals(const char *code);
void quote_format_price(char *out, size_t size, const char *code, double price);
void quote_format_signed(char *out, size_t size, double value, int decimals);
void quote_format_percent(char *out, size_t size, double pct);
quote_trend_t quote_trend(double change);
const char *quote_trend_text(quote_trend_t trend);
