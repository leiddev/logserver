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
 */

#include "html_page.hh"

#include <cstddef>

const char* sse_log_page_html =
  #include "html/log_page_head.html"
  #include "html/log_page_sse.html"
  #include "html/log_page_tail.html"
;

const char* ws_log_page_html =
  #include "html/log_page_head.html"
  #include "html/log_page_ws.html"
  #include "html/log_page_tail.html"
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