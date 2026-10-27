#include "stock_positions.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_set_and_profit(void)
{
    assert(!stock_position_is_set(0, 2));
    assert(!stock_position_is_set(10, 0));
    assert(!stock_position_is_set(NAN, 2));
    assert(stock_position_is_set(10.5, 2));

    assert(fabs(stock_position_profit(12.0, 10.0, 2) - 400.0) < 1e-9);
    assert(fabs(stock_position_profit(9.5, 10.0, 3) + 150.0) < 1e-9);
    assert(isnan(stock_position_profit(12.0, 0, 2)));
    assert(isnan(stock_position_profit(0, 10, 2)));

    assert(fabs(stock_position_today_profit(2.5, 3) - 750.0) < 1e-9);
    assert(fabs(stock_position_today_profit(-1.2, 10) + 1200.0) < 1e-9);
    assert(stock_position_today_profit(0, 2) == 0);
    assert(isnan(stock_position_today_profit(2.0, 0)));
    assert(isnan(stock_position_today_profit(NAN, 2)));
    assert(isnan(stock_position_today_profit(INFINITY, 2)));
}

static void test_format_amount(void)
{
    char text[24];
    stock_position_format_amount(text, sizeof(text), 0.001);
    assert(strcmp(text, "0.00") == 0);
    stock_position_format_amount(text, sizeof(text), 1234.5);
    assert(strcmp(text, "+1234.50") == 0);
    stock_position_format_amount(text, sizeof(text), -1234.5);
    assert(strcmp(text, "-1234.50") == 0);
    stock_position_format_amount(text, sizeof(text), 10000.0);
    assert(strcmp(text, "+1.00万") == 0);
    stock_position_format_amount(text, sizeof(text), 12345.67);
    assert(strcmp(text, "+1.23万") == 0);
    stock_position_format_amount(text, sizeof(text), 123456789.0);
    assert(strcmp(text, "+1.23亿") == 0);
    stock_position_format_amount(text, sizeof(text), -123456789.0);
    assert(strcmp(text, "-1.23亿") == 0);
    stock_position_format_amount(text, sizeof(text), NAN);
    assert(strcmp(text, "--") == 0);
}

static void test_serialize_parse(void)
{
    const char codes[2][STOCK_CODE_LEN] = {"sh600519", "sz000001"};
    const double costs[2] = {1311.84, 0};
    const uint32_t lots[2] = {2, 0};
    char text[STOCK_POSITIONS_TEXT_MAX];

    assert(stock_positions_serialize(text, sizeof(text), codes, costs, lots, 2) > 0);
    assert(strcmp(text, "sh600519:1311.8400:2") == 0);

    double parsed_costs[2];
    uint32_t parsed_lots[2];
    assert(stock_positions_parse(text, codes, 2, parsed_costs, parsed_lots) == 1);
    assert(fabs(parsed_costs[0] - 1311.84) < 1e-6);
    assert(parsed_lots[0] == 2);
    assert(parsed_costs[1] == 0 && parsed_lots[1] == 0);

    const double none[2] = {0, 0};
    const uint32_t no_lots[2] = {0, 0};
    assert(stock_positions_serialize(text, sizeof(text), codes, none, no_lots, 2) == 0);
    assert(text[0] == '\0');

    const double cost_only[2] = {10.0, 0};
    assert(stock_positions_serialize(text, sizeof(text), codes, cost_only, no_lots, 2) > 0);
    assert(strcmp(text, "sh600519:10.0000:0") == 0);
}

static void test_parse_variants(void)
{
    const char codes[3][STOCK_CODE_LEN] = {"sh600519", "sz000001", "sh000001"};
    double costs[3];
    uint32_t lots[3];

    const char text[] =
        " sz000001 : 10.25 : 5 ,bad,sh600519:0:0;sh000001:3000.5:1,sz000001:11:2";
    assert(stock_positions_parse(text, codes, 3, costs, lots) == 4);
    assert(costs[0] == 0 && lots[0] == 0);
    assert(fabs(costs[1] - 11.0) < 1e-9 && lots[1] == 2);
    assert(fabs(costs[2] - 3000.5) < 1e-9 && lots[2] == 1);

    assert(stock_positions_parse(NULL, codes, 3, costs, lots) == 0);
    assert(costs[0] == 0 && lots[2] == 0);

    assert(stock_positions_parse("bad:1:2,sh600519:1:2:3", codes, 3, costs, lots) == 0);

    char small[8];
    const double some_costs[1] = {1};
    const uint32_t some_lots[1] = {1};
    assert(stock_positions_serialize(small, sizeof(small), codes, some_costs, some_lots, 1) == -1);
}

int main(void)
{
    test_set_and_profit();
    test_format_amount();
    test_serialize_parse();
    test_parse_variants();
    puts("stock positions tests: PASS");
    return 0;
}
