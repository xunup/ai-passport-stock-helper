// main/quote_parse.h —— 新浪行情响应解析(纯逻辑,可在主机上测试)。
#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "stock_codes.h"

#define QUOTE_NAME_MAX 40

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
