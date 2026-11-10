/*
 * gbcodec - GB2312 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#include <gbcodec/gb2312.h>

#include "detail/gb2312_table.h"

/*
 * O(log n) lookup by code point: binary search the table sorted by code
 * point. Returns 0 when the code point has no GB2312 mapping.
 */
static uint16_t gbc_lookup_by_code_point(uint16_t code_point)
{
    size_t low = 0u;
    size_t high = GBC_TABLE_ENTRIES;

    while (low < high) {
        const size_t middle = low + (high - low) / 2u;
        const uint16_t candidate = gbc_detail_code_point_order[middle].code_point;

        if (candidate < code_point) {
            low = middle + 1u;
        } else {
            high = middle;
        }
    }

    if (low < GBC_TABLE_ENTRIES && gbc_detail_code_point_order[low].code_point == code_point) {
        return gbc_detail_code_point_order[low].gb2312;
    }
    return 0u;
}

/*
 * O(1) lookup by GB2312 code: index the flat table directly. The caller must
 * have validated that both bytes are inside the GB2312 ranges. Returns 0 when
 * the code is not assigned.
 */
static uint16_t gbc_lookup_by_gb2312(uint8_t lead, uint8_t trail)
{
    const size_t row = (size_t)((uint32_t)lead - GBC_GB2312_FIRST_LEAD);
    const size_t column = (size_t)((uint32_t)trail - GBC_GB2312_FIRST_TRAIL);

    return gbc_detail_gb2312_index[row * GBC_GB2312_TRAILS_PER_LEAD + column];
}

gbc_status gbc_gb2312_decode(const uint8_t *src, size_t src_length,
                             uint32_t *out_code_point, size_t *out_consumed)
{
    uint32_t first;
    uint16_t mapped;

    if (src == NULL || out_code_point == NULL || out_consumed == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    if (src_length == 0u) {
        return GBC_E_TRUNCATED;
    }

    first = (uint32_t)src[0];
    if (first < 0x80u) {
        *out_code_point = first;
        *out_consumed = 1u;
        return GBC_OK;
    }
    if (first < GBC_GB2312_FIRST_LEAD || first > GBC_GB2312_LAST_LEAD) {
        return GBC_E_INVALID_SEQUENCE;
    }
    if (src_length < 2u) {
        return GBC_E_TRUNCATED;
    }
    if ((uint32_t)src[1] < GBC_GB2312_FIRST_TRAIL || (uint32_t)src[1] > GBC_GB2312_LAST_TRAIL) {
        return GBC_E_INVALID_SEQUENCE;
    }

    mapped = gbc_lookup_by_gb2312(src[0], src[1]);
    if (mapped == 0u) {
        return GBC_E_UNMAPPABLE;
    }

    *out_code_point = (uint32_t)mapped;
    *out_consumed = 2u;
    return GBC_OK;
}

size_t gbc_gb2312_encode(uint32_t code_point, uint8_t out[2])
{
    uint16_t mapped;

    if (out == NULL) {
        return 0u;
    }
    if (code_point < 0x80u) {
        out[0] = (uint8_t)code_point;
        return 1u;
    }
    if (code_point > 0xFFFFu) {
        /* GB2312 only covers the basic multilingual plane. */
        return 0u;
    }

    mapped = gbc_lookup_by_code_point((uint16_t)code_point);
    if (mapped == 0u) {
        return 0u;
    }

    out[0] = (uint8_t)(mapped >> 8);
    out[1] = (uint8_t)(mapped & 0xFFu);
    return 2u;
}
