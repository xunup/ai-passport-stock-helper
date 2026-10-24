#include "stock_codes.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_normalize(void)
{
    char code[STOCK_CODE_LEN];
    assert(stock_code_normalize("600519", code) && strcmp(code, "sh600519") == 0);
    assert(stock_code_normalize(" 000001 ", code) && strcmp(code, "sz000001") == 0);
    assert(stock_code_normalize("300750", code) && strcmp(code, "sz300750") == 0);
    assert(stock_code_normalize("510300", code) && strcmp(code, "sh510300") == 0);
    assert(stock_code_normalize("159915", code) && strcmp(code, "sz159915") == 0);
    assert(stock_code_normalize("430047", code) && strcmp(code, "bj430047") == 0);
    assert(stock_code_normalize("830799", code) && strcmp(code, "bj830799") == 0);
    assert(stock_code_normalize("SH600519", code) && strcmp(code, "sh600519") == 0);
    assert(stock_code_normalize("sz399001", code) && strcmp(code, "sz399001") == 0);

    assert(!stock_code_normalize("", code));
    assert(!stock_code_normalize("60051", code));
    assert(!stock_code_normalize("6005199", code));
    assert(!stock_code_normalize("60051x", code));
    assert(!stock_code_normalize("900001", code));
    assert(!stock_code_normalize("hk00700", code));
    assert(!stock_code_normalize("sh60051", code));
}

static void test_parse(void)
{
    char codes[STOCK_CODES_MAX][STOCK_CODE_LEN];
    int count = -1;
    int bad = -1;

    assert(stock_codes_parse("600519, 000001，sh000001； 510300、300750",
                             codes, STOCK_CODES_MAX, &count, &bad) == STOCK_CODES_OK);
    assert(count == 5);
    assert(strcmp(codes[0], "sh600519") == 0);
    assert(strcmp(codes[1], "sz000001") == 0);
    assert(strcmp(codes[2], "sh000001") == 0);
    assert(strcmp(codes[3], "sh510300") == 0);
    assert(strcmp(codes[4], "sz300750") == 0);

    assert(stock_codes_parse("600519,600519", codes, STOCK_CODES_MAX, &count, &bad) ==
           STOCK_CODES_OK);
    assert(count == 1);

    bad = -1;
    assert(stock_codes_parse("600519,abc,000001", codes, STOCK_CODES_MAX, &count, &bad) ==
           STOCK_CODES_ERR_INVALID);
    assert(bad == 2 && count == 0);

    assert(stock_codes_parse("  ,  ", codes, STOCK_CODES_MAX, &count, &bad) ==
           STOCK_CODES_ERR_EMPTY);

    assert(stock_codes_parse("600519,000001,300750,510300,159915,430047,830799,872925,600000",
                             codes, STOCK_CODES_MAX, &count, &bad) ==
           STOCK_CODES_ERR_TOO_MANY);
    assert(bad == 9);
}

static void test_join(void)
{
    char codes[3][STOCK_CODE_LEN] = {"sh600519", "sz000001", "sh000001"};
    char text[64];
    assert(stock_codes_join(text, sizeof(text), codes, 3) == 26);
    assert(strcmp(text, "sh600519,sz000001,sh000001") == 0);

    char small[10];
    assert(stock_codes_join(small, sizeof(small), codes, 3) == -1);
    assert(small[0] == '\0');
}

int main(void)
{
    test_normalize();
    test_parse();
    test_join();
    puts("stock code tests: PASS");
    return 0;
}
