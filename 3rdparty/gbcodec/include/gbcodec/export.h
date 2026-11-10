/*
 * gbcodec - symbol visibility macros.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 */
#ifndef GBCODEC_EXPORT_H
#define GBCODEC_EXPORT_H

#if defined(_WIN32) && defined(GBCODEC_SHARED)
#if defined(GBCODEC_BUILDING_LIBRARY)
#define GBC_API __declspec(dllexport)
#else
#define GBC_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define GBC_API __attribute__((visibility("default")))
#else
#define GBC_API
#endif

#endif /* GBCODEC_EXPORT_H */
