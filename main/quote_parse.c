// main/quote_parse.c —— 新浪财经 hq.sinajs.cn 响应的 GBK 解码与字段解析。
#include "quote_parse.h"

#include "gbk_table.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static const char *mem_find(const char *hay, size_t hay_len,
                            const char *needle, size_t needle_len)
{
    if (needle_len == 0 || needle_len > hay_len) return NULL;
    for (size_t i = 0; i + needle_len <= hay_len; i++) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, needle_len) == 0) {
            return hay + i;
        }
    }
    return NULL;
}

static size_t utf8_encode(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

// 字体只带 GB2312 一级汉字与少量标点。落在字体之外的字符显示成方框,
// 因此在这里就替换掉,保证屏幕上的名字每一格都有字形。
static bool codepoint_in_font(uint32_t cp, uint8_t gbk_hi)
{
    if (gbk_hi >= 0xB0 && gbk_hi <= 0xD7) return true;  // GB2312 一级汉字
    switch (cp) {
    case 0x00B7:  // ·
    case 0x2014:  // —
    case 0x2018:
    case 0x2019:
    case 0x201C:
    case 0x201D:
    case 0x2026:  // …
    case 0x2191:  // ↑
    case 0x2193:  // ↓
    case 0x3001:  // 、
    case 0x3002:  // 。
    case 0x300A:
    case 0x300B:
    case 0x3010:
    case 0x3011:
    case 0xFF01:
    case 0xFF08:
    case 0xFF09:
    case 0xFF0C:
    case 0xFF1A:
    case 0xFF1B:
    case 0xFF1F:
    case 0xFFE5:  // ￥
        return true;
    default:
        return false;
    }
}

static void name_to_utf8(const char *gbk, size_t len, char *out, size_t out_size)
{
    size_t used = 0;
    size_t i = 0;
    while (i < len) {
        const uint8_t byte = (uint8_t)gbk[i];
        char encoded[4];
        size_t encoded_len;
        size_t consumed = 1;

        if (byte < 0x80) {
            if (byte < 0x20 || byte == 0x7F) {
                encoded_len = 1;
                encoded[0] = '?';
            } else {
                encoded_len = 1;
                encoded[0] = (char)byte;
            }
        } else if (byte >= 0x81 && byte <= 0xFE && i + 1 < len) {
            const uint8_t lo = (uint8_t)gbk[i + 1];
            uint16_t cp = gbk_gb2312_to_unicode(byte, lo);
            consumed = 2;
            if (cp != 0 && codepoint_in_font(cp, byte)) {
                encoded_len = utf8_encode(cp, encoded);
            } else {
                encoded_len = 1;
                encoded[0] = '?';
            }
        } else {
            encoded_len = 1;
            encoded[0] = '?';
        }

        if (used + encoded_len >= out_size) break;
        memcpy(out + used, encoded, encoded_len);
        used += encoded_len;
        i += consumed;
    }
    out[used] = '\0';
}

static bool field_double(const char *field, double *value)
{
    char text[32];
    const size_t len = strlen(field);
    if (len == 0 || len >= sizeof(text)) return false;
    memcpy(text, field, len + 1);
    char *end = NULL;
    const double parsed = strtod(text, &end);
    if (end == text || !isfinite(parsed)) return false;
    *value = parsed;
    return true;
}

static void parse_payload(const char *code, const char *payload, size_t len, quote_t *out)
{
    memset(out, 0, sizeof(*out));
    memcpy(out->code, code, STOCK_CODE_LEN - 1);

    char text[512];
    if (len >= sizeof(text)) len = sizeof(text) - 1;
    memcpy(text, payload, len);
    text[len] = '\0';

    const char *fields[6];
    size_t field_count = 0;
    char *cursor = text;
    fields[field_count++] = cursor;
    while (field_count < 6) {
        char *comma = strchr(cursor, ',');
        if (!comma) break;
        *comma = '\0';
        cursor = comma + 1;
        fields[field_count++] = cursor;
    }
    if (field_count < 6) return;

    name_to_utf8(fields[0], strlen(fields[0]), out->name, sizeof(out->name));
    if (out->name[0] == '\0') return;

    double open = 0;
    if (!field_double(fields[1], &open) ||
        !field_double(fields[2], &out->prev_close) ||
        !field_double(fields[3], &out->price)) {
        return;
    }
    if (out->prev_close <= 0 || out->price <= 0) return;

    out->change = out->price - out->prev_close;
    out->change_pct = out->change / out->prev_close * 100.0;
    out->valid = true;
}

size_t quote_parse_response(const char *body, size_t len, quote_t *out, size_t capacity)
{
    if (!body || !out || capacity == 0) return 0;
    if (len == 0) len = strlen(body);

    size_t count = 0;
    const char *cursor = body;
    const char *end = body + len;
    while (count < capacity) {
        const char *token = mem_find(cursor, (size_t)(end - cursor), "hq_str_", 7);
        if (!token) break;
        const char *code_start = token + 7;
        const char *equals = memchr(code_start, '=', (size_t)(end - code_start));
        if (!equals) break;
        const char *quote_start = memchr(equals + 1, '"', (size_t)(end - equals - 1));
        if (!quote_start) break;
        const char *quote_end = memchr(quote_start + 1, '"', (size_t)(end - quote_start - 1));
        if (!quote_end) break;

        char code[STOCK_CODE_LEN] = {0};
        const size_t code_len = (size_t)(equals - code_start);
        if (code_len > 0 && code_len < STOCK_CODE_LEN) {
            memcpy(code, code_start, code_len);
        }
        if (strlen(code) == STOCK_CODE_LEN - 1) {
            parse_payload(code, quote_start + 1, (size_t)(quote_end - quote_start - 1), &out[count]);
            count++;
        }
        cursor = quote_end + 1;
    }
    return count;
}

const quote_t *quote_list_find(const quote_t *items, size_t count, const char *code)
{
    if (!items || !code) return NULL;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(items[i].code, code) == 0) return &items[i];
    }
    return NULL;
}
