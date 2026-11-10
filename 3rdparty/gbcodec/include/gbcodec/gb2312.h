/*
 * gbcodec - GB2312 codec.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_GB2312_H
#define GBCODEC_GB2312_H

#include <gbcodec/export.h>
#include <gbcodec/status.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decodes one GB2312 character from src.
 *
 * A byte below 0x80 is decoded as the corresponding ASCII code point and
 * consumes one byte. Otherwise the first byte must be a lead byte in the
 * range 0xA1..0xF7 and the second byte a trail byte in 0xA1..0xFE, and the
 * pair must be assigned in GB2312. On success *out_code_point receives the
 * code point and *out_consumed the number of bytes read (1 or 2). On failure
 * both output parameters are left untouched.
 */
GBC_API gbc_status gbc_gb2312_decode(const uint8_t *src, size_t src_length,
                                     uint32_t *out_code_point, size_t *out_consumed);

/*
 * Encodes one code point as GB2312 into out, which must have room for two
 * bytes. The bytes are written in stream order (lead byte first), so the
 * result does not depend on the endianness of the host.
 *
 * Returns 1 or 2, or 0 when the code point is not representable.
 */
GBC_API size_t gbc_gb2312_encode(uint32_t code_point, uint8_t out[2]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GBCODEC_GB2312_H */
