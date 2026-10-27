// main/stock_positions.h —— 成本价、持仓手数与盈亏金额(纯逻辑,可在主机上测试)。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stock_codes.h"

#define STOCK_POSITION_COST_MAX 1000000.0
#define STOCK_POSITION_LOTS_MAX 100000
#define STOCK_POSITION_LOT_SHARES 100
#define STOCK_POSITIONS_TEXT_MAX 256

typedef struct {
    char code[STOCK_CODE_LEN];
    double cost;
    uint32_t lots;
} stock_position_t;

// 成本价与手数都大于 0 才算已设置,才显示盈亏。
bool stock_position_is_set(double cost, uint32_t lots);

// (现价 - 成本价) × 手数 × 100;未设置或价格无效时返回 NAN。
double stock_position_profit(double price, double cost, uint32_t lots);

// (现价 - 昨收) × 手数 × 100;手数为 0 或涨跌额无效时返回 NAN。
double stock_position_today_profit(double change, uint32_t lots);

// 金额显示:不足一万保留两位小数,较大金额缩成"万"/"亿"。
void stock_position_format_amount(char *out, size_t size, double amount);

// 序列化为 code:cost:lots,...。只写成本或手数非零的条目,返回长度或 -1。
int stock_positions_serialize(char *out, size_t out_size,
                              const char codes[][STOCK_CODE_LEN],
                              const double costs[], const uint32_t lots[], int count);

// 将与 codes 匹配的条目写入 costs/lots(先清零),返回匹配条数。
int stock_positions_parse(const char *text,
                          const char codes[][STOCK_CODE_LEN], int code_count,
                          double costs[], uint32_t lots[]);
