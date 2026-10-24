#include "quote_format.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_decimals(void)
{
    assert(quote_price_decimals("sh600519") == 2);
    assert(quote_price_decimals("sz000001") == 2);
    assert(quote_price_decimals("sh000001") == 2);
    assert(quote_price_decimals("sh510300") == 3);
    assert(quote_price_decimals("sz159915") == 3);
    assert(quote_price_decimals(NULL) == 2);
}

static void test_price(void)
{
    char text[24];
    quote_format_price(text, sizeof(text), "sh600519", 1335.0);
    assert(strcmp(text, "1335.00") == 0);
    quote_format_price(text, sizeof(text), "sh510300", 4.178);
    assert(strcmp(text, "4.178") == 0);
    quote_format_price(text, sizeof(text), "sh600519", 0);
    assert(strcmp(text, "--") == 0);
    quote_format_price(text, sizeof(text), "sh600519", NAN);
    assert(strcmp(text, "--") == 0);
}

static void test_signed(void)
{
    char text[24];
    quote_format_signed(text, sizeof(text), 16.51, 2);
    assert(strcmp(text, "+16.51") == 0);
    quote_format_signed(text, sizeof(text), -1.2, 3);
    assert(strcmp(text, "-1.200") == 0);
    quote_format_signed(text, sizeof(text), 0.0, 2);
    assert(strcmp(text, "0.00") == 0);
    quote_format_signed(text, sizeof(text), 0.004, 2);
    assert(strcmp(text, "0.00") == 0);

    quote_format_percent(text, sizeof(text), 1.25219);
    assert(strcmp(text, "+1.25%") == 0);
    quote_format_percent(text, sizeof(text), -0.444);
    assert(strcmp(text, "-0.44%") == 0);
    quote_format_percent(text, sizeof(text), 0.001);
    assert(strcmp(text, "0.00%") == 0);
}

static void test_trend(void)
{
    assert(quote_trend(0.01) == QUOTE_TREND_UP);
    assert(quote_trend(-0.01) == QUOTE_TREND_DOWN);
    assert(quote_trend(0.0) == QUOTE_TREND_FLAT);
    assert(strcmp(quote_trend_text(QUOTE_TREND_UP), "涨") == 0);
    assert(strcmp(quote_trend_text(QUOTE_TREND_DOWN), "跌") == 0);
    assert(strcmp(quote_trend_text(QUOTE_TREND_FLAT), "平") == 0);
}

int main(void)
{
    test_decimals();
    test_price();
    test_signed();
    test_trend();
    puts("quote format tests: PASS");
    return 0;
}
