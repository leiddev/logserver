#pragma once

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <thread>
#include <functional>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <mutex>
#include "client.h"
#include "server_observer.h"
#include "pipe_ret_t.h"
#include "file_descriptor.h"

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>
#define uint UINT
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

class TcpServer {

private:

    FileDescriptor _sockfd;
    struct sockaddr_in _serverAddress;
    struct sockaddr_in _clientAddress;
    fd_set _fds;
    std::vector<Client*> _clients;
    std::vector<server_observer_t> _subscribers;

    std::mutex _subscribersMtx;
    std::mutex _clientsMtx;

    std::thread * _clientsRemoverThread = nullptr;
    std::atomic<bool> _stopRemoveClientsTask;

    bool _enableSend = false;

    void publishClientMsg(const Client & client, const char * msg, size_t msgSize);
    void publishClientDisconnected(const std::string&, const std::string&);
    pipe_ret_t waitForClient(uint32_t timeout);
    void clientEventHandler(const Client&, ClientEvent, const char * msg, size_t msgSize);
    void removeDeadClients();
    void terminateDeadClientsRemover();
    static pipe_ret_t sendToClient(const Client & client, const char * msg, size_t size);

public:
    TcpServer();
    ~TcpServer();
    pipe_ret_t start(int port, int maxNumOfClients = 5, bool removeDeadClientsAutomatically = true);
    void initializeSocket();
    void bindAddress(int port);
    void listenToClients(int maxNumOfClients);
    std::string acceptClient(uint timeout);
    void subscribe(const server_observer_t & observer);
    pipe_ret_t sendToAllClients(const char * msg, size_t size);
    pipe_ret_t sendToClient(const std::string & clientIP, const char * msg, size_t size);
    pipe_ret_t close();
    void printClients();
};

