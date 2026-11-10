/*
 * gbcodec - whole-string conversion between UTF-8 and GB2312.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#include <gbcodec/convert.h>

#include <string.h>

#include <gbcodec/gb2312.h>
#include <gbcodec/utf8.h>

/*
 * Per-call policy shared by the two passes of a conversion.
 *
 * The same state is used for measuring and for writing, so both passes always
 * agree on the size of the result. When replace is zero an unmappable
 * character aborts the conversion; otherwise it is substituted by replacement,
 * or deleted when replacement is zero.
 */
typedef struct gbc_write_state {
    int replace;
    uint32_t replacement;
    size_t replaced;
} gbc_write_state;

/*
 * Converts a counted UTF-8 string to GB2312. When dst is NULL the function
 * only measures the result. Nothing is written to dst when an error occurs
 * after the first pass, because the public entry points measure first.
 */
static gbc_status gbc_write_gb2312(gbc_write_state *state, const uint8_t *src, size_t src_length,
                                   uint8_t *dst, size_t *out_written)
{
    size_t offset = 0u;
    size_t written = 0u;

    while (offset < src_length) {
        uint32_t code_point = 0u;
        size_t consumed = 0u;
        uint8_t encoded[2];
        size_t produced;
        gbc_status status;

        status = gbc_utf8_decode(src + offset, src_length - offset, &code_point, &consumed);
        if (status != GBC_OK) {
            return status;
        }

        produced = gbc_gb2312_encode(code_point, encoded);
        if (produced == 0u) {
            if (!state->replace) {
                return GBC_E_UNMAPPABLE;
            }
            ++state->replaced;
            if (state->replacement == 0u) {
                /* Deletion: consume the character without emitting anything. */
                offset += consumed;
                continue;
            }
            produced = gbc_gb2312_encode(state->replacement, encoded);
            if (produced == 0u) {
                /* Rejected before the conversion started. */
                return GBC_E_INVALID_ARGUMENT;
            }
        }

        if (dst != NULL) {
            dst[written] = encoded[0];
            if (produced > 1u) {
                dst[written + 1u] = encoded[1];
            }
        }
        written += produced;
        offset += consumed;
    }

    *out_written = written;
    return GBC_OK;
}

/* Converts a counted GB2312 string to UTF-8; see gbc_write_gb2312(). */
static gbc_status gbc_write_utf8(gbc_write_state *state, const uint8_t *src, size_t src_length,
                                 uint8_t *dst, size_t *out_written)
{
    size_t offset = 0u;
    size_t written = 0u;

    while (offset < src_length) {
        uint32_t code_point = 0u;
        size_t consumed = 0u;
        uint8_t encoded[4];
        size_t produced;
        size_t index;
        gbc_status status;

        status = gbc_gb2312_decode(src + offset, src_length - offset, &code_point, &consumed);
        if (status != GBC_OK) {
            if (status != GBC_E_UNMAPPABLE || !state->replace) {
                return status;
            }
            /*
             * An unassigned byte pair is well formed, so its length is known
             * even though *consumed was not filled in: a lead byte in
             * A1..F7 is always followed by one trail byte.
             */
            ++state->replaced;
            consumed = (src[offset] < 0x80u) ? 1u : 2u;
            code_point = state->replacement;
            if (code_point == 0u) {
                offset += consumed;
                continue;
            }
        }

        produced = gbc_utf8_encode(code_point, encoded);
        if (produced == 0u) {
            /*
             * Unreachable for decoded input; only a replacement code point
             * that cannot be encoded gets here, and those are rejected before
             * the conversion starts.
             */
            return GBC_E_INVALID_ARGUMENT;
        }

        if (dst != NULL) {
            for (index = 0u; index < produced; ++index) {
                dst[written + index] = encoded[index];
            }
        }
        written += produced;
        offset += consumed;
    }

    *out_written = written;
    return GBC_OK;
}

typedef gbc_status (*gbc_run_function)(gbc_write_state *, const uint8_t *, size_t, uint8_t *,
                                       size_t *);

static gbc_status gbc_convert_counted(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, size_t *out_length,
                                      size_t *out_replaced, gbc_run_function run,
                                      gbc_write_state *state)
{
    size_t needed = 0u;
    size_t replaced = 0u;
    gbc_status status;

    if (src == NULL || dst == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }

    status = run(state, src, src_length, NULL, &needed);
    if (status != GBC_OK) {
        if (dst_capacity > 0u) {
            dst[0] = 0u;
        }
        return status;
    }

    /* The write pass applies the same policy and counts the same characters. */
    replaced = state->replaced;

    /* One byte is always reserved for the terminating NUL. */
    if (needed != 0u && needed + 1u > dst_capacity) {
        if (dst_capacity > 0u) {
            dst[0] = 0u;
        }
        return GBC_E_BUFFER_TOO_SMALL;
    }

    if (dst_capacity > 0u) {
        size_t written = 0u;

        status = run(state, src, src_length, dst, &written);
        if (status != GBC_OK) {
            dst[0] = 0u;
            return status;
        }
        dst[written] = 0u;
        if (out_length != NULL) {
            *out_length = written;
        }
    } else if (out_length != NULL) {
        *out_length = 0u;
    }

    if (out_replaced != NULL) {
        *out_replaced = replaced;
    }

    return GBC_OK;
}

/*
 * Checks that a substitution code point can be expressed in the target
 * encoding. Zero is the documented "delete the character" request and needs no
 * representation.
 */
static gbc_status gbc_check_replacement(uint32_t replacement, int to_gb2312)
{
    uint8_t encoded[4];

    if (replacement == 0u) {
        return GBC_OK;
    }
    if (to_gb2312) {
        return gbc_gb2312_encode(replacement, encoded) == 0u ? GBC_E_INVALID_ARGUMENT : GBC_OK;
    }
    return gbc_utf8_encode(replacement, encoded) == 0u ? GBC_E_INVALID_ARGUMENT : GBC_OK;
}

static gbc_status gbc_convert_replace(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, uint32_t replacement,
                                      size_t *out_length, size_t *out_replaced,
                                      gbc_run_function run, int to_gb2312)
{
    gbc_write_state state;
    gbc_status status;

    if (src == NULL || dst == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }

    status = gbc_check_replacement(replacement, to_gb2312);
    if (status != GBC_OK) {
        if (dst_capacity > 0u) {
            dst[0] = 0u;
        }
        return status;
    }

    state.replace = 1;
    state.replacement = replacement;
    state.replaced = 0u;

    return gbc_convert_counted(src, src_length, dst, dst_capacity, out_length, out_replaced, run,
                               &state);
}

gbc_status gbc_utf8_to_gb2312_capacity(const uint8_t *src, size_t src_length, size_t *out_length)
{
    gbc_write_state state = {0, 0u, 0u};
    size_t needed = 0u;
    gbc_status status;

    if (src == NULL || out_length == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }

    status = gbc_write_gb2312(&state, src, src_length, NULL, &needed);
    if (status != GBC_OK) {
        return status;
    }

    *out_length = needed;
    return GBC_OK;
}

gbc_status gbc_gb2312_to_utf8_capacity(const uint8_t *src, size_t src_length, size_t *out_length)
{
    gbc_write_state state = {0, 0u, 0u};
    size_t needed = 0u;
    gbc_status status;

    if (src == NULL || out_length == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }

    status = gbc_write_utf8(&state, src, src_length, NULL, &needed);
    if (status != GBC_OK) {
        return status;
    }

    *out_length = needed;
    return GBC_OK;
}

gbc_status gbc_utf8_to_gb2312(const uint8_t *src, size_t src_length, uint8_t *dst,
                              size_t dst_capacity, size_t *out_length)
{
    gbc_write_state state = {0, 0u, 0u};

    return gbc_convert_counted(src, src_length, dst, dst_capacity, out_length, NULL,
                               gbc_write_gb2312, &state);
}

gbc_status gbc_gb2312_to_utf8(const uint8_t *src, size_t src_length, uint8_t *dst,
                              size_t dst_capacity, size_t *out_length)
{
    gbc_write_state state = {0, 0u, 0u};

    return gbc_convert_counted(src, src_length, dst, dst_capacity, out_length, NULL,
                               gbc_write_utf8, &state);
}

gbc_status gbc_utf8_to_gb2312_replace(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, uint32_t replacement,
                                      size_t *out_length, size_t *out_replaced)
{
    return gbc_convert_replace(src, src_length, dst, dst_capacity, replacement, out_length,
                               out_replaced, gbc_write_gb2312, 1);
}

gbc_status gbc_gb2312_to_utf8_replace(const uint8_t *src, size_t src_length, uint8_t *dst,
                                      size_t dst_capacity, uint32_t replacement,
                                      size_t *out_length, size_t *out_replaced)
{
    return gbc_convert_replace(src, src_length, dst, dst_capacity, replacement, out_length,
                               out_replaced, gbc_write_utf8, 0);
}

gbc_status gbc_utf8_to_gb2312_z(const char *src, char *dst, size_t dst_capacity,
                                size_t *out_length)
{
    if (src == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    return gbc_utf8_to_gb2312((const uint8_t *)src, strlen(src), (uint8_t *)dst, dst_capacity,
                              out_length);
}

gbc_status gbc_gb2312_to_utf8_z(const char *src, char *dst, size_t dst_capacity,
                                size_t *out_length)
{
    if (src == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    return gbc_gb2312_to_utf8((const uint8_t *)src, strlen(src), (uint8_t *)dst, dst_capacity,
                              out_length);
}

gbc_status gbc_utf8_to_gb2312_replace_z(const char *src, char *dst, size_t dst_capacity,
                                        uint32_t replacement, size_t *out_length,
                                        size_t *out_replaced)
{
    if (src == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    return gbc_utf8_to_gb2312_replace((const uint8_t *)src, strlen(src), (uint8_t *)dst,
                                      dst_capacity, replacement, out_length, out_replaced);
}

gbc_status gbc_gb2312_to_utf8_replace_z(const char *src, char *dst, size_t dst_capacity,
                                        uint32_t replacement, size_t *out_length,
                                        size_t *out_replaced)
{
    if (src == NULL) {
        return GBC_E_INVALID_ARGUMENT;
    }
    return gbc_gb2312_to_utf8_replace((const uint8_t *)src, strlen(src), (uint8_t *)dst,
                                      dst_capacity, replacement, out_length, out_replaced);
}
