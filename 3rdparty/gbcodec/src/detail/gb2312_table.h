/*
 * gbcodec - internal GB2312 lookup tables.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 *
 * This header is private to the implementation. It is not installed.
 */
#ifndef GBCODEC_DETAIL_GB2312_TABLE_H
#define GBCODEC_DETAIL_GB2312_TABLE_H

#include <stdint.h>

#define GBC_GB2312_FIRST_LEAD 0xA1u
#define GBC_GB2312_LAST_LEAD 0xF7u
#define GBC_GB2312_FIRST_TRAIL 0xA1u
#define GBC_GB2312_LAST_TRAIL 0xFEu
#define GBC_GB2312_TRAILS_PER_LEAD 94u

#define GBC_GB2312_ROW_COUNT \
    (GBC_GB2312_LAST_LEAD - GBC_GB2312_FIRST_LEAD + 1u)
#define GBC_GB2312_INDEX_SIZE (GBC_GB2312_ROW_COUNT * GBC_GB2312_TRAILS_PER_LEAD)

/* Number of GB2312 code points: 6763 hanzi plus 682 symbols. */
#define GBC_TABLE_ENTRIES 7445u

typedef struct gbc_table_entry {
    uint16_t code_point;
    uint16_t gb2312;
} gbc_table_entry;

/*
 * Indexed by (lead - 0xA1) * 94 + (trail - 0xA1); 0 means "not assigned".
 * U+0000 is never a GB2312 mapping, so 0 is a safe sentinel.
 */
extern const uint16_t gbc_detail_gb2312_index[GBC_GB2312_INDEX_SIZE];

/* Sorted by code point, for binary search. */
extern const gbc_table_entry gbc_detail_code_point_order[GBC_TABLE_ENTRIES];

#endif /* GBCODEC_DETAIL_GB2312_TABLE_H */
