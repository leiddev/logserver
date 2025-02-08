/*
 * Leid 2025.2
 */
#pragma once
#ifndef _WEBSOCKET_SERVER_HH_
#define _WEBSOCKET_SERVER_HH_

void websocket_server_start(uint16_t port);
void websocket_server_stop();
void websocket_send_message(const std::string& message);

#endif  // !_WEBSOCKET_SERVER_HH_
