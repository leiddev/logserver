// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * Accessors for the log viewer pages that are compiled into the executable.
 * One page per web server type; the matching one is served at "/" .
 */
#pragma once
#ifndef _HTML_PAGE_HH_
#define _HTML_PAGE_HH_

const char* get_ws_log_page_html();
const char* get_sse_log_page_html();

#endif  // !_HTML_PAGE_HH_