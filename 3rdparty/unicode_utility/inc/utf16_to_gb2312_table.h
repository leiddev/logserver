/*
 * Leid 2023.8
 */
#ifndef _UTF16_TO_GB2312_TABLE_H_
#define _UTF16_TO_GB2312_TABLE_H_

#include <stdint.h>

#define MAX_UTF16_SIZE  (6768+693)

extern const uint16_t utf16_to_gb2312_table[MAX_UTF16_SIZE][2];

#endif // !_UTF16_TO_GB2312_TABLE_H_
