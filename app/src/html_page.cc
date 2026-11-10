// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * Embeds the log viewer pages into the executable. Each .html file holds a
 * raw string literal, so including it here yields the page text at compile
 * time and the server needs no files on the target device.
 */

#include "html_page.hh"

#include <cstddef>

const char* ws_log_page_html =
  #include "html/ws_log_page.html"
;

const char* sse_log_page_html =
  #include "html/sse_log_page.html"
;

const char* get_ws_log_page_html() {
  return ws_log_page_html;
}

const char* get_sse_log_page_html() {
  return sse_log_page_html;
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