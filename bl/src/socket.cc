#include "bl/socket.h"
#include "common.h"

#include <cstring>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace bl {

Socket::Socket(int port) {
  _port = port;
  _socket_fd = -1;
}

Socket::~Socket() {
  if (_socket_fd >= 0)
    close(_socket_fd);
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
  unsigned int sin_size = sizeof(client_addr);
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

    nBytesThisTime = send(s, tmp, nBytesOneTime, MSG_NOSIGNAL);  // or 0
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
  struct hostent *he;
  struct sockaddr_in server;

  if ((he = gethostbyname(szIp)) == NULL) {
    LOG_ERR("gethostbyname() error");
    return -1;
  }

  if (type == UDP)
    s_type = SOCK_DGRAM;
  else if (type == TCP)
    s_type = SOCK_STREAM;
  else {
    LOG_ERR("Socket type error.");
    return -1;
  }

  if ((_socket_fd = socket(AF_INET, s_type, 0)) == -1) {
    LOG_ERR("socket() error: {}", strerror(errno));
    //perror("socket");
    return -1;
  }

  if (type == TCP) {
    int syncnt = 3;
    ::setsockopt(_socket_fd, IPPROTO_TCP, TCP_SYNCNT, &syncnt, sizeof(syncnt));
  }

  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr = *((struct in_addr *)he->h_addr);

  if (connect(_socket_fd, (struct sockaddr *)&server, sizeof(server)) ==
      -1) {
    LOG_ERR("connect() error: {}. ip:{}, port:{}", strerror(errno), szIp, port);
    //perror("connect");
    return -1;
  }

  LOG_INFO("Connect to {}:{} succeed.", inet_ntoa(server.sin_addr), port);

  _port = port;
  return _socket_fd;
}

int Socket::Connect(const char* szIp, int port, int iTimeOut, socket_type type) {
  int s_type;
  int flags;
  int n;
  struct hostent *he;
  struct sockaddr_in server;

  if ((he = gethostbyname(szIp)) == NULL) {
    LOG_ERR("gethostbyname() error");
    return -1;
  }

  if (type == UDP)
    s_type = SOCK_DGRAM;
  else if (type == TCP)
    s_type = SOCK_STREAM;
  else {
    LOG_ERR("Socket type error.");
    return -1;
  }

  if ((_socket_fd = socket(AF_INET, s_type, 0)) == -1) {
    LOG_ERR("socket() error: {}", strerror(errno));
    return -1;
  }

  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(port);
  server.sin_addr = *((struct in_addr *)he->h_addr);

  flags = fcntl(_socket_fd, F_GETFL, 0);
  fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK); // 设置为非阻塞

  n = connect(_socket_fd, (struct sockaddr *)&server, sizeof(server));
  if (n < 0) {
    struct timeval tv;
    fd_set wset;

    if ((errno != EINPROGRESS) && (errno != EWOULDBLOCK)) {
      LOG_ERR("Connect to {}:{} error: no EINPROGRESS or EWOULDBLOCK", inet_ntoa(server.sin_addr), port);
      return -1;
    }

    tv.tv_sec = iTimeOut / 1000;
    tv.tv_usec = (iTimeOut - tv.tv_sec * 1000) * 1000;

    FD_ZERO(&wset);
    FD_SET(_socket_fd, &wset); // 把socket添加到select中进行监听

    n = select(_socket_fd + 1, NULL, &wset, NULL, &tv);
    if (n < 0) { // 出错
      LOG_ERR("Connect to {}:{} error: select error", inet_ntoa(server.sin_addr), port);
      fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK); // 恢复为阻塞模式
      return -1;
    } else if (0 == n) {  // 超时
      LOG_ERR("Connect to {}:{} timeout!", inet_ntoa(server.sin_addr), port);
      fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK); // 恢复为阻塞模式
      return 0;
    }
  }

  fcntl(_socket_fd, F_SETFL, flags & ~O_NONBLOCK); // 恢复为阻塞模式

  int err = 0;
  socklen_t errlen = sizeof(err);
  if (getsockopt(_socket_fd, SOL_SOCKET, SO_ERROR, &err, &errlen) == -1) {
    LOG_ERR("getsockopt() error: {}", strerror(errno));
    return -1;
  }

  if (err != 0) {
    LOG_ERR("Connect: SOL_SOCKET -> SO_ERROR: {}", strerror(err));
    return -1;
  }

  LOG_INFO("Connect to {}:{} succeed.", inet_ntoa(server.sin_addr), port);

  _port = port;
  return _socket_fd;
}

int Socket::Close() {
  if (_socket_fd >= 0)
    close(_socket_fd);
  return 0;
}

}  // namespace bl