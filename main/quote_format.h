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
// 大数字压缩到万/亿:47163 → "4.72万",56441204 → "5644万"。
void quote_format_compact(char *out, size_t size, double value);
// 总市值(单位:亿元,保留一位小数):3273.96 → "3274.0亿",641444.37 → "64.1万亿"。
void quote_format_market_cap(char *out, size_t size, double yi);
quote_trend_t quote_trend(double change);
const char *quote_trend_text(quote_trend_t trend);
