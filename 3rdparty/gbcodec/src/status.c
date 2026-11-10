/*
 * gbcodec - error code descriptions.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#include <gbcodec/status.h>

const char *gbc_status_string(gbc_status status)
{
    switch (status) {
    case GBC_OK:
        return "success";
    case GBC_E_INVALID_ARGUMENT:
        return "invalid argument";
    case GBC_E_INVALID_SEQUENCE:
        return "invalid byte sequence";
    case GBC_E_TRUNCATED:
        return "truncated byte sequence";
    case GBC_E_UNMAPPABLE:
        return "character cannot be represented";
    case GBC_E_BUFFER_TOO_SMALL:
        return "destination buffer too small";
    default:
        return "unknown status";
    }
}
