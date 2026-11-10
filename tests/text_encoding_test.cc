// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Unit tests for textenc::toDisplayUtf8. The cases mirror the bytes the ARM
 * target was probed with: producers send UTF-8 today, but GB2312 and plain
 * garbage both reach the server as well and must not lose the line.
 */

#include "test_framework.hh"
#include "text_encoding.hh"

#include <cstdio>
#include <string>

namespace {

// 测试中文日志
const char *const kChineseUtf8 = "\xe6\xb5\x8b\xe8\xaf\x95\xe4\xb8\xad\xe6\x96\x87\xe6\x97\xa5\xe5\xbf\x97";
const char *const kChineseGb2312 = "\xb2\xe2\xca\xd4\xd6\xd0\xce\xc4\xc8\xd5\xd6\xbe";
const char *const kReplacement = "\xef\xbf\xbd";  // U+FFFD

std::string bytes(const char *data, std::size_t size) {
  return std::string(data, size);
}

// True when every byte decodes as UTF-8, per RFC 3629. Written out here so
// the test does not depend on the implementation it is checking.
bool isValidUtf8(const std::string &s) {
  std::size_t offset = 0;
  while (offset < s.size()) {
    const unsigned char lead = static_cast<unsigned char>(s[offset]);
    std::size_t trailing = 0;
    if (lead < 0x80) {
      trailing = 0;
    } else if ((lead & 0xE0) == 0xC0) {
      trailing = 1;
    } else if ((lead & 0xF0) == 0xE0) {
      trailing = 2;
    } else if ((lead & 0xF8) == 0xF0) {
      trailing = 3;
    } else {
      return false;  // stray continuation byte or invalid lead
    }
    if (offset + trailing >= s.size() && trailing > 0) {
      return false;  // truncated sequence
    }
    for (std::size_t i = 1; i <= trailing; ++i) {
      const unsigned char cont = static_cast<unsigned char>(s[offset + i]);
      if ((cont & 0xC0) != 0x80) {
        return false;
      }
    }
    offset += trailing + 1;
  }
  return true;
}

bool contains(const std::string &haystack, const std::string &needle) {
  return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST_CASE("valid UTF-8 is returned unchanged") {
  const std::string ascii = "plain ascii line\n";
  CHECK_EQ(ascii, textenc::toDisplayUtf8(ascii));

  const std::string chinese(kChineseUtf8);
  CHECK_EQ(chinese, textenc::toDisplayUtf8(chinese));

  const std::string mixed = std::string("[2026-11-10 10:00:00.000] ") + kChineseUtf8 + "\n";
  CHECK_EQ(mixed, textenc::toDisplayUtf8(mixed));
}

TEST_CASE("GB2312 is converted to UTF-8") {
  const std::string gb(kChineseGb2312);
  CHECK_EQ(std::string(kChineseUtf8), textenc::toDisplayUtf8(gb));
}

TEST_CASE("GB2312 keeps an ASCII prefix intact") {
  const std::string input = std::string("[2026-11-10 10:00:00.000] [midcontroller] ") + kChineseGb2312;
  const std::string expected =
      std::string("[2026-11-10 10:00:00.000] [midcontroller] ") + kChineseUtf8;
  CHECK_EQ(expected, textenc::toDisplayUtf8(input));
}

TEST_CASE("GB2312 punctuation is converted") {
  // A1 A1 is the ideographic space U+3000.
  const std::string input = bytes("\xa1\xa1", 2);
  CHECK_EQ(std::string("\xe3\x80\x80"), textenc::toDisplayUtf8(input));
}

TEST_CASE("bytes that are neither UTF-8 nor GB2312 become U+FFFD") {
  // e6 b5 8b e8 is the prefix that used to crash the 32 bit build.
  const std::string input = bytes("\xe6\xb5\x8b\xe8", 4);
  const std::string out = textenc::toDisplayUtf8(input);

  CHECK(!out.empty());
  CHECK(isValidUtf8(out));
  CHECK(contains(out, kReplacement));
}

TEST_CASE("a lone high byte becomes U+FFFD") {
  const std::string input = bytes("\xe9", 1);
  const std::string out = textenc::toDisplayUtf8(input);

  CHECK(!out.empty());
  CHECK(isValidUtf8(out));
  CHECK(contains(out, kReplacement));
}

TEST_CASE("garbage next to valid text keeps the valid part") {
  const std::string input = std::string("keep ") + bytes("\xe6\xb5\x8b\xe8", 4) + " tail";
  const std::string out = textenc::toDisplayUtf8(input);

  CHECK(isValidUtf8(out));
  CHECK(contains(out, "keep"));
  CHECK(contains(out, "tail"));
}

TEST_CASE("an embedded NUL byte does not truncate the line") {
  const std::string input = bytes("before\0after", 12);
  const std::string out = textenc::toDisplayUtf8(input);

  CHECK_EQ(std::size_t(12), out.size());
}

TEST_CASE("an empty string stays empty") {
  CHECK_EQ(std::string(), textenc::toDisplayUtf8(std::string()));
}

int main() {
  return testfw::run_all();
}
