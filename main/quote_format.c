// main/quote_format.c —— 价格、涨跌额、涨跌幅的格式化与颜色分类。
#include "quote_format.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

int quote_price_decimals(const char *code)
{
    if (!code || strlen(code) != 8) return 2;
    const char exchange1 = code[1];
    const char board = code[2];
    if (code[0] == 's' && exchange1 == 'h' && board == '5') return 3;  // 沪市 ETF
    if (code[0] == 's' && exchange1 == 'z' && board == '1') return 3;  // 深市 ETF/LOF
    return 2;
}

void quote_format_price(char *out, size_t size, const char *code, double price)
{
    if (!out || size == 0) return;
    if (!isfinite(price) || price <= 0) {
        snprintf(out, size, "--");
        return;
    }
    snprintf(out, size, "%.*f", quote_price_decimals(code), price);
}

void quote_format_signed(char *out, size_t size, double value, int decimals)
{
    if (!out || size == 0) return;
    if (decimals < 0) decimals = 0;
    if (decimals > 4) decimals = 4;

    double zero_limit = 0.5;
    for (int i = 0; i < decimals; i++) zero_limit /= 10.0;
    if (!isfinite(value) || fabs(value) < zero_limit) {
        snprintf(out, size, "%.*f", decimals, 0.0);
        return;
    }
    char format[8];
    snprintf(format, sizeof(format), "%%+.%df", decimals);
    snprintf(out, size, format, value);
}

void quote_format_percent(char *out, size_t size, double pct)
{
    if (!out || size == 0) return;
    if (!isfinite(pct) || fabs(pct) < 0.005) {
        snprintf(out, size, "0.00%%");
        return;
    }
    snprintf(out, size, "%+.2f%%", pct);
}

quote_trend_t quote_trend(double change)
{
    if (!isfinite(change)) return QUOTE_TREND_FLAT;
    if (change > 0.0005) return QUOTE_TREND_UP;
    if (change < -0.0005) return QUOTE_TREND_DOWN;
    return QUOTE_TREND_FLAT;
}

const char *quote_trend_text(quote_trend_t trend)
{
    switch (trend) {
    case QUOTE_TREND_UP: return "涨";
    case QUOTE_TREND_DOWN: return "跌";
    default: return "平";
    }
}
