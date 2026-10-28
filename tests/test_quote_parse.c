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

static const char kTencentPufa[] =
    "v_sh600000=\"1~"
    "\xC6\xD6\xB7\xA2\xD2\xF8\xD0\xD0"
    "~600000~9.83~9.82~9.80~47163~27940~19223~"
    "9.82~231~9.81~302~9.80~227~9.79~92~9.78~283~"
    "9.83~43~9.84~759~9.85~914~9.86~2752~9.87~1483~~"
    "20261028093424~0.01~0.10~9.86~9.76~9.83/47163/46329751~47163~4633~0.01~6.39~~"
    "9.86~9.76~1.02~3273.96~3273.96~0.43~10.80~8.84\";\n";

static const char kTencentIndex[] =
    "v_sh000001=\"1~"
    "\xC9\xCF\xD6\xA4\xD6\xB8\xCA\xFD"
    "~000001~3660.88~3648.30~3676.92~56441204~0~0~"
    "0.00~0~0.00~0~0.00~0~0.00~0~0.00~0~"
    "0.00~0~0.00~0~0.00~0~0.00~0~0.00~0~~"
    "20261028093709~12.58~0.34~3676.92~3660.85~3660.88/56441204/95257467113~"
    "56441204~9525747~0.12~15.92~~3676.92~3660.85~0.44~573523.83~641444.37\";\n";

static void test_tencent_detail(void)
{
    quote_detail_t detail;
    memset(&detail, 0, sizeof(detail));
    assert(quote_parse_tencent_detail(kTencentPufa, sizeof(kTencentPufa) - 1, &detail));
    assert(detail.valid);
    assert(fabs(detail.price - 9.83) < 0.0001);
    assert(fabs(detail.prev_close - 9.82) < 0.0001);
    assert(fabs(detail.open - 9.80) < 0.0001);
    assert(fabs(detail.high - 9.86) < 0.0001);
    assert(fabs(detail.low - 9.76) < 0.0001);
    assert(fabs(detail.volume_hand - 47163) < 0.001);
    assert(fabs(detail.amount_wan - 4633) < 0.001);
    assert(detail.has_market_cap);
    assert(fabs(detail.market_cap_yi - 3273.96) < 0.001);
    assert(detail.has_pe);
    assert(fabs(detail.pe - 6.39) < 0.001);

    assert(fabs(detail.bid[0].price - 9.82) < 0.0001);
    assert(fabs(detail.bid[0].volume - 231) < 0.001);
    assert(fabs(detail.bid[4].price - 9.78) < 0.0001);
    assert(fabs(detail.ask[0].price - 9.83) < 0.0001);
    assert(fabs(detail.ask[0].volume - 43) < 0.001);
    assert(fabs(detail.ask[4].price - 9.87) < 0.0001);
    assert(fabs(detail.ask[4].volume - 1483) < 0.001);

    // 保留分时:解析报价不会清掉调用方已有的分钟数据。
    detail.minute[0] = 9.8f;
    detail.minute_count = 1;
    assert(quote_parse_tencent_detail(kTencentPufa, sizeof(kTencentPufa) - 1, &detail));
    assert(fabs(detail.minute[0] - 9.8f) < 0.0001);
    assert(detail.minute_count == 1);
}

static void test_tencent_detail_index(void)
{
    quote_detail_t detail;
    memset(&detail, 0, sizeof(detail));
    assert(quote_parse_tencent_detail(kTencentIndex, sizeof(kTencentIndex) - 1, &detail));
    assert(detail.valid);
    assert(fabs(detail.price - 3660.88) < 0.001);
    assert(fabs(detail.open - 3676.92) < 0.001);
    assert(fabs(detail.high - 3676.92) < 0.001);
    assert(fabs(detail.low - 3660.85) < 0.001);
    assert(fabs(detail.market_cap_yi - 641444.37) < 0.01);
    assert(detail.has_pe);
    // 指数没有挂单:五档全是 0,界面显示为 "--"。
    for (int i = 0; i < QUOTE_DETAIL_LEVELS; i++) {
        assert(detail.ask[i].price == 0);
        assert(detail.bid[i].price == 0);
    }
}

static void test_tencent_detail_bad(void)
{
    quote_detail_t detail;
    memset(&detail, 0, sizeof(detail));
    const char none[] = "v_pv_none_match=\"1\";\n";
    assert(!quote_parse_tencent_detail(none, sizeof(none) - 1, &detail));
    assert(!quote_parse_tencent_detail("garbage", 7, &detail));
    assert(!quote_parse_tencent_detail(NULL, 0, &detail));
    // 停牌/无成交:现价为 0,视为无效。
    const char halted[] =
        "v_sh600000=\"1~X~600000~0.00~9.82~0.00~0~0~0~0.00~0~0.00~0~0.00~0~0.00~0~"
        "0.00~0~0.00~0~0.00~0~0.00~0~0.00~0~0.00~0~~20261028093424~0~0~0~0~0/0/0~"
        "0~0~0~0~~0~0~0~0~0~0~0~0\";\n";
    assert(!quote_parse_tencent_detail(halted, sizeof(halted) - 1, &detail));
}

static void test_minute_list(void)
{
    const char body[] =
        "var minute_data_list = [['09:30:00', '9.8', '415500'],"
        "['09:31:00', '9.83', '1870986'],['09:32:00', '9.81', '919300']];\n";
    float prices[8];
    uint16_t slots[8];
    assert(quote_parse_minute_list(body, sizeof(body) - 1, prices, slots, 8) == 3);
    assert(fabs(prices[0] - 9.8f) < 0.0001);
    assert(fabs(prices[1] - 9.83f) < 0.0001);
    assert(fabs(prices[2] - 9.81f) < 0.0001);
    assert(slots[0] == 0 && slots[1] == 1 && slots[2] == 2);
    // 容量不足时只取前 n 条。
    assert(quote_parse_minute_list(body, sizeof(body) - 1, prices, slots, 2) == 2);
    assert(fabs(prices[1] - 9.83f) < 0.0001);
    assert(slots[1] == 1);

    // 上午收在 120,下午从 121 起,15:00 是最后一格(241)。
    const char day[] =
        "var minute_data_list = [['09:30:00', '9.8', '1'],['11:30:00', '9.9', '1'],"
        "['13:00:00', '9.95', '1'],['15:00:00', '10.1', '1']];\n";
    assert(quote_parse_minute_list(day, sizeof(day) - 1, prices, slots, 8) == 4);
    assert(slots[0] == 0 && slots[1] == 120 && slots[2] == 121 && slots[3] == 241);

    // 午休、开盘前与乱序时间都不进入曲线。
    const char odd[] =
        "var minute_data_list = [['09:25:00', '9.7', '1'],['12:10:00', '9.9', '1'],"
        "['13:05:00', '10.0', '1'],['09:40:00', '9.8', '1']];\n";
    assert(quote_parse_minute_list(odd, sizeof(odd) - 1, prices, slots, 8) == 1);
    assert(slots[0] == 121 + 5);

    const char empty[] = "var minute_data_list = [];\n";
    assert(quote_parse_minute_list(empty, sizeof(empty) - 1, prices, slots, 8) == 0);
    assert(quote_parse_minute_list("garbage", 7, prices, slots, 8) == 0);
    assert(quote_parse_minute_list(NULL, 0, prices, slots, 8) == 0);
    assert(quote_parse_minute_list(body, sizeof(body) - 1, NULL, slots, 8) == 0);
    assert(quote_parse_minute_list(body, sizeof(body) - 1, prices, NULL, 8) == 0);
    // 截断在第三条中间:前两条仍然可用。
    const char cut[] = "var minute_data_list = [['09:30:00', '9.8', '415500'],"
                       "['09:31:00', '9.8";
    assert(quote_parse_minute_list(cut, sizeof(cut) - 1, prices, slots, 8) == 1);
}

int main(void)
{
    test_single();
    test_multiple_and_find();
    test_index_layout();
    test_empty_and_bad_payload();
    test_unsupported_glyph();
    test_truncated();
    test_tencent_detail();
    test_tencent_detail_index();
    test_tencent_detail_bad();
    test_minute_list();
    puts("quote parse tests: PASS");
    return 0;
}
