
#include <functional>
#include <thread>
#include <algorithm>
#include <chrono>
#include "../include/tcp_server.h"
#include "../include/common.h"


TcpServer::TcpServer() {
    _subscribers.reserve(10);
    _clients.reserve(10);
    _stopRemoveClientsTask = false;
#ifdef _WIN32
    WSADATA wsaData;
    (void)WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

TcpServer::~TcpServer() {
    close();
#ifdef _WIN32
    WSACleanup();
#endif
}

void TcpServer::subscribe(const server_observer_t & observer) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);
    _subscribers.push_back(observer);
}

void TcpServer::printClients() {
    std::lock_guard<std::mutex> lock(_clientsMtx);
    if (_clients.empty()) {
        std::cout << "no connected clients\n";
    }
    for (const Client *client : _clients) {
        client->print();
    }
}

/**
 * Remove dead clients (disconnected) from clients vector periodically
 */
void TcpServer::removeDeadClients() {
    std::vector<Client*>::const_iterator clientToRemove;
    while (!_stopRemoveClientsTask) {
        {
          std::unique_lock<std::mutex> lock(_clientsMtx);
            do {
                clientToRemove = std::find_if(_clients.begin(), _clients.end(),
                                              [](Client *client) { return !client->isConnected(); });

                if (clientToRemove != _clients.end()) {
                    try {
                        (*clientToRemove)->close();
                    } catch (const std::runtime_error &error) {
                        std::cout << "removeDeadClients error: " << error.what() << "\n";
                    }
                    delete *clientToRemove;
                    clientToRemove = _clients.erase(clientToRemove); // point to next element
                }
            } while (clientToRemove != _clients.end());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
}

void TcpServer::terminateDeadClientsRemover() {
    if (_clientsRemoverThread) {
        _stopRemoveClientsTask = true;
        _clientsRemoverThread->join();
        delete _clientsRemoverThread;
        _clientsRemoverThread = nullptr;
    }
}

/**
 * Handle different client events. Subscriber callbacks should be short and fast, and must not
 * call other server functions to avoid deadlock
 */
void TcpServer::clientEventHandler(const Client &client, ClientEvent event, const char * msg, size_t size) {
    switch (event) {
        case ClientEvent::DISCONNECTED: {
            publishClientDisconnected(client.getIp(), msg);
            break;
        }
        case ClientEvent::INCOMING_MSG: {
            publishClientMsg(client, msg, size);
            break;
        }
    }
}

/*
 * Publish incomingPacketHandler client message to observer.
 * Observers get only messages that originated
 * from clients with IP address identical to
 * the specific observer requested IP
 */
void TcpServer::publishClientMsg(const Client & client, const char * msg, size_t msgSize) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);

    for (const server_observer_t& subscriber : _subscribers) {
        if (subscriber.wantedIP == client.getIp() || subscriber.wantedIP.empty()) {
            if (subscriber.incomingPacketHandler) {
                subscriber.incomingPacketHandler(client.getIp(), msg, msgSize);
            }
        }
    }
}

/*
 * Publish client disconnection to observer.
 * Observers get only notify about clients
 * with IP address identical to the specific
 * observer requested IP
 */
void TcpServer::publishClientDisconnected(const std::string &clientIP, const std::string &clientMsg) {
    std::lock_guard<std::mutex> lock(_subscribersMtx);

    for (const server_observer_t& subscriber : _subscribers) {
        if (subscriber.wantedIP == clientIP) {
            if (subscriber.disconnectionHandler) {
                subscriber.disconnectionHandler(clientIP, clientMsg);
            }
        }
    }
}

/*
 * Bind port and start listening
 * Return tcp_ret_t
 */
pipe_ret_t TcpServer::start(int port, int maxNumOfClients, bool removeDeadClientsAutomatically) {
    if (removeDeadClientsAutomatically) {
        _clientsRemoverThread = new std::thread(&TcpServer::removeDeadClients, this);
    }
    try {
        initializeSocket();
        bindAddress(port);
        listenToClients(maxNumOfClients);
        _enableSend = true;
    } catch (const std::runtime_error &error) {
        return pipe_ret_t::failure(error.what());
    }
    return pipe_ret_t::success();
}

void TcpServer::initializeSocket() {
    _sockfd.set(socket(AF_INET, SOCK_STREAM, 0));
    const bool socketFailed = (_sockfd.get() == -1);
    if (socketFailed) {
#ifdef _WIN32
        int last_error = WSAGetLastError();
        throw std::runtime_error("Windows socket failed");
#else
        throw std::runtime_error(strerror(errno));
#endif
    }

    // set socket for reuse (otherwise might have to wait 4 minutes every time socket is closed)
    const int option = 1;
    setsockopt(_sockfd.get(), SOL_SOCKET, SO_REUSEADDR, (char *)&option, sizeof(option));
}

void TcpServer::bindAddress(int port) {
    memset(&_serverAddress, 0, sizeof(_serverAddress));
    _serverAddress.sin_family = AF_INET;
    _serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    _serverAddress.sin_port = htons(port);

    const int bindResult = ::bind(_sockfd.get(), (struct sockaddr *)&_serverAddress, sizeof(_serverAddress));
    const bool bindFailed = (bindResult == -1);
    if (bindFailed) {
#ifdef _WIN32
        int last_error = WSAGetLastError();
        throw std::runtime_error("Windows bind failed");
#else
        throw std::runtime_error(strerror(errno));
#endif
    }
}

void TcpServer::listenToClients(int maxNumOfClients) {
    const int clientsQueueSize = maxNumOfClients;
    const bool listenFailed = (listen(_sockfd.get(), clientsQueueSize) == -1);
    if (listenFailed) {
#ifdef _WIN32
        int last_error = WSAGetLastError();
        throw std::runtime_error("Windows listen failed");
#else
        throw std::runtime_error(strerror(errno));
#endif
    }
}

/*
 * Accept and handle new client socket. To handle multiple clients, user must
 * call this function in a loop to enable the acceptance of more than one.
 * If timeout argument equal 0, this function is executed in blocking mode.
 * If timeout argument is > 0 then this function is executed in non-blocking
 * mode (async) and will quit after timeout seconds if no client tried to connect.
 * Return accepted client IP, or throw error if failed
 */
std::string TcpServer::acceptClient(uint timeout) {
    const pipe_ret_t waitingForClient = waitForClient(timeout);
    if (!waitingForClient.isSuccessful()) {
        throw std::runtime_error(waitingForClient.message());
    }

    socklen_t socketSize  = sizeof(_clientAddress);
    const int fileDescriptor = accept(_sockfd.get(), (struct sockaddr*)&_clientAddress, &socketSize);

    const bool acceptFailed = (fileDescriptor == -1);
    if (acceptFailed) {
#ifdef _WIN32
        int last_error = WSAGetLastError();
        throw std::runtime_error("Windows accept failed");
#else
        throw std::runtime_error(strerror(errno));
#endif
    }

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(fileDescriptor, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(struct timeval));
    setsockopt(fileDescriptor, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(struct timeval));

    auto newClient = new Client(fileDescriptor);
    newClient->setIp(inet_ntoa(_clientAddress.sin_addr));
    using namespace std::placeholders;
    newClient->setEventsHandler(std::bind(&TcpServer::clientEventHandler, this, _1, _2, _3, _4));
    newClient->startListen();

    std::lock_guard<std::mutex> lock(_clientsMtx);
    _clients.push_back(newClient);

    return newClient->getIp();
}

pipe_ret_t TcpServer::waitForClient(uint32_t timeout) {
    if (timeout > 0) {
        const fd_wait::Result waitResult = fd_wait::waitFor(_sockfd, timeout);
        const bool noIncomingClient = (!FD_ISSET(_sockfd.get(), &_fds));

        if (waitResult == fd_wait::Result::FAILURE) {
            return pipe_ret_t::failure(strerror(errno));
        } else if (waitResult == fd_wait::Result::TIMEOUT) {
            return pipe_ret_t::failure("Timeout waiting for client");
        } else if (noIncomingClient) {
            return pipe_ret_t::failure("File descriptor is not set");
        }
    }

    return pipe_ret_t::success();
}

/*
 * Send message to all connected clients.
 * Return true if message was sent successfully to all clients
 */
pipe_ret_t TcpServer::sendToAllClients(const char * msg, size_t size) {
    if (!_enableSend) {
        return pipe_ret_t::failure("server not start");
    }

    std::lock_guard<std::mutex> lock(_clientsMtx);

    for (Client *client : _clients) {
        auto beforeTime = std::chrono::steady_clock::now();
        pipe_ret_t sendingResult = sendToClient(*client, msg, size);
        auto afterTime = std::chrono::steady_clock::now();
        double duration_millsecond = std::chrono::duration<double, std::milli>(afterTime - beforeTime).count();
        if (duration_millsecond > 3000) {
            std::cout << "sendToClient failed: Timeout!"
                      << " IP=" << client->getIp() << std::endl;
            client->setConnected(false);
        }
        if (!sendingResult.isSuccessful()) {
            std::cout << "sendToClient failed: " << sendingResult.message() << " IP=" << client->getIp() << std::endl;
            client->setConnected(false);
            // return sendingResult;
        }
    }

    return pipe_ret_t::success();
}

/*
 * Send message to specific client (determined by client IP address).
 * Return true if message was sent successfully
 */
pipe_ret_t TcpServer::sendToClient(const Client & client, const char * msg, size_t size){
    try{
        client.send(msg, size);
    } catch (const std::runtime_error &error) {
        return pipe_ret_t::failure(error.what());
    }

    return pipe_ret_t::success();
}

pipe_ret_t TcpServer::sendToClient(const std::string & clientIP, const char * msg, size_t size) {
    if (!_enableSend) {
        return pipe_ret_t::failure("server not start");
    }
    
    std::lock_guard<std::mutex> lock(_clientsMtx);

    const auto clientIter = std::find_if(_clients.begin(), _clients.end(),
         [&clientIP](Client *client) { return client->getIp() == clientIP; });

    if (clientIter == _clients.end()) {
        return pipe_ret_t::failure("client not found");
    }

    const Client &client = *(*clientIter);
    return sendToClient(client, msg, size);
}

/*
 * Close server and clients resources.
 * Return true is successFlag, false otherwise
 */
pipe_ret_t TcpServer::close() {
    _enableSend = false;
    terminateDeadClientsRemover();
    { // close clients
        std::unique_lock<std::mutex> lock(_clientsMtx);

        for (Client * client : _clients) {
            try {
                client->close();
            } catch (const std::runtime_error& error) {
                lock.unlock();
                return pipe_ret_t::failure(error.what());
            }
        }
        _clients.clear();
    }

    if (_sockfd.get() != -1) {  // close server
#ifdef _WIN32
        const int closeServerResult = closesocket(_sockfd.get());
#else
        const int closeServerResult = ::close(_sockfd.get());
#endif
        const bool closeServerFailed = (closeServerResult == -1);
        if (closeServerFailed) {
#ifdef _WIN32
            int last_error = WSAGetLastError();
            throw pipe_ret_t::failure("Windows closesocket failed");
#else
            return pipe_ret_t::failure(strerror(errno));
#endif
        } else {
            _sockfd.set(-1);
        }
    }

    return pipe_ret_t::success();
}
