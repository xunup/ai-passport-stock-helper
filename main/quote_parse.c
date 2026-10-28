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

static bool detail_field(const char *const *fields, size_t count, size_t index, double *value)
{
    if (index >= count || fields[index][0] == '\0') return false;
    return field_double(fields[index], value);
}

static void parse_levels(const char *const *fields, size_t count, size_t price_index,
                         quote_level_t *out)
{
    for (int i = 0; i < QUOTE_DETAIL_LEVELS; i++) {
        const size_t index = price_index + (size_t)i * 2;
        double price = 0;
        double volume = 0;
        // 停牌/指数没有挂单,价格会返回 0.00,按空档处理。
        if (!detail_field(fields, count, index, &price) || price <= 0) continue;
        if (!detail_field(fields, count, index + 1, &volume) || volume < 0) volume = 0;
        out[i].price = price;
        out[i].volume = volume;
    }
}

bool quote_parse_tencent_detail(const char *body, size_t len, quote_detail_t *out)
{
    if (!body || !out) return false;
    if (len == 0) len = strlen(body);

    const char *open = mem_find(body, len, "=\"", 2);
    if (!open) return false;
    const char *start = open + 2;
    const char *end = memchr(start, '"', len - (size_t)(start - body));
    if (!end || end == start) return false;

    size_t payload_len = (size_t)(end - start);
    char text[1024];
    if (payload_len >= sizeof(text)) payload_len = sizeof(text) - 1;
    memcpy(text, start, payload_len);
    text[payload_len] = '\0';

    const char *fields[56];
    size_t count = 0;
    char *cursor = text;
    fields[count++] = cursor;
    while (count < sizeof(fields) / sizeof(fields[0])) {
        char *separator = strchr(cursor, '~');
        if (!separator) break;
        *separator = '\0';
        cursor = separator + 1;
        fields[count++] = cursor;
    }
    // 现价、昨收、成交量、成交额、市盈率、总市值在 45 号字段之前。
    if (count < 46) return false;

    double price = 0;
    double prev_close = 0;
    if (!detail_field(fields, count, 3, &price) || price <= 0 ||
        !detail_field(fields, count, 4, &prev_close) || prev_close <= 0) {
        return false;
    }

    out->price = price;
    out->prev_close = prev_close;
    out->open = 0;
    out->high = 0;
    out->low = 0;
    out->volume_hand = 0;
    out->amount_wan = 0;
    out->market_cap_yi = 0;
    out->pe = 0;
    out->has_market_cap = false;
    out->has_pe = false;

    if (detail_field(fields, count, 5, &out->open) && out->open < 0) out->open = 0;
    if (detail_field(fields, count, 33, &out->high) && out->high < 0) out->high = 0;
    if (detail_field(fields, count, 34, &out->low) && out->low < 0) out->low = 0;

    if (detail_field(fields, count, 6, &out->volume_hand) && out->volume_hand < 0) {
        out->volume_hand = 0;
    }
    if (detail_field(fields, count, 37, &out->amount_wan) && out->amount_wan < 0) {
        out->amount_wan = 0;
    }
    out->has_market_cap = detail_field(fields, count, 45, &out->market_cap_yi) &&
                          out->market_cap_yi > 0;
    if (!out->has_market_cap) out->market_cap_yi = 0;
    out->has_pe = detail_field(fields, count, 39, &out->pe) && out->pe > 0;
    if (!out->has_pe) out->pe = 0;

    memset(out->ask, 0, sizeof(out->ask));
    memset(out->bid, 0, sizeof(out->bid));
    parse_levels(fields, count, 19, out->ask);  // 卖一价起,随后是量
    parse_levels(fields, count, 9, out->bid);   // 买一价起,随后是量

    out->valid = true;
    return true;
}

// 把 "09:30:00" 映射到当日 242 格时间轴:上午 0..120(11:30),下午 121..241(15:00)。
static bool minute_slot_from_time(const char *text, size_t len, uint16_t *slot)
{
    if (len < 5 || text[2] != ':') return false;
    for (int i = 0; i < 5; i++) {
        if (i == 2) continue;
        if (text[i] < '0' || text[i] > '9') return false;
    }
    const int hour = (text[0] - '0') * 10 + (text[1] - '0');
    const int minute = (text[3] - '0') * 10 + (text[4] - '0');
    const int total = hour * 60 + minute;
    if (total < 9 * 60 + 30 || total > 15 * 60) return false;
    int index;
    if (total <= 11 * 60 + 30) {
        index = total - (9 * 60 + 30);
    } else if (total >= 13 * 60) {
        index = 121 + (total - 13 * 60);
    } else {
        return false;  // 午休没有分钟点
    }
    if (index < 0 || index >= QUOTE_MINUTE_MAX) return false;
    *slot = (uint16_t)index;
    return true;
}

size_t quote_parse_minute_list(const char *body, size_t len, float *prices,
                               uint16_t *slots, size_t capacity)
{
    if (!body || !prices || !slots || capacity == 0) return 0;
    if (len == 0) len = strlen(body);

    const char *cursor = body;
    const char *end = body + len;
    size_t count = 0;
    while (count < capacity) {
        const char *record = mem_find(cursor, (size_t)(end - cursor), "['", 2);
        if (!record) break;
        const char *time_start = record + 2;
        const char *time_end = memchr(time_start, '\'', (size_t)(end - time_start));
        if (!time_end) break;
        const char *price_quote = memchr(time_end + 1, '\'', (size_t)(end - time_end - 1));
        if (!price_quote) break;
        const char *price_start = price_quote + 1;
        const char *price_end = memchr(price_start, '\'', (size_t)(end - price_start));
        if (!price_end) break;
        cursor = price_end + 1;

        uint16_t slot = 0;
        if (!minute_slot_from_time(time_start, (size_t)(time_end - time_start), &slot)) continue;
        // 时间必须递增,否则曲线会倒着画。
        if (count > 0 && slot <= slots[count - 1]) continue;

        char token[24];
        const size_t token_len = (size_t)(price_end - price_start);
        if (token_len > 0 && token_len < sizeof(token)) {
            memcpy(token, price_start, token_len);
            token[token_len] = '\0';
            char *parsed_end = NULL;
            const double price = strtod(token, &parsed_end);
            if (parsed_end != token && isfinite(price) && price > 0) {
                prices[count] = (float)price;
                slots[count] = slot;
                count++;
            }
        }
    }
    return count;
}
