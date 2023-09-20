#ifndef _BL_SOCKET_H_
#define _BL_SOCKET_H_

#define NET_SUCCESS 0
#define NET_TIMEOUT int(0x80000001L)
#define NET_SELECT_ERR int(0x80000002L)
#define NET_RECV_ERR int(0x80000004L)
#define NET_CONNECT_DOWN int(0x80000008L)

namespace bl {

class Socket {
 public:
  explicit Socket(int port = 0);
  ~Socket();
  static bool is_init;
  typedef enum {
    TCP = 1,		/* Sequenced, reliable, connection-based byte streams.  */
    UDP = 2,		/* Connectionless, unreliable datagrams of fixed maximum length.  */
  } socket_type;

 public:
  int Listen(int backlog);
  int Accept();
  int Connect(const char* szIp, int port, socket_type type=TCP);
  int Connect(const char* szIp, int port, int iTimeOut, socket_type type=TCP);
  int Send(const unsigned char* buffer, int size, int iTimeOut=2000);
  int Recv(unsigned char* buf, unsigned int len, int iTimeOut,
           int* result);
  int Recv(unsigned char* buf, unsigned int len);
  int Bind(socket_type type);
  int SetSocketPort(int port);
  int SetSocketFd(int fd);
  int Close();
  int Recvfrom(unsigned char* Buf, int BufSize, char* ServerIp = nullptr, int* Port = nullptr);

 private:
  int _port;
  int _socket_fd;
};
}  // namespace bl
#endif