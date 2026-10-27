#include "refresh_options.h"

#include <assert.h>
#include <stdio.h>

static void test_normalize(void)
{
    assert(refresh_interval_normalize(0) == 0);
    assert(refresh_interval_normalize(1) == 1);
    assert(refresh_interval_normalize(2) == 1);
    assert(refresh_interval_normalize(7) == 5);
    assert(refresh_interval_normalize(8) == 10);
    assert(refresh_interval_normalize(45) == 30);
    assert(refresh_interval_normalize(90) == 60);
    assert(refresh_interval_normalize(200) == 120);
    assert(refresh_interval_normalize(300) == 300);
    assert(refresh_interval_normalize(600) == 300);
}

static void test_step(void)
{
    assert(refresh_interval_step(0, 1) == 1);
    assert(refresh_interval_step(0, -1) == 0);
    assert(refresh_interval_step(1, -1) == 0);
    assert(refresh_interval_step(1, 1) == 3);
    assert(refresh_interval_step(5, 1) == 10);
    assert(refresh_interval_step(5, -1) == 3);
    assert(refresh_interval_step(300, 1) == 300);
    assert(refresh_interval_step(300, -100) == 0);
    assert(refresh_interval_step(7, 1) == 10);
    assert(refresh_interval_step(7, -1) == 3);
}

int main(void)
{
    test_normalize();
    test_step();
    puts("refresh options tests: PASS");
    return 0;
}
