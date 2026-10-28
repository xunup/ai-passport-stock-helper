// main/quote_parse.h —— 新浪行情响应解析(纯逻辑,可在主机上测试)。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stock_codes.h"

#define QUOTE_NAME_MAX 40
#define QUOTE_DETAIL_LEVELS 5
#define QUOTE_MINUTE_MAX 242  // 09:30-11:30 与 13:00-15:00 共 242 个分钟点

typedef struct {
    char code[STOCK_CODE_LEN];
    char name[QUOTE_NAME_MAX];  // 已转成 UTF-8;字体不支持的字符替换为 '?'
    double price;
    double prev_close;
    double change;
    double change_pct;
    bool valid;
} quote_t;

// 解析形如 var hq_str_sh600519="贵州茅台,1311.840,..."; 的响应。
// 返回写入 out 的条目数(含无效条目);响应里没有的代码不会补。
size_t quote_parse_response(const char *body, size_t len, quote_t *out, size_t capacity);

const quote_t *quote_list_find(const quote_t *items, size_t count, const char *code);

typedef struct {
    double price;
    double volume;  // 手
} quote_level_t;

// 详情页快照。分时价格按时间顺序保存,并记录它在当日 242 格时间轴上的位置。
typedef struct {
    double price;
    double prev_close;
    double open;   // 今开
    double high;   // 最高
    double low;    // 最低
    double volume_hand;
    double amount_wan;
    double market_cap_yi;  // 总市值(亿元),指数为市场总市值
    double pe;             // 市盈率 TTM;<=0(亏损)视为无效
    bool has_market_cap;
    bool has_pe;
    quote_level_t ask[QUOTE_DETAIL_LEVELS];  // 卖一..卖五
    quote_level_t bid[QUOTE_DETAIL_LEVELS];  // 买一..买五
    float minute[QUOTE_MINUTE_MAX];
    uint16_t minute_slot[QUOTE_MINUTE_MAX];  // 0=09:30, 120=11:30, 121=13:00, 241=15:00
    uint16_t minute_count;
    bool valid;
} quote_detail_t;

// 解析 qt.gtimg.cn 的 v_sh600519="1~名称~代码~现价~昨收~..."。
// 只覆盖报价与五档字段,分时数组由调用方保留。名称是 GBK,不在这里解码。
bool quote_parse_tencent_detail(const char *body, size_t len, quote_detail_t *out);

// 解析新浪 vML_DataList.php 的 [['09:30:00','9.80','415500'],...]。
// prices 与 slots 并行写入,返回分钟价个数;slots 见 quote_detail_t.minute_slot。
size_t quote_parse_minute_list(const char *body, size_t len, float *prices,
                               uint16_t *slots, size_t capacity);
