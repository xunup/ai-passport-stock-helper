// main/stock_codes.c —— 自选股代码的规范化、列表解析与序列化。
#include "stock_codes.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int lower_ascii(int c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }

static bool all_digits(const char *text, size_t len)
{
    if (len != 6) return false;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)text[i])) return false;
    }
    return true;
}

static const char *exchange_for_digit(char digit)
{
    switch (digit) {
    case '6':
    case '5':
        return "sh";
    case '0':
    case '1':
    case '3':
        return "sz";
    case '4':
    case '8':
        return "bj";
    default:
        return NULL;
    }
}

bool stock_code_normalize(const char *input, char out[STOCK_CODE_LEN])
{
    if (!input || !out) return false;

    while (*input == ' ' || *input == '\t' || *input == '\r' || *input == '\n') input++;
    size_t len = strlen(input);
    while (len > 0) {
        const char tail = input[len - 1];
        if (tail != ' ' && tail != '\t' && tail != '\r' && tail != '\n') break;
        len--;
    }
    if (len == 0 || len >= STOCK_CODE_LEN + 4) return false;

    char text[16];
    if (len >= sizeof(text)) return false;
    for (size_t i = 0; i < len; i++) text[i] = (char)lower_ascii((unsigned char)input[i]);
    text[len] = '\0';

    if (len == 6) {
        if (!all_digits(text, len)) return false;
        const char *exchange = exchange_for_digit(text[0]);
        if (!exchange) return false;
        memcpy(out, exchange, 2);
        memcpy(out + 2, text, 6);
        out[STOCK_CODE_LEN - 1] = '\0';
        return true;
    }

    if (len == 8) {
        const bool prefixed = (text[0] == 's' && (text[1] == 'h' || text[1] == 'z')) ||
                              (text[0] == 'b' && text[1] == 'j');
        if (!prefixed || !all_digits(text + 2, 6)) return false;
        memcpy(out, text, STOCK_CODE_LEN);
        out[STOCK_CODE_LEN - 1] = '\0';
        return true;
    }

    return false;
}

static bool is_separator(char c)
{
    return c == ',' || c == ';' || c == '|' || c == ' ' || c == '\t' ||
           c == '\r' || c == '\n';
}

// 全角逗号/分号/顿号在 UTF-8 中占 3 字节，按字节判断会切坏多字节字符。
static size_t separator_len(const char *p)
{
    if (is_separator(*p)) return 1;
    if (strncmp(p, "\xEF\xBC\x8C", 3) == 0 ||   // ，
        strncmp(p, "\xEF\xBC\x9B", 3) == 0 ||   // ；
        strncmp(p, "\xE3\x80\x81", 3) == 0) {   // 、
        return 3;
    }
    return 0;
}

stock_codes_status_t stock_codes_parse(const char *text,
                                       char out[][STOCK_CODE_LEN],
                                       int max_codes,
                                       int *count,
                                       int *bad_index)
{
    if (count) *count = 0;
    if (bad_index) *bad_index = 0;
    if (!text) return STOCK_CODES_ERR_EMPTY;

    int used = 0;
    int token_index = 0;
    const char *cursor = text;

    for (;;) {
        size_t skip;
        while ((skip = separator_len(cursor)) != 0) cursor += skip;
        if (!*cursor) break;

        const char *start = cursor;
        while (*cursor && separator_len(cursor) == 0) cursor++;
        size_t len = (size_t)(cursor - start);
        token_index++;

        char token[16];
        if (len >= sizeof(token)) {
            if (bad_index) *bad_index = token_index;
            return STOCK_CODES_ERR_INVALID;
        }
        memcpy(token, start, len);
        token[len] = '\0';

        char code[STOCK_CODE_LEN];
        if (!stock_code_normalize(token, code)) {
            if (bad_index) *bad_index = token_index;
            return STOCK_CODES_ERR_INVALID;
        }

        bool duplicate = false;
        for (int i = 0; i < used; i++) {
            if (strcmp(out[i], code) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        if (used >= max_codes) {
            if (bad_index) *bad_index = token_index;
            return STOCK_CODES_ERR_TOO_MANY;
        }
        memcpy(out[used], code, STOCK_CODE_LEN);
        used++;
    }

    if (used == 0) return STOCK_CODES_ERR_EMPTY;
    if (count) *count = used;
    return STOCK_CODES_OK;
}

int stock_codes_join(char *out, size_t out_size,
                     const char codes[][STOCK_CODE_LEN], int count)
{
    if (!out || out_size == 0 || !codes || count < 0) return -1;
    out[0] = '\0';
    size_t used = 0;
    for (int i = 0; i < count; i++) {
        const char *separator = i == 0 ? "" : ",";
        int written = snprintf(out + used, out_size - used, "%s%s", separator, codes[i]);
        if (written < 0 || (size_t)written >= out_size - used) {
            out[0] = '\0';
            return -1;
        }
        used += (size_t)written;
    }
    return (int)used;
}

const char *stock_codes_status_text(stock_codes_status_t status)
{
    switch (status) {
    case STOCK_CODES_OK: return "成功";
    case STOCK_CODES_ERR_EMPTY: return "请至少添加一只股票";
    case STOCK_CODES_ERR_TOO_MANY: return "最多支持 8 只股票";
    case STOCK_CODES_ERR_INVALID: return "股票代码格式不正确，例如 600519 或 sh600519";
    default: return "未知错误";
    }
}
