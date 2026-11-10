/*
 * gbcodec - read-only string view used by the C++ wrapper.
 *
 * Copyright (c) 2016-2026 leid
 * SPDX-License-Identifier: ISC
 *
 * The C++ wrapper needs a non-owning view of a byte string that works with
 * both C++11 and C++17. In C++17 mode gbcodec::string_view is simply
 * std::string_view; otherwise it is the minimal equivalent declared below.
 * User code only has to name gbcodec::string_view and works in both modes.
 */
#ifndef GBCODEC_STRING_VIEW_HPP
#define GBCODEC_STRING_VIEW_HPP

#include <cstddef>
#include <string>

/*
 * Effective language level. MSVC keeps __cplusplus at 199711L unless
 * /Zc:__cplusplus is passed, so prefer its own _MSVC_LANG macro.
 */
#if defined(_MSVC_LANG)
#define GBCODEC_CPLUSPLUS _MSVC_LANG
#else
#define GBCODEC_CPLUSPLUS __cplusplus
#endif

/* Use std::string_view when the library provides it and we are in C++17. */
#if defined(__has_include)
#if __has_include(<string_view>)
#define GBCODEC_HAVE_STRING_VIEW_HEADER 1
#endif
#endif

#if defined(GBCODEC_HAVE_STRING_VIEW_HEADER) && GBCODEC_CPLUSPLUS >= 201703L
#include <string_view>
#define GBCODEC_USE_STD_STRING_VIEW 1
#endif

namespace gbcodec {

#if defined(GBCODEC_USE_STD_STRING_VIEW)

/* C++17: the standard view, so std::string_view interoperates directly. */
using string_view = std::string_view;

#else

/*
 * C++11 fallback with the subset of std::string_view that the wrapper needs.
 *
 * Like std::string_view it does not own the characters: the referenced buffer
 * must stay alive (and unchanged) for as long as the view is used.
 */
class string_view {
public:
    string_view() noexcept : data_(nullptr), size_(0) {}

    string_view(const char *text) noexcept
        : data_(text),
          size_(text == nullptr ? std::size_t(0) : std::char_traits<char>::length(text))
    {
    }

    string_view(const std::string &text) noexcept : data_(text.data()), size_(text.size()) {}

    string_view(const char *text, std::size_t length) noexcept : data_(text), size_(length) {}

    const char *data() const noexcept
    {
        return data_;
    }

    std::size_t size() const noexcept
    {
        return size_;
    }

    bool empty() const noexcept
    {
        return size_ == 0;
    }

private:
    const char *data_;
    std::size_t size_;
};

#endif /* GBCODEC_USE_STD_STRING_VIEW */

} /* namespace gbcodec */

#endif /* GBCODEC_STRING_VIEW_HPP */
