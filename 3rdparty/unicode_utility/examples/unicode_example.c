/*
 * Leid 2023.8
 */
#include "unicode_utility.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static void xdump(const void * data, size_t length);

int main() {
    char utf8_str[64];
    char gb2312_str[64];
    int len;

    printf("Test begin.\n");
    strcpy(utf8_str, "\xE6\x88\x91\xE4\xBB\xAC\x31\x32\x33\xE5\xA5\xB9\xE4\xBB\xAC\x34\x35\x36\xFF\xFF\x20\x37\x38\x39"); // 我们123她们456\xFF\xFF 789
    xdump(utf8_str, strlen(utf8_str) + 1);

    printf("Test 1:\n");
    memset(gb2312_str, 0, sizeof(gb2312_str));

    len = UTF8ToGB2312Str(utf8_str, gb2312_str, sizeof(gb2312_str));
    if (len > 0) {
        printf("GB2312: %s\n", gb2312_str);
        xdump(gb2312_str, len + 1);
    } else {
       printf("UTF8ToGB2312Str: ERROR\n"); 
    }

    printf("Test 2:\n");
    memset(utf8_str, 0, sizeof(utf8_str));

    len = GB2312ToUTF8Str(gb2312_str, utf8_str, sizeof(utf8_str));
    if (len > 0) {
        printf("UTF8: %s\n", utf8_str);
        xdump(utf8_str, len + 1);
    } else {
       printf("GB2312ToUTF8Str: ERROR\n");
    }

    printf("Test end.\n");
    return 0;
}

static void xdump(const void * data, size_t length) {
    const unsigned char * const cp = (const unsigned char*)data;
    int i;

    for (i = 0; i < length; i++) {
        printf("%02x ", cp[i]);
    }
    printf("\n");
}
