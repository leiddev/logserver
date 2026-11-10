/*
 * gbcodec - UTF-16 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_UTF16_H
#define GBCODEC_UTF16_H

#include <gbcodec/export.h>
#include <gbcodec/status.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decodes one UTF-16 code unit sequence from src.
 *
 * A high surrogate must be followed by a low surrogate. On success
 * *out_code_point receives the code point and *out_consumed the number of
 * code units read (1 or 2). On failure both output parameters are left
 * untouched.
 */
GBC_API gbc_status gbc_utf16_decode(const uint16_t *src, size_t src_length,
                                    uint32_t *out_code_point, size_t *out_consumed);

/*
 * Encodes one code point as UTF-16 into out, which must have room for two
 * code units. Returns 1 or 2, or 0 when the code point is a surrogate or
 * greater than U+10FFFF.
 */
GBC_API size_t gbc_utf16_encode(uint32_t code_point, uint16_t out[2]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GBCODEC_UTF16_H */
