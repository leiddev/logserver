/*
 * gbcodec - error codes.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_STATUS_H
#define GBCODEC_STATUS_H

#include <gbcodec/export.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Status codes returned by the conversion functions.
 *
 * GBC_OK is zero so that callers can test the return value directly. Every
 * other value describes a specific failure and is stable across releases.
 */
typedef enum gbc_status {
    GBC_OK = 0,                 /* Success. */
    GBC_E_INVALID_ARGUMENT = 1, /* A pointer argument was NULL. */
    GBC_E_INVALID_SEQUENCE = 2, /* The input bytes are not a valid sequence. */
    GBC_E_TRUNCATED = 3,        /* The input ends in the middle of a sequence. */
    GBC_E_UNMAPPABLE = 4,       /* The character has no counterpart in the
                                   target character set. */
    GBC_E_BUFFER_TOO_SMALL = 5  /* The destination buffer is too small. */
} gbc_status;

/*
 * Returns a short, static, English description of a status code. Never
 * returns NULL; unknown values yield "unknown status".
 */
GBC_API const char *gbc_status_string(gbc_status status);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GBCODEC_STATUS_H */
