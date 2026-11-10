/*
 * gbcodec - UTF-16 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#include <gbcodec/utf16.h>

gbc_status gbc_utf16_decode(const uint16_t *src, size_t src_length,
                            uint32_t *out_code_point, size_t *out_consumed)
{
    uint32_t first;

    if (src == NULL || out_code_point == NULL || out_consumed == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    if (src_length == 0u) {
        return GBC_E_TRUNCATED;
    }

    first = (uint32_t)src[0];

    if (first >= 0xD800u && first <= 0xDBFFu) {
        uint32_t second;

        if (src_length < 2u) {
            return GBC_E_TRUNCATED;
        }
        second = (uint32_t)src[1];
        if (second < 0xDC00u || second > 0xDFFFu) {
            return GBC_E_INVALID_SEQUENCE;
        }
        *out_code_point = 0x10000u + ((first - 0xD800u) << 10) + (second - 0xDC00u);
        *out_consumed = 2u;
        return GBC_OK;
    }

    if (first >= 0xDC00u && first <= 0xDFFFu) {
        /* Lone low surrogate. */
        return GBC_E_INVALID_SEQUENCE;
    }

    *out_code_point = first;
    *out_consumed = 1u;
    return GBC_OK;
}

size_t gbc_utf16_encode(uint32_t code_point, uint16_t out[2])
{
    if (out == NULL) {
        return 0u;
    }
    if (code_point > 0x10FFFFu) {
        return 0u;
    }
    if (code_point >= 0xD800u && code_point <= 0xDFFFu) {
        return 0u;
    }

    if (code_point < 0x10000u) {
        out[0] = (uint16_t)code_point;
        return 1u;
    }

    code_point -= 0x10000u;
    out[0] = (uint16_t)(0xD800u | (code_point >> 10));
    out[1] = (uint16_t)(0xDC00u | (code_point & 0x3FFu));
    return 2u;
}
