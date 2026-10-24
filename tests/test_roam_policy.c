#include "roam_policy.h"

#include <assert.h>
#include <stdio.h>

static void test_strong_current_stays(void)
{
    assert(!roam_policy_should_switch(-50, -30));
    assert(!roam_policy_should_switch(-70, -50)); // 刚好在门限上,仍视为够用
    assert(!roam_policy_should_switch(-60, -40));
}

static void test_weak_current_switches(void)
{
    assert(roam_policy_should_switch(-78, -60));
    assert(roam_policy_should_switch(-85, -72));
    assert(roam_policy_should_switch(-71, -63));
}

static void test_hysteresis(void)
{
    assert(!roam_policy_should_switch(-80, -73)); // 只强 7 dBm
    assert(!roam_policy_should_switch(-80, -75)); // 目标仍偏弱
    assert(!roam_policy_should_switch(-80, -90)); // 更差
}

int main(void)
{
    test_strong_current_stays();
    test_weak_current_switches();
    test_hysteresis();
    puts("roam policy tests: PASS");
    return 0;
}
