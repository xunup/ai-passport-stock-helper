// main/roam_policy.c —— 只在当前弱、目标明显更强且目标可用时切换。
#include "roam_policy.h"

bool roam_policy_should_switch(int current_rssi, int candidate_rssi)
{
    if (current_rssi >= ROAM_TRIGGER_RSSI) return false;
    if (candidate_rssi < ROAM_MIN_TARGET_RSSI) return false;
    return candidate_rssi >= current_rssi + ROAM_MIN_IMPROVEMENT_DB;
}
