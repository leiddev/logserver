// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
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