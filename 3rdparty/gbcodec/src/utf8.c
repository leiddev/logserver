/*
 * gbcodec - UTF-8 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#include <gbcodec/utf8.h>

gbc_status gbc_utf8_decode(const uint8_t *src, size_t src_length,
                           uint32_t *out_code_point, size_t *out_consumed)
{
    uint32_t lead;
    uint32_t code_point;
    size_t needed;
    size_t index;

    if (src == NULL || out_code_point == NULL || out_consumed == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    if (src_length == 0u) {
        return GBC_E_TRUNCATED;
    }

    lead = (uint32_t)src[0];
    if (lead < 0x80u) {
        *out_code_point = lead;
        *out_consumed = 1u;
        return GBC_OK;
    }

    if (lead >= 0xC2u && lead <= 0xDFu) {
        needed = 2u;
        code_point = lead & 0x1Fu;
    } else if (lead >= 0xE0u && lead <= 0xEFu) {
        needed = 3u;
        code_point = lead & 0x0Fu;
    } else if (lead >= 0xF0u && lead <= 0xF4u) {
        needed = 4u;
        code_point = lead & 0x07u;
    } else {
        /* Continuation byte, overlong lead (0xC0/0xC1) or invalid lead. */
        return GBC_E_INVALID_SEQUENCE;
    }

    if (src_length < needed) {
        return GBC_E_TRUNCATED;
    }

    for (index = 1u; index < needed; ++index) {
        const uint32_t byte = (uint32_t)src[index];

        if ((byte & 0xC0u) != 0x80u) {
            return GBC_E_INVALID_SEQUENCE;
        }
        code_point = (code_point << 6) | (byte & 0x3Fu);
    }

    /* Reject overlong forms, surrogates and values above U+10FFFF. */
    if ((needed == 3u && code_point < 0x800u) || (needed == 4u && code_point < 0x10000u) ||
        (code_point >= 0xD800u && code_point <= 0xDFFFu) || code_point > 0x10FFFFu) {
        return GBC_E_INVALID_SEQUENCE;
    }

    *out_code_point = code_point;
    *out_consumed = needed;
    return GBC_OK;
}

size_t gbc_utf8_encode(uint32_t code_point, uint8_t out[4])
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

    if (code_point < 0x80u) {
        out[0] = (uint8_t)code_point;
        return 1u;
    }
    if (code_point < 0x800u) {
        out[0] = (uint8_t)(0xC0u | (code_point >> 6));
        out[1] = (uint8_t)(0x80u | (code_point & 0x3Fu));
        return 2u;
    }
    if (code_point < 0x10000u) {
        out[0] = (uint8_t)(0xE0u | (code_point >> 12));
        out[1] = (uint8_t)(0x80u | ((code_point >> 6) & 0x3Fu));
        out[2] = (uint8_t)(0x80u | (code_point & 0x3Fu));
        return 3u;
    }

    out[0] = (uint8_t)(0xF0u | (code_point >> 18));
    out[1] = (uint8_t)(0x80u | ((code_point >> 12) & 0x3Fu));
    out[2] = (uint8_t)(0x80u | ((code_point >> 6) & 0x3Fu));
    out[3] = (uint8_t)(0x80u | (code_point & 0x3Fu));
    return 4u;
}
