// Generated alongside main/gbk_table.c by tools/gen_gbk_table.py.
#pragma once

#include <stdint.h>

// Returns the BMP code point for a GB2312/GBK byte pair, or 0 if invalid.
uint16_t gbk_gb2312_to_unicode(uint8_t hi, uint8_t lo);
