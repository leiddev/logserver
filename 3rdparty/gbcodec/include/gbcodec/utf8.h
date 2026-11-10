/*
 * gbcodec - UTF-8 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_UTF8_H
#define GBCODEC_UTF8_H

#include <gbcodec/export.h>
#include <gbcodec/status.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decodes one UTF-8 sequence from src.
 *
 * The input is validated strictly (RFC 3629): overlong forms, surrogate code
 * points, values above U+10FFFF and stray lead or continuation bytes are all
 * rejected. On success *out_code_point receives the code point and
 * *out_consumed the number of bytes read (1 to 4). On failure both output
 * parameters are left untouched.
 */
GBC_API gbc_status gbc_utf8_decode(const uint8_t *src, size_t src_length,
                                   uint32_t *out_code_point, size_t *out_consumed);

/*
 * Encodes one code point as UTF-8 into out, which must have room for four
 * bytes. Returns the number of bytes written (1 to 4), or 0 when the code
 * point is a surrogate or greater than U+10FFFF.
 */
GBC_API size_t gbc_utf8_encode(uint32_t code_point, uint8_t out[4]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GBCODEC_UTF8_H */
