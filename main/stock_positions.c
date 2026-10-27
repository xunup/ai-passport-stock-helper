// main/stock_positions.c —— 持仓盈亏计算与 NVS 文本格式。
#include "stock_positions.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool stock_position_is_set(double cost, uint32_t lots)
{
    return isfinite(cost) && cost > 0 && lots > 0;
}

double stock_position_profit(double price, double cost, uint32_t lots)
{
    if (!stock_position_is_set(cost, lots) || !isfinite(price) || price <= 0) return NAN;
    return (price - cost) * (double)lots * (double)STOCK_POSITION_LOT_SHARES;
}

double stock_position_today_profit(double change, uint32_t lots)
{
    if (lots == 0 || !isfinite(change)) return NAN;
    return change * (double)lots * (double)STOCK_POSITION_LOT_SHARES;
}

void stock_position_format_amount(char *out, size_t size, double amount)
{
    if (!out || size == 0) return;
    if (!isfinite(amount)) {
        snprintf(out, size, "--");
        return;
    }
    const double magnitude = fabs(amount);
    if (magnitude < 0.005) {
        snprintf(out, size, "0.00");
    } else if (magnitude >= 100000000.0) {
        snprintf(out, size, "%+.2f亿", amount / 100000000.0);
    } else if (magnitude >= 10000.0) {
        snprintf(out, size, "%+.2f万", amount / 10000.0);
    } else {
        snprintf(out, size, "%+.2f", amount);
    }
}

int stock_positions_serialize(char *out, size_t out_size,
                              const char codes[][STOCK_CODE_LEN],
                              const double costs[], const uint32_t lots[], int count)
{
    if (!out || out_size == 0 || !codes || !costs || !lots) return -1;
    if (count < 0 || count > STOCK_CODES_MAX) return -1;

    size_t used = 0;
    out[0] = '\0';
    for (int i = 0; i < count; i++) {
        const double cost = isfinite(costs[i]) && costs[i] > 0 ? costs[i] : 0.0;
        if (cost <= 0 && lots[i] == 0) continue;
        const int written = snprintf(out + used, out_size - used, "%s%s:%.4f:%u",
                                     used ? "," : "", codes[i], cost,
                                     (unsigned)lots[i]);
        if (written < 0 || (size_t)written >= out_size - used) {
            out[0] = '\0';
            return -1;
        }
        used += (size_t)written;
    }
    return (int)used;
}

static char *trim(char *text)
{
    while (*text && isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) end--;
    *end = '\0';
    return text;
}

int stock_positions_parse(const char *text,
                          const char codes[][STOCK_CODE_LEN], int code_count,
                          double costs[], uint32_t lots[])
{
    if (!costs || !lots || !codes) return 0;
    for (int i = 0; i < code_count; i++) {
        costs[i] = 0;
        lots[i] = 0;
    }
    if (!text || code_count <= 0) return 0;

    int applied = 0;
    const char *cursor = text;
    while (*cursor) {
        while (*cursor == ',' || *cursor == ';' || isspace((unsigned char)*cursor)) cursor++;
        if (!*cursor) break;

        const char *start = cursor;
        while (*cursor && *cursor != ',' && *cursor != ';') cursor++;
        const size_t len = (size_t)(cursor - start);

        char entry[64];
        if (len < sizeof(entry)) {
            memcpy(entry, start, len);
            entry[len] = '\0';
            char *first = strchr(entry, ':');
            char *second = first ? strchr(first + 1, ':') : NULL;
            if (first && second) {
                *first = '\0';
                *second = '\0';
                char *code = trim(entry);
                char *cost_text = trim(first + 1);
                char *lots_text = trim(second + 1);

                char *end = NULL;
                const double cost = strtod(cost_text, &end);
                const bool cost_ok = end != cost_text && *end == '\0' && isfinite(cost) &&
                                     cost >= 0 && cost <= STOCK_POSITION_COST_MAX;
                const unsigned long parsed_lots = strtoul(lots_text, &end, 10);
                const bool lots_ok = end != lots_text && *end == '\0' &&
                                     parsed_lots <= STOCK_POSITION_LOTS_MAX;
                if (cost_ok && lots_ok) {
                    for (int i = 0; i < code_count; i++) {
                        if (strcmp(codes[i], code) != 0) continue;
                        costs[i] = cost;
                        lots[i] = (uint32_t)parsed_lots;
                        applied++;
                        break;
                    }
                }
            }
        }
        if (*cursor == ',' || *cursor == ';') cursor++;
    }
    return applied;
}
