/*
 * gbcodec - whole-string conversion between UTF-8 and GB2312.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_CONVERT_H
#define GBCODEC_CONVERT_H

#include <gbcodec/export.h>
#include <gbcodec/status.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Conversion between UTF-8 and GB2312 byte strings.
 *
 * Contract for the counted functions below:
 *
 *   - src_length is the exact number of input bytes; the input may contain
 *     embedded NUL bytes and does not need to be NUL-terminated.
 *   - dst_capacity is the total size of dst, in bytes, including the space
 *     reserved for the terminating NUL.
 *   - On success *out_length receives the number of bytes written, excluding
 *     the terminator, and dst is NUL-terminated.
 *   - When dst_capacity is 0, nothing is written to dst and
 *     GBC_E_BUFFER_TOO_SMALL is returned unless the result is empty.
 *   - On any failure dst[0] is set to NUL when dst_capacity is greater than
 *     0 and *out_length (when requested) is left untouched.
 *
 * The _z variants take a NUL-terminated input string and are otherwise
 * identical.
 */

/* Returns the number of bytes the GB2312 result would occupy, excluding NUL. */
GBC_API gbc_status gbc_utf8_to_gb2312_capacity(const uint8_t *src, size_t src_length,
                                               size_t *out_length);

/* Returns the number of bytes the UTF-8 result would occupy, excluding NUL. */
GBC_API gbc_status gbc_gb2312_to_utf8_capacity(const uint8_t *src, size_t src_length,
                                               size_t *out_length);

GBC_API gbc_status gbc_utf8_to_gb2312(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, size_t *out_length);

GBC_API gbc_status gbc_gb2312_to_utf8(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, size_t *out_length);

GBC_API gbc_status gbc_utf8_to_gb2312_z(const char *src, char *dst, size_t dst_capacity,
                                        size_t *out_length);

GBC_API gbc_status gbc_gb2312_to_utf8_z(const char *src, char *dst, size_t dst_capacity,
                                        size_t *out_length);

/*
 * Lossy conversion.
 *
 * The *_replace functions behave like their strict counterparts, except that a
 * character with no mapping in the target encoding is substituted instead of
 * failing with GBC_E_UNMAPPABLE. This covers two distinct cases:
 *
 *   - UTF-8 to GB2312: the code point is outside GB2312 (for example an emoji
 *     or U+20AC).
 *   - GB2312 to UTF-8: the byte pair is well formed but unassigned (for
 *     example AA A1), which happens with data produced by GBK or by encoders
 *     that filled the user-defined areas.
 *
 * Only GBC_E_UNMAPPABLE is recovered from. Malformed input is still fatal:
 * GBC_E_INVALID_SEQUENCE and GBC_E_TRUNCATED are returned exactly as before,
 * because the length of the offending sequence is unknown and silently
 * dropping bytes would desynchronise the rest of the string.
 *
 * Contract additions:
 *
 *   - replacement is a Unicode code point, not an encoded byte sequence; it is
 *     encoded with the target encoding internally. It must be representable in
 *     the target encoding (for GB2312 that rules out everything above U+FFFF
 *     and unassigned code points), otherwise GBC_E_INVALID_ARGUMENT is
 *     returned before dst is touched.
 *   - replacement == 0 deletes the character instead of substituting it.
 *   - On success *out_replaced receives the number of characters substituted or
 *     deleted; it may be NULL and is left untouched on failure.
 *   - These functions never return GBC_E_UNMAPPABLE.
 *
 * No separate measuring function is needed, because the result is bounded by
 * the input: a UTF-8 sequence is never shorter than its GB2312 encoding (at
 * most two bytes), and a GB2312 pair expands to at most three UTF-8 bytes.
 * Hence src_length + 1 bytes suffice for UTF-8 to GB2312 and 3 * src_length + 1
 * for the other direction, in both cases including the terminator.
 */
GBC_API gbc_status gbc_utf8_to_gb2312_replace(const uint8_t *src, size_t src_length,
                                              uint8_t *dst, size_t dst_capacity,
                                              uint32_t replacement, size_t *out_length,
                                              size_t *out_replaced);

GBC_API gbc_status gbc_gb2312_to_utf8_replace(const uint8_t *src, size_t src_length,
                                              uint8_t *dst, size_t dst_capacity,
                                              uint32_t replacement, size_t *out_length,
                                              size_t *out_replaced);

GBC_API gbc_status gbc_utf8_to_gb2312_replace_z(const char *src, char *dst, size_t dst_capacity,
                                                uint32_t replacement, size_t *out_length,
                                                size_t *out_replaced);

GBC_API gbc_status gbc_gb2312_to_utf8_replace_z(const char *src, char *dst, size_t dst_capacity,
                                                uint32_t replacement, size_t *out_length,
                                                size_t *out_replaced);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GBCODEC_CONVERT_H */
