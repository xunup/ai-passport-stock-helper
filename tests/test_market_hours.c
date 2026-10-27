#include "market_hours.h"

#include <assert.h>
#include <stdio.h>

static void test_is_trading(void)
{
    assert(!market_hours_is_trading(9, 14));
    assert(market_hours_is_trading(9, 15));
    assert(market_hours_is_trading(10, 0));
    assert(market_hours_is_trading(11, 29));
    assert(market_hours_is_trading(11, 30));
    assert(!market_hours_is_trading(11, 31));
    assert(!market_hours_is_trading(12, 59));
    assert(market_hours_is_trading(13, 0));
    assert(market_hours_is_trading(14, 59));
    assert(market_hours_is_trading(15, 0));
    assert(!market_hours_is_trading(15, 1));
    assert(!market_hours_is_trading(0, 0));
    assert(!market_hours_is_trading(23, 59));
}

static void test_seconds_until_open(void)
{
    assert(market_hours_seconds_until_open(8, 0, 0) == 4500);
    assert(market_hours_seconds_until_open(9, 14, 59) == 1);
    assert(market_hours_seconds_until_open(9, 15, 0) == 0);
    assert(market_hours_seconds_until_open(10, 30, 30) == 0);
    assert(market_hours_seconds_until_open(11, 30, 59) == 0);
    assert(market_hours_seconds_until_open(11, 31, 0) == 5340);
    assert(market_hours_seconds_until_open(12, 59, 59) == 1);
    assert(market_hours_seconds_until_open(15, 0, 59) == 0);
    assert(market_hours_seconds_until_open(15, 1, 0) == 65640);
    assert(market_hours_seconds_until_open(23, 59, 59) == 33301);
    assert(market_hours_seconds_until_open(0, 0, 0) == 33300);
}

int main(void)
{
    test_is_trading();
    test_seconds_until_open();
    puts("market hours tests: PASS");
    return 0;
}
