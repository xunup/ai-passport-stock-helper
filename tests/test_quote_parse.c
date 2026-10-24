#include "quote_parse.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char kMoutai[] =
    "var hq_str_sh600519=\""
    "\xB9\xF3\xD6\xDD\xC3\xA9\xCC\xA8"
    ",1311.840,1318.490,1335.000,1343.880,1310.000,1333.000,1335.000,5768593,"
    "7677216283.000,600,1333.000,200,1331.000,200,1330.000,300,1329.500,100,"
    "1329.020,11600,1335.000,200,1335.010,200,1335.140,100,1335.190,100,"
    "1335.200,2026-10-23,15:34:59,00,D|900|1201500.00\";\n";

static const char kPingAn[] =
    "var hq_str_sz000001=\""
    "\xC6\xBD\xB0\xB2\xD2\xF8\xD0\xD0"
    ",11.270,11.300,11.200,11.350,11.170,"
    "2026-10-23,15:36:00\";\n";

static void test_single(void)
{
    quote_t quotes[2];
    const size_t count = quote_parse_response(kMoutai, sizeof(kMoutai) - 1, quotes, 2);
    assert(count == 1);
    assert(strcmp(quotes[0].code, "sh600519") == 0);
    assert(strcmp(quotes[0].name, "贵州茅台") == 0);
    assert(quotes[0].valid);
    assert(fabs(quotes[0].prev_close - 1318.49) < 0.0001);
    assert(fabs(quotes[0].price - 1335.0) < 0.0001);
    assert(fabs(quotes[0].change - 16.51) < 0.0001);
    assert(fabs(quotes[0].change_pct - 1.25219) < 0.01);
}

static void test_multiple_and_find(void)
{
    char body[1024];
    snprintf(body, sizeof(body), "%s%s", kMoutai, kPingAn);
    quote_t quotes[4];
    const size_t count = quote_parse_response(body, strlen(body), quotes, 4);
    assert(count == 2);
    assert(strcmp(quotes[1].name, "平安银行") == 0);
    assert(quotes[1].valid);

    const quote_t *found = quote_list_find(quotes, count, "sz000001");
    assert(found == &quotes[1]);
    assert(quote_list_find(quotes, count, "sh000001") == NULL);
}

static void test_index_layout(void)
{
    const char body[] =
        "var hq_str_sh000001=\""
        "\xC9\xCF\xD6\xA4\xD6\xB8\xCA\xFD"
        ",3623.5669,3658.0256,3690.4753,3691.3988,3574.2712,0,0,552842912,"
        "898666874902,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"
        "2026-10-23,15:35:30,00,\";\n";
    quote_t quote;
    assert(quote_parse_response(body, sizeof(body) - 1, &quote, 1) == 1);
    assert(strcmp(quote.name, "上证指数") == 0);
    assert(quote.valid);
    assert(fabs(quote.change - 32.4497) < 0.001);
}

static void test_empty_and_bad_payload(void)
{
    const char body[] =
        "var hq_str_sh999999=\"\";\n"
        "var hq_str_sz000002=\""
        "\xCD\xF2\xBF\xC6\x41"
        ",8.000,8.100,8.000,8.100,8.000,"
        "2026-10-23,15:00:00\";\n"
        "var hq_str_sh600000=\""
        "\xC6\xD6\xB7\xA2\xD2\xF8\xD0\xD0"
        ",0,0,0,0,0,2026-10-23,15:00:00\";\n";
    quote_t quotes[4];
    const size_t count = quote_parse_response(body, sizeof(body) - 1, quotes, 4);
    assert(count == 3);
    assert(!quotes[0].valid);
    assert(strcmp(quotes[0].code, "sh999999") == 0);
    assert(quotes[1].valid);
    assert(!quotes[2].valid);
}

static void test_unsupported_glyph(void)
{
    // D8A1 是 GB2312 二级汉字,字体未收录 → '?';C3A9 是“茅”,保留。
    const char body[] =
        "var hq_str_sz000003=\""
        "\xD8\xA1\xC3\xA9"
        ",1.000,1.000,1.000,1.000,1.000,2026-10-23,15:00:00\";\n";
    quote_t quote;
    assert(quote_parse_response(body, sizeof(body) - 1, &quote, 1) == 1);
    assert(strcmp(quote.name, "?茅") == 0);
}

static void test_truncated(void)
{
    const char body[] = "var hq_str_sh600519=\"no closing quote";
    quote_t quote;
    assert(quote_parse_response(body, sizeof(body) - 1, &quote, 1) == 0);

    const char no_space[] = "garbage";
    assert(quote_parse_response(no_space, sizeof(no_space) - 1, &quote, 1) == 0);
    assert(quote_parse_response(kMoutai, sizeof(kMoutai) - 1, NULL, 1) == 0);
}

int main(void)
{
    test_single();
    test_multiple_and_find();
    test_index_layout();
    test_empty_and_bad_payload();
    test_unsupported_glyph();
    test_truncated();
    puts("quote parse tests: PASS");
    return 0;
}
