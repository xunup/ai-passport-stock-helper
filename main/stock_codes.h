#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STOCK_CODE_LEN 9
#define STOCK_CODES_MAX 8

typedef enum {
    STOCK_CODES_OK = 0,
    STOCK_CODES_ERR_EMPTY,
    STOCK_CODES_ERR_TOO_MANY,
    STOCK_CODES_ERR_INVALID,
} stock_codes_status_t;

// Normalizes a single A-share/index/ETF code. Accepted forms:
//   600519, sh600519, sh000001, 600519.SH, 000001.SZ, 430047.BJ
// Bare codes infer the exchange: 6/5 -> sh, 0/1/3 -> sz, 4/8 -> bj.
bool stock_code_normalize(const char *input, char out[STOCK_CODE_LEN]);

// Parses a comma/semicolon/whitespace separated list. Duplicates are removed.
// `bad_index` is the zero-based token index of an invalid code, or -1.
stock_codes_status_t stock_codes_parse(const char *text,
                                       char out[][STOCK_CODE_LEN],
                                       int max_codes,
                                       int *count,
                                       int *bad_index);

// Joins normalized codes with commas. Returns the written length or -1.
int stock_codes_join(char *out, size_t out_size,
                     const char codes[][STOCK_CODE_LEN], int count);

const char *stock_codes_status_text(stock_codes_status_t status);
