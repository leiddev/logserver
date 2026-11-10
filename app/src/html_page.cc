// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * Embeds the log viewer pages into the executable. Each .html file holds a
 * raw string literal, so including it here yields the page text at compile
 * time and the server needs no files on the target device.
 */

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