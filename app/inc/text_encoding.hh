// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Turns a raw log line into text a browser can render. Log producers on this
 * platform are not consistent about encoding, so the input may be UTF-8 (the
 * common case), GB2312, or something that is neither.
 */
#pragma once
#ifndef _TEXT_ENCODING_HH_
#define _TEXT_ENCODING_HH_

#include <string>

namespace textenc {

// Returns s unchanged when it is valid UTF-8. Otherwise the line is converted
// from GB2312, with bytes that are not valid GB2312 either replaced by U+FFFD.
// Never returns an empty string for a non-empty input.
std::string toDisplayUtf8(const std::string &s);

}  // namespace textenc

#endif  // !_TEXT_ENCODING_HH_
