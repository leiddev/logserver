/*
 * Leid 2025.2
 */

const char* log_page_html =
  #include "html/log_page.html"
;

const char* sse_log_page_html =
  #include "html/sse_log_page.html"
;

const char* get_log_page_html() {
  return log_page_html;
}

const char* get_sse_log_page_html() {
  return sse_log_page_html;
}