// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * Embeds the log viewer page into the executable. Each .html file holds a
 * raw string literal, so including it here yields the page text at compile
 * time and the server needs no files on the target device.
 *
 * The page is shared by both web front ends: log_page_head.html holds the
 * layout, the styling and everything that renders lines, a transport file
 * (log_page_sse.html or log_page_ws.html) adds connect(), disconnect() and
 * isConnected(), and log_page_tail.html binds the buttons and closes the
 * document. The three literals are adjacent, so the compiler concatenates
 * them into one string at compile time; each build therefore carries only the
 * transport it can actually serve.
 *
 * The page holds __APP_NAME__ and __APP_VERSION__ tokens, which the about
 * dialog shows. They are filled in from common.h on the first request, so the
 * version on the page cannot drift from the one the program reports.
 */

#include "html_page.hh"

#include <cstddef>
#include <cstring>
#include <string>

#include "common.h"

namespace {

// Replaces every occurrence of token in text with value.
void replaceAll(std::string& text, const char* token, const char* value) {
  const std::size_t token_len = std::strlen(token);
  const std::size_t value_len = std::strlen(value);
  std::string::size_type pos = text.find(token);
  while (pos != std::string::npos) {
    text.replace(pos, token_len, value);
    // Carry on past what was inserted, so a value holding the token cannot loop.
    pos = text.find(token, pos + value_len);
  }
}

std::string fillAppInfo(const char* page) {
  std::string text(page);
  replaceAll(text, "__APP_NAME__", APP_NAME);
  replaceAll(text, "__APP_VERSION__", APP_VERSION_STR);
  return text;
}

}  // namespace

const char* get_sse_log_page_html() {
  static const std::string page = fillAppInfo(
    #include "html/log_page_head.html"
    #include "html/log_page_sse.html"
    #include "html/log_page_tail.html"
  );
  return page.c_str();
}

const char* get_ws_log_page_html() {
  static const std::string page = fillAppInfo(
    #include "html/log_page_head.html"
    #include "html/log_page_ws.html"
    #include "html/log_page_tail.html"
  );
  return page.c_str();
}

std::string htmlEscape(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    switch (text[i]) {
      case '&':
        escaped += "&amp;";
        break;
      case '<':
        escaped += "&lt;";
        break;
      case '>':
        escaped += "&gt;";
        break;
      case '"':
        escaped += "&quot;";
        break;
      case '\'':
        escaped += "&#39;";
        break;
      default:
        escaped += text[i];
        break;
    }
  }
  return escaped;
}