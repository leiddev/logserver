#include "bl/socket.h"
#include "common.h"

#include <cstring>
#include <fcntl.h>
#include <stdio.h>

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL (0)
#endif

namespace bl {

Socket::Socket(int port) {
  _port = port;
  _socket_fd = -1;
#ifdef _WIN32
  WSADATA wsaData;
  (void)WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

Socket::~Socket() {
  Close();
#ifdef _WIN32
  WSACleanup();
#endif
}

int Socket::SetSocketPort(int port) {
  _port = port;
  return 0;
}

int Socket::SetSocketFd(int fd) {
  _socket_fd = fd;
  return 0;
}

int Socket::Bind(socket_type type) {
  struct sockaddr_in addr;
  int addr_len = sizeof(addr);

  if (type == UDP)
    _socket_fd = socket(PF_INET, SOCK_DGRAM, 0);
  else if (type == TCP)
    _socket_fd = socket(PF_INET, SOCK_STREAM, 0);
  else {
    LOG_ERR("Socket type error.");
    return -1;
  }

  if (_socket_fd == -1) {
    LOG_ERR("Create svr socket error.");
    return -1;
  }
  struct timeval tv;
  tv.tv_sec = 2;
  tv.tv_usec = 0;

  if (::setsockopt(_socket_fd, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,
                   sizeof(struct timeval)) != 0) {
    LOG_ERR("SO_SNDTIMEO setsockopt error: {}", strerror(errno));
    return -1;
  }

  if (::setsockopt(_socket_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,
                   sizeof(struct timeval)) != 0) {
    LOG_ERR("SO_RCVTIMEO setsockopt error: {}", strerror(errno));
    return -1;
  }

  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(_port);
  addr.sin_addr.s_addr = INADDR_ANY;

  if (-1 == bind(_socket_fd, (struct sockaddr *)&addr, addr_len)) {
    //perror("bind");
    LOG_ERR("bind() error: {}.", strerror(errno));
    return -1;
  }
  return 0;
}

int Socket::Listen(int backlog) {
  if(listen(_socket_fd, backlog) == -1) {
    //perror("listen");
    LOG_ERR("Socket listen error: {}", strerror(errno));
    return -1;
  }
  return 0;
}

int Socket::Accept() {
  struct sockaddr_in client_addr;
  socklen_t sin_size = sizeof(client_addr);
  int client_fd;

  if((client_fd = accept(_socket_fd, (sockaddr*)(&client_addr), &sin_size)) == -1 ) {
    //perror("accept");
    LOG_ERR("Socket accept failed.");
    return -1;
  }

  LOG_INFO("Received a connection from {}:{}.",
    static_cast<char*>(inet_ntoa(client_addr.sin_addr)), ntohs(client_addr.sin_port));

  return client_fd;
}

int Socket::Send(const unsigned char *buffer, int size, int iTimeOut) {
#define SENDSIZE 2 * 1024 * 1024
  int nBytesSent = 0;
  int nBytesThisTime;
  int nBytesOneTime = SENDSIZE;  //
  const unsigned char *tmp = buffer;
  int iRetry = 0;
  int nLeft;
  fd_set write_set, except_set;
  struct timeval tv;
  int iRet = -1;
  int timeout_ms = iTimeOut;
  tmp = buffer;
  int s = _socket_fd;
#if 0
    LOG_DEBUG("socket: %08x, buf: %p, len: %d\n", s, tmp, size);
	LOG_DEBUG("[0x%x 0x%x 0x%x 0x%x] [0x%x 0x%x 0x%x 0x%x] [0x%x 0x%x 0x%x 0x%x]\n"
		,buffer[0]
		,buffer[1]
		,buffer[2]
		,buffer[3]
		,buffer[4]
		,buffer[5]
		,buffer[6]
		,buffer[7]
		,buffer[8]
		,buffer[9]
		,buffer[10]
		,buffer[11]
		);
#endif
  // dump_mem((unsigned char *)tmp, size > 32 ? 32 : size);
  do {
    if (size < SENDSIZE) {
      nBytesOneTime = size - nBytesSent;
    }

    FD_ZERO(&write_set);
    FD_ZERO(&except_set);
    FD_SET(s, &write_set);
    FD_SET(s, &except_set);

    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    /* (int)(s+1): make M$ compiler happy */
    iRet = select((int)(s + 1), NULL, &write_set, &except_set, &tv);
    if (iRet == 0) {
      LOG_ERR("Send select timeout.");
      return -1;
    } else if (iRet == -1) {
      LOG_ERR("Send select error.");
      return -1;
    } else {
      if (FD_ISSET(s, &except_set))
      {
        LOG_ERR("exception occur.");
        return -1;
      } else if (!FD_ISSET(s, &write_set))
      {
        LOG_ERR("ready num is {}, but cannot find fd any where.", iRet);
        return -1;
      }
    }

    nBytesThisTime = send(s, (char *)tmp, nBytesOneTime, MSG_NOSIGNAL);  // or 0
    // LOG_INFO("nBytesThisTime {}.", nBytesThisTime);
    if (nBytesThisTime < 0) {  //
      iRetry++;
      if (5 == iRetry) {
        // LOG_ERR("{} > 5", nBytesThisTime);
        return -2;
      }
      continue;
    }

    nBytesSent += nBytesThisTime;
    tmp += nBytesThisTime;

    nLeft = size - nBytesSent;
    if (nLeft < nBytesOneTime) {
      nBytesOneTime = nLeft;
    }
  } while (nBytesSent < size);

  return nBytesSent;
}

int Socket::Recv(unsigned char *buf, unsigned int len) {
  return recv(_socket_fd, (char *)buf, len, 0);
}

int Socket::Recv(unsigned char *buf, unsigned int len, int iTimeOut,
                 int *result) {
  unsigned char *pTemp = buf;
  int iRst = len;
  int iRet = 0;
  fd_set read_set, except_set;
  struct timeval tv;
  int iErr = NET_SUCCESS;

  if (!result)
    return -1;

  int s = _socket_fd;

  while (len > 0) {
    iErr = NET_SUCCESS;
    FD_ZERO(&read_set);
    FD_ZERO(&except_set);
    FD_SET(s, &read_set);
    FD_SET(s, &except_set);

    tv.tv_sec = iTimeOut / 1000;
    tv.tv_usec = (iTimeOut % 1000) * 1000;
    if (iTimeOut < 0)
      iRet = select((int)(s + 1), &read_set, NULL, &except_set, NULL);
    else
      iRet = select((int)(s + 1), &read_set, NULL, &except_set, &tv);

    if (iRet == 0) {
      iErr = NET_TIMEOUT;
    } else if (iRet < 0) {
      // printf("select error, code = {}.", WSAGetLastError());
      iErr = NET_SELECT_ERR;
    } else {
      if (FD_ISSET(s, &except_set))
      {
        // printf("exception occur.");
        iErr = NET_SELECT_ERR;
      } else if (!FD_ISSET(s, &read_set))
      {
        // printf("ready num is %d, but cannot find fd any where.", iRet);
        iErr = NET_SELECT_ERR;
      }
    }

    if (NET_SELECT_ERR == iErr) {
      LOG_INFO("Selct error");
      break;
    }

    if (NET_TIMEOUT == iErr) {
      // LOG_INFO("Selct Timeout");
      break;
    }

    int iRet = recv(s, (char *)pTemp, len, 0);

    if (iRet < 0) {
      iErr = NET_RECV_ERR;
      LOG_INFO("recv error");
      break;
    }
    if (iRet == 0) {
      iErr = NET_CONNECT_DOWN;
      LOG_INFO("Connect closed");
      break;
    } else if (iRet > 0) {
      pTemp += iRet;
      len -= iRet;
    }
  }

  *result = iErr;

  return (iRst - len);
}

int Socket::Connect(const char *szIp, int port, socket_type type) {
  int s_type;
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *p;
  struct sockaddr addr;
  socklen_t addr_len = 0;
  int status;
  char addrstr[INET6_ADDRSTRLEN];

  if (type == UDP)
    s_type = SOCK_DGRAM;
  else if (type == TCP)
    s_type = SOCK_STREAM;
  else {
    LOG_ERR("Socket type error.");
    return -1;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;  // IPv4
  hints.ai_socktype = s_type;
  hints.ai_flags = AI_NUMERICSERV;  // port passed as as numeric value
  hints.ai_protocol = 0;

  auto port_str = std::to_string(port);
  if ((status = getaddrinfo(szIp, port_str.c_str(), &hints, &res)) != 0) {
    LOG_ERR("getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  for (p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) {  // IPv4
      if (p->ai_addr) {
        addr = *p->ai_addr;
        addr_len = p->ai_addrlen;
        break;
      }
    }
  }
  freeaddrinfo(res);  // free the linked list

  struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
  inet_ntop(AF_INET, &(addr_in->sin_addr), addrstr, sizeof(addrstr));

  if ((_socket_fd = socket(AF_INET, s_type, 0)) == -1) {
    LOG_ERR("socket() error: {}", strerror(errno));
    //perror("socket");
    return -1;
  }

  #ifdef TCP_SYNCNT
  if (type == TCP) {
    int syncnt = 3;
    ::setsockopt(_socket_fd, IPPROTO_TCP, TCP_SYNCNT, &syncnt, sizeof(syncnt));
  }
  #endif 

  if (connect(_socket_fd, &addr, addr_len) ==
      -1) {
    LOG_ERR("connect() error: {}. ip:{}, port:{}", strerror(errno), szIp, port);
    //perror("connect");
    return -1;
  }

  LOG_INFO("Connect to {}:{} succeed.", addrstr, port);

  _port = port;
  return _socket_fd;
}

int Socket::Connect(const char* szIp, int port, int iTimeOut, socket_type type) {
  int s_type;
  int n;
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *p;
  struct sockaddr addr;
  socklen_t addr_len = 0;
  int status;
  char addrstr[INET6_ADDRSTRLEN];

  if (type == UDP)
    s_type = SOCK_DGRAM;
  else if (type == TCP)
    s_type = SOCK_STREAM;
  else {
    LOG_ERR("Socket type error.");
    return -1;
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv4
  hints.ai_socktype = s_type;
  hints.ai_flags = AI_NUMERICSERV; // port passed as as numeric value
  hints.ai_protocol = 0;

  auto port_str = std::to_string(port);
  if ((status = getaddrinfo(szIp, port_str.c_str(), &hints, &res)) != 0) {
    LOG_ERR("getaddrinfo error: %s\n", gai_strerror(status));
    return -1;
  }

  for(p = res; p != NULL; p = p->ai_next) {
    if (p->ai_family == AF_INET) { // IPv4
      if (p->ai_addr) {
        addr = *p->ai_addr;
        addr_len = p->ai_addrlen;
        break;
      }
    }
  }
  freeaddrinfo(res); // free the linked list

  struct sockaddr_in *addr_in = (struct sockaddr_in *)&addr;
  inet_ntop(AF_INET, &(addr_in->sin_addr), addrstr, sizeof(addrstr));

  if ((_socket_fd = socket(AF_INET, s_type, 0)) == -1) {
    LOG_ERR("socket() error: {}", strerror(errno));
    return -1;
  }

   // Change to Non-Blocking Mode
#ifdef _WIN32
  unsigned long on = 1;
  unsigned long off = 0;
  ioctlsocket(_socket_fd, FIONBIO, &on);
#else
  int flags;
  flags = fcntl(_socket_fd, F_GETFL, 0);
  fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK);
#endif

  n = connect(_socket_fd, &addr, addr_len);
  if (n < 0) {
    struct timeval tv;
    fd_set wset;

#ifndef _WIN32
    if ((errno != EINPROGRESS) && (errno != EWOULDBLOCK)) {
      LOG_ERR("Connect to {}:{} error: no EINPROGRESS or EWOULDBLOCK", addrstr, port);
      return -1;
    }
#endif

    tv.tv_sec = iTimeOut / 1000;
    tv.tv_usec = (iTimeOut - tv.tv_sec * 1000) * 1000;

    FD_ZERO(&wset);
    FD_SET(_socket_fd, &wset); // add socket to select

    n = select(_socket_fd + 1, NULL, &wset, NULL, &tv);
    if (n < 0) { // error
      LOG_ERR("Connect to {}:{} error: select error", addrstr, port);

      // Change to Blocking Mode
#ifdef _WIN32
      ioctlsocket(_socket_fd, FIONBIO, &off);
#else
      fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
      return -1;
    } else if (0 == n) {  // error
      LOG_ERR("Connect to {}:{} timeout!", addrstr, port);

      // Change to Blocking Mode
#ifdef _WIN32
      ioctlsocket(_socket_fd, FIONBIO, &off);
#else
      fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK);
#endif
      return 0;
    }
  }

  // Change to Blocking Mode
#ifdef _WIN32
  ioctlsocket(_socket_fd, FIONBIO, &off);
#else
  fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK);
#endif

  int err = 0;
  socklen_t errlen = sizeof(err);
  if (getsockopt(_socket_fd, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen) == -1) {
    LOG_ERR("getsockopt() error: {}", strerror(errno));
    return -1;
  }

  if (err != 0) {
    LOG_ERR("Connect: SOL_SOCKET -> SO_ERROR: {}", strerror(err));
    return -1;
  }

  LOG_INFO("Connect to {}:{} succeed.", addrstr, port);

  _port = port;
  return _socket_fd;
}

int Socket::Close() {
#ifdef _WIN32
  if (_socket_fd > 0) {
    if (SOCKET_ERROR == closesocket(_socket_fd)) {
      int last_error = WSAGetLastError();
      LOG_ERR("Windows closesocket failed: {}", last_error);
      return -1;
    } else {
      _socket_fd = -1;
      return 0;
    }
  }
#else
  if (_socket_fd > 0) {
    if (0 != ::close(_socket_fd)) {
      LOG_ERR("Close socket failed: {}", strerror(errno));
      return -1;
    } else {
      _socket_fd = -1;
      return 0;
    }
  }
#endif
  return 0;
}

int Socket::Recvfrom(unsigned char *Buf, int BufSize, char *ServerIp, int *Port) {
  sockaddr_in Addr;
  socklen_t AddrLen;
  int RecvLen = 0;
  if (_socket_fd == -1) {
    LOG_ERR("socket no alive error.");
    return -1;
  }

  if (ServerIp && Port) {
    memset(&Addr, 0, sizeof(Addr));
    AddrLen = 20;
    RecvLen = recvfrom(_socket_fd, (char *)Buf, BufSize, 0, (sockaddr *)&Addr, &AddrLen);
    memcpy(ServerIp, inet_ntoa(Addr.sin_addr), std::strlen(inet_ntoa(Addr.sin_addr)));
    *Port = ntohs(Addr.sin_port);
    // printf("srcip = %s, AddrLen = %d \n", inet_ntoa(Addr.sin_addr), AddrLen);
  } else {
    RecvLen = recvfrom(_socket_fd, (char *)Buf, BufSize, 0, NULL, NULL);
  }
  return RecvLen;
}
}  // namespace bl