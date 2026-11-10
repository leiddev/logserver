/*
 * gbcodec - C++ wrapper around the C API.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 *
 * This header is header-only: it needs only the C library. All functions are
 * inline, so including it from several translation units is safe.
 *
 * The wrapper requires C++11. When the compiler runs in C++17 mode it uses
 * std::string_view; otherwise it falls back to an equivalent read-only view
 * declared in <gbcodec/string_view.hpp>. In both cases the public parameter
 * type is gbcodec::string_view, so user code does not have to care.
 */
#ifndef GBCODEC_GBCODEC_HPP
#define GBCODEC_GBCODEC_HPP

#include <gbcodec/gbcodec.h>
#include <gbcodec/string_view.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace gbcodec {

/* Exception thrown by the throwing conversion helpers. */
class error : public std::runtime_error {
public:
    explicit error(gbc_status status)
        : std::runtime_error(gbc_status_string(status)), status_(status)
    {
    }

    /* The status code that caused the failure. */
    gbc_status status() const noexcept
    {
        return status_;
    }

private:
    gbc_status status_;
};

namespace detail {

using count_function = gbc_status (*)(const std::uint8_t *, std::size_t, std::size_t *);
using convert_function = gbc_status (*)(const std::uint8_t *, std::size_t, std::uint8_t *,
                                        std::size_t, std::size_t *);
using replace_function = gbc_status (*)(const std::uint8_t *, std::size_t, std::uint8_t *,
                                        std::size_t, std::uint32_t, std::size_t *,
                                        std::size_t *);

/* Measures the result, then converts into a buffer of exactly the right size. */
inline void run(count_function count, convert_function convert, string_view input,
                std::string &output)
{
    /*
     * A default-constructed std::string_view has a null data() pointer, which
     * the C API rejects. An empty input always yields an empty output, so
     * short-circuit it here to keep "" and std::string_view{} equivalent.
     */
    if (input.empty()) {
        output.clear();
        return;
    }

    const auto *const src = reinterpret_cast<const std::uint8_t *>(input.data());
    std::size_t needed = 0;
    gbc_status status = count(src, input.size(), &needed);
    if (status != GBC_OK) {
        throw error(status);
    }

    output.resize(needed + 1);
    std::size_t written = 0;
    status = convert(src, input.size(), reinterpret_cast<std::uint8_t *>(&output[0]),
                     output.size(), &written);
    if (status != GBC_OK) {
        output.clear();
        throw error(status);
    }
    output.resize(written);
}

inline bool try_run(count_function count, convert_function convert, string_view input,
                    std::string &output, gbc_status *status) noexcept
{
    try {
        run(count, convert, input, output);
    } catch (const error &failure) {
        output.clear();
        if (status != nullptr) {
            *status = failure.status();
        }
        return false;
    } catch (...) {
        output.clear();
        if (status != nullptr) {
            *status = GBC_E_INVALID_ARGUMENT;
        }
        return false;
    }

    if (status != nullptr) {
        *status = GBC_OK;
    }
    return true;
}

/*
 * Lossy conversion: the result never needs more than output_scale bytes per
 * input byte plus the terminator (1 for GB2312, 3 for UTF-8), so no measuring
 * pass is required and one buffer suffices.
 */
inline void run_replace(replace_function convert, string_view input, std::uint32_t replacement,
                        std::size_t output_scale, std::string &output, std::size_t *replaced)
{
    /*
     * Unlike run(), an empty input is still passed to the C API: it validates
     * the replacement code point and reports an unusable one even when there is
     * nothing to convert. A default-constructed view has a null data() pointer,
     * so substitute a readable dummy address.
     */
    static const std::uint8_t empty_input = 0u;
    const auto *const src = input.empty() ? &empty_input
                                          : reinterpret_cast<const std::uint8_t *>(input.data());

    output.resize(input.size() * output_scale + 1u);

    std::size_t written = 0;
    std::size_t count = 0;
    const gbc_status status =
        convert(src, input.size(), reinterpret_cast<std::uint8_t *>(&output[0]), output.size(),
                replacement, &written, &count);
    if (status != GBC_OK) {
        output.clear();
        throw error(status);
    }

    output.resize(written);
    if (replaced != nullptr) {
        *replaced = count;
    }
}

inline bool try_run_replace(replace_function convert, string_view input, std::uint32_t replacement,
                            std::size_t output_scale, std::string &output, std::size_t *replaced,
                            gbc_status *status) noexcept
{
    try {
        run_replace(convert, input, replacement, output_scale, output, replaced);
    } catch (const error &failure) {
        output.clear();
        if (status != nullptr) {
            *status = failure.status();
        }
        return false;
    } catch (...) {
        output.clear();
        if (status != nullptr) {
            *status = GBC_E_INVALID_ARGUMENT;
        }
        return false;
    }

    if (status != nullptr) {
        *status = GBC_OK;
    }
    return true;
}

} /* namespace detail */

/* Returns the English description of a status code. */
inline const char *status_string(gbc_status status) noexcept
{
    return gbc_status_string(status);
}

/*
 * Converts a UTF-8 string to GB2312.
 *
 * Throws gbcodec::error when the input is malformed or contains a character
 * that GB2312 cannot represent.
 */
inline std::string to_gb2312(string_view utf8)
{
    std::string result;
    detail::run(gbc_utf8_to_gb2312_capacity, gbc_utf8_to_gb2312, utf8, result);
    return result;
}

/*
 * Lossy variant of to_gb2312(): characters that GB2312 cannot represent are
 * substituted by replacement, a Unicode code point, instead of throwing.
 * Pass 0 to delete them. Malformed input still throws gbcodec::error.
 */
inline std::string to_gb2312(string_view utf8, char32_t replacement)
{
    std::string result;
    detail::run_replace(gbc_utf8_to_gb2312_replace, utf8,
                        static_cast<std::uint32_t>(replacement), 1u, result, nullptr);
    return result;
}

/* As above; replaced receives the number of substituted or deleted characters. */
inline std::string to_gb2312(string_view utf8, char32_t replacement, std::size_t &replaced)
{
    std::string result;
    detail::run_replace(gbc_utf8_to_gb2312_replace, utf8,
                        static_cast<std::uint32_t>(replacement), 1u, result, &replaced);
    return result;
}

/* Converts a GB2312 string to UTF-8; throws gbcodec::error on failure. */
inline std::string from_gb2312(string_view gb2312)
{
    std::string result;
    detail::run(gbc_gb2312_to_utf8_capacity, gbc_gb2312_to_utf8, gb2312, result);
    return result;
}

/*
 * Lossy variant of from_gb2312(): unassigned byte pairs are substituted by
 * replacement (U+FFFD is the usual choice) instead of throwing. Pass 0 to
 * delete them. Malformed input still throws gbcodec::error.
 */
inline std::string from_gb2312(string_view gb2312, char32_t replacement)
{
    std::string result;
    detail::run_replace(gbc_gb2312_to_utf8_replace, gb2312,
                        static_cast<std::uint32_t>(replacement), 3u, result, nullptr);
    return result;
}

/* As above; replaced receives the number of substituted or deleted characters. */
inline std::string from_gb2312(string_view gb2312, char32_t replacement, std::size_t &replaced)
{
    std::string result;
    detail::run_replace(gbc_gb2312_to_utf8_replace, gb2312,
                        static_cast<std::uint32_t>(replacement), 3u, result, &replaced);
    return result;
}

/*
 * Non-throwing variant of to_gb2312(). Returns false and leaves gb2312 empty
 * on failure; status, when not null, receives the reason.
 */
inline bool try_to_gb2312(string_view utf8, std::string &gb2312,
                          gbc_status *status = nullptr) noexcept
{
    return detail::try_run(gbc_utf8_to_gb2312_capacity, gbc_utf8_to_gb2312, utf8, gb2312, status);
}

/*
 * Non-throwing variant of the lossy to_gb2312(). replaced, when not null,
 * receives the number of substituted or deleted characters.
 */
inline bool try_to_gb2312(string_view utf8, char32_t replacement, std::string &gb2312,
                          std::size_t *replaced = nullptr,
                          gbc_status *status = nullptr) noexcept
{
    return detail::try_run_replace(gbc_utf8_to_gb2312_replace, utf8,
                                   static_cast<std::uint32_t>(replacement), 1u, gb2312, replaced,
                                   status);
}

/*
 * Non-throwing variant of from_gb2312(). Returns false and leaves utf8 empty
 * on failure; status, when not null, receives the reason.
 */
inline bool try_from_gb2312(string_view gb2312, std::string &utf8,
                            gbc_status *status = nullptr) noexcept
{
    return detail::try_run(gbc_gb2312_to_utf8_capacity, gbc_gb2312_to_utf8, gb2312, utf8, status);
}

/*
 * Non-throwing variant of the lossy from_gb2312(). replaced, when not null,
 * receives the number of substituted or deleted characters.
 */
inline bool try_from_gb2312(string_view gb2312, char32_t replacement, std::string &utf8,
                            std::size_t *replaced = nullptr,
                            gbc_status *status = nullptr) noexcept
{
    return detail::try_run_replace(gbc_gb2312_to_utf8_replace, gb2312,
                                   static_cast<std::uint32_t>(replacement), 3u, utf8, replaced,
                                   status);
}

} /* namespace gbcodec */

#endif /* GBCODEC_GBCODEC_HPP */
