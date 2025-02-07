#pragma once

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <vector>
#include <errno.h>
#include <thread>
#include <mutex>
#include <atomic>
#include "client_observer.h"
#include "pipe_ret_t.h"
#include "file_descriptor.h"

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

class TcpClient
{
private:
    FileDescriptor _sockfd;
    std::atomic<bool> _isConnected;
    std::atomic<bool> _isClosed;
    struct sockaddr_in _server;
    std::vector<client_observer_t> _subscibers;
    std::thread * _receiveTask = nullptr;
    std::mutex _subscribersMtx;

    void initializeSocket();
    void startReceivingMessages();
    void setAddress(const std::string& address, int port);
    void publishServerMsg(const char * msg, size_t msgSize);
    void publishServerDisconnected(const pipe_ret_t & ret);
    void receiveTask();
    void terminateReceiveThread();

public:
    TcpClient();
    ~TcpClient();
    pipe_ret_t connectTo(const std::string & address, int port);
    pipe_ret_t sendMsg(const char * msg, size_t size);

    void subscribe(const client_observer_t & observer);
    bool isConnected() const { return _isConnected; }
    pipe_ret_t close();
};

