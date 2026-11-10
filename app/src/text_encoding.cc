// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Implementation of the display conversion. Everything here goes through the
 * gbcodec C API rather than the C++ wrapper: the wrapper signals failure with
 * exceptions, and this code must not throw on malformed input.
 */
#include "text_encoding.hh"

#include <gbcodec/gbcodec.h>

#include <cstddef>
#include <cstdint>

namespace {

const std::uint32_t kReplacementCodePoint = 0xFFFD;  // U+FFFD REPLACEMENT CHARACTER

// True when every byte of s decodes as UTF-8.
bool isValidUtf8(const std::string &s) {
  std::size_t offset = 0;
  while (offset < s.size()) {
    std::uint32_t code_point = 0;
    std::size_t consumed = 0;
    const gbc_status status =
        gbc_utf8_decode(reinterpret_cast<const std::uint8_t *>(s.data()) + offset, s.size() - offset,
                        &code_point, &consumed);
    if (status != GBC_OK || consumed == 0) {
      return false;
    }
    offset += consumed;
  }
  return true;
}

// Whole-string GB2312 to UTF-8 with substitutions. Returns false when the
// input is not even a valid GB2312 byte sequence.
bool convertFromGb2312(const std::string &s, std::string *out) {
  out->resize(s.size() * 3 + 1);  // worst case per the gbcodec contract
  std::size_t written = 0;
  std::size_t replaced = 0;
  const gbc_status status = gbc_gb2312_to_utf8_replace(
      reinterpret_cast<const std::uint8_t *>(s.data()), s.size(),
      reinterpret_cast<std::uint8_t *>(&(*out)[0]), out->size(), kReplacementCodePoint, &written,
      &replaced);
  if (status != GBC_OK) {
    out->clear();
    return false;
  }
  out->resize(written);
  return true;
}

void appendCodePoint(std::uint32_t code_point, std::string *out) {
  std::uint8_t encoded[4] = {0, 0, 0, 0};
  const std::size_t length = gbc_utf8_encode(code_point, encoded);
  out->append(reinterpret_cast<const char *>(encoded), length);
}

// Last resort: walk the bytes, keeping whatever decodes either way and
// substituting a single U+FFFD for a byte that decodes as neither.
std::string salvage(const std::string &s) {
  const std::uint8_t *bytes = reinterpret_cast<const std::uint8_t *>(s.data());
  std::string result;
  result.reserve(s.size());

  std::size_t offset = 0;
  while (offset < s.size()) {
    std::uint32_t code_point = 0;
    std::size_t consumed = 0;

    if (gbc_utf8_decode(bytes + offset, s.size() - offset, &code_point, &consumed) == GBC_OK &&
        consumed > 0) {
      result.append(s, offset, consumed);
      offset += consumed;
      continue;
    }

    consumed = 0;
    if (gbc_gb2312_decode(bytes + offset, s.size() - offset, &code_point, &consumed) == GBC_OK &&
        consumed > 0) {
      appendCodePoint(code_point, &result);
      offset += consumed;
      continue;
    }

    appendCodePoint(kReplacementCodePoint, &result);
    ++offset;
  }
  return result;
}

}  // namespace

namespace textenc {

std::string toDisplayUtf8(const std::string &s) {
  if (s.empty()) {
    return s;
  }
  if (isValidUtf8(s)) {
    return s;
  }

  std::string converted;
  if (convertFromGb2312(s, &converted)) {
    return converted;
  }

  return salvage(s);
}

}  // namespace textenc
