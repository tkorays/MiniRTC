/**
 * @file network_interface.cc
 * @brief MiniRTC network interface implementation
 */

#include "minirtc/transport/network_interface.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace minirtc {

// ============================================================================
// Platform Helpers
// ============================================================================

#ifdef _WIN32
using SocketType = SOCKET;
const SocketType kInvalidSocket = INVALID_SOCKET;
inline int CloseSocket(SocketType s) { return closesocket(s); }
inline int SetBlocking(SocketType s, bool blocking) {
  unsigned long mode = blocking ? 0 : 1;
  return ioctlsocket(s, FIONBIO, &mode);
}
inline int GetLastError() { return WSAGetLastError(); }
#else
using SocketType = int;
const SocketType kInvalidSocket = -1;
inline int CloseSocket(SocketType s) { return close(s); }
inline int SetBlocking(SocketType s, bool blocking) {
  int flags = fcntl(s, F_GETFL, 0);
  if (flags < 0) return -1;
  if (blocking) {
    flags &= ~O_NONBLOCK;
  } else {
    flags |= O_NONBLOCK;
  }
  return fcntl(s, F_SETFL, flags);
}
inline int GetLastError() { return errno; }
#endif

// ============================================================================
// NetworkInterface Implementation
// ============================================================================

NetworkInterface::NetworkInterface()
    : socket_fd_(kInvalidSocket),
      type_(NetworkInterfaceType::kUdpSocket) {
}

NetworkInterface::~NetworkInterface() {
  Close();
}

TransportError NetworkInterface::Create(NetworkInterfaceType type,
                                         const NetworkAddress& local_addr) {
  Close();

  type_ = type;
  local_addr_ = local_addr;

  int ip_protocol = 0;
  int socket_type = SOCK_DGRAM;

#ifdef _WIN32
  WSADATA wsa_data;
  static bool initialized = false;
  if (!initialized) {
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    initialized = true;
  }
#endif

  if (type == NetworkInterfaceType::kTcpSocket) {
    socket_type = SOCK_STREAM;
  }

  // Create socket
  if (local_addr.ip.find(':') != std::string::npos) {
    // IPv6
    socket_fd_ = socket(AF_INET6, socket_type, 0);
  } else {
    // IPv4
    socket_fd_ = socket(AF_INET, socket_type, 0);
  }

  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kSocketError;
  }

  // Set socket options
  SetOptions(options_);

  return TransportError::kOk;
}

void NetworkInterface::Close() {
  if (socket_fd_ != kInvalidSocket) {
    close(socket_fd_);
    socket_fd_ = kInvalidSocket;
  }
}

TransportError NetworkInterface::Bind(const NetworkAddress& addr) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  if (addr.ip.find(':') != std::string::npos) {
    // IPv6
    struct sockaddr_in6 saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(addr.port);

    if (addr.ip == "0.0.0.0" || addr.ip.empty()) {
      saddr.sin6_addr = in6addr_any;
    } else {
      inet_pton(AF_INET6, addr.ip.c_str(), &saddr.sin6_addr);
    }

    if (::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&saddr),
               sizeof(saddr)) < 0) {
      return TransportError::kSocketError;
    }
  } else {
    // IPv4
    struct sockaddr_in saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(addr.port);

    if (addr.ip == "0.0.0.0" || addr.ip.empty()) {
      saddr.sin_addr.s_addr = INADDR_ANY;
    } else {
      inet_pton(AF_INET, addr.ip.c_str(), &saddr.sin_addr);
    }

    if (::bind(socket_fd_, reinterpret_cast<struct sockaddr*>(&saddr),
               sizeof(saddr)) < 0) {
      return TransportError::kSocketError;
    }
  }

  local_addr_ = addr;
  return TransportError::kOk;
}

TransportError NetworkInterface::Connect(const NetworkAddress& addr) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  remote_addr_ = addr;

  if (addr.ip.find(':') != std::string::npos) {
    struct sockaddr_in6 saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(addr.port);
    inet_pton(AF_INET6, addr.ip.c_str(), &saddr.sin6_addr);

    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&saddr),
                  sizeof(saddr)) < 0) {
      return TransportError::kSocketError;
    }
  } else {
    struct sockaddr_in saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(addr.port);
    inet_pton(AF_INET, addr.ip.c_str(), &saddr.sin_addr);

    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&saddr),
                  sizeof(saddr)) < 0) {
      return TransportError::kSocketError;
    }
  }

  return TransportError::kOk;
}

TransportError NetworkInterface::Listen(int backlog) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  if (type_ != NetworkInterfaceType::kTcpSocket) {
    return TransportError::kNotSupported;
  }

  if (::listen(socket_fd_, backlog) < 0) {
    return TransportError::kSocketError;
  }

  return TransportError::kOk;
}

TransportError NetworkInterface::Accept(
    std::shared_ptr<INetworkInterface>* client_socket,
    NetworkAddress* client_addr) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  struct sockaddr_in caddr;
  socklen_t caddr_len = sizeof(caddr);

  SocketType client_fd = ::accept(socket_fd_,
                                   reinterpret_cast<struct sockaddr*>(&caddr),
                                   &caddr_len);

  if (client_fd == kInvalidSocket) {
    return TransportError::kSocketError;
  }

  auto client = CreateNetworkInterface();
  // Note: We'd need to set up the client socket properly
  // This is simplified

  char ip_str[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &caddr.sin_addr, ip_str, INET_ADDRSTRLEN);
  client_addr->ip = ip_str;
  client_addr->port = ntohs(caddr.sin_port);

  // TODO: Return proper client socket
  (void)client_socket;

  return TransportError::kOk;
}

TransportError NetworkInterface::SendTo(const uint8_t* data,
                                         size_t size,
                                         const NetworkAddress& to) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  int sent = 0;

  if (to.ip.find(':') != std::string::npos) {
    struct sockaddr_in6 saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(to.port);
    inet_pton(AF_INET6, to.ip.c_str(), &saddr.sin6_addr);

    sent = ::sendto(socket_fd_, reinterpret_cast<const char*>(data), size, 0,
                    reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr));
  } else {
    struct sockaddr_in saddr;
    std::memset(&saddr, 0, sizeof(saddr));
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(to.port);
    inet_pton(AF_INET, to.ip.c_str(), &saddr.sin_addr);

    sent = ::sendto(socket_fd_, reinterpret_cast<const char*>(data), size, 0,
                    reinterpret_cast<struct sockaddr*>(&saddr), sizeof(saddr));
  }

  if (sent < 0) {
    return TransportError::kSocketError;
  }

  return TransportError::kOk;
}

TransportError NetworkInterface::ReceiveFrom(uint8_t* buffer,
                                              size_t buffer_size,
                                              size_t* received,
                                              NetworkAddress* from) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);

#ifdef _WIN32
  int ret = recvfrom(socket_fd_, reinterpret_cast<char*>(buffer),
                     static_cast<int>(buffer_size), 0,
                     reinterpret_cast<struct sockaddr*>(&saddr), &saddr_len);
#else
  ssize_t ret = recvfrom(socket_fd_, buffer, buffer_size, 0,
                         reinterpret_cast<struct sockaddr*>(&saddr), &saddr_len);
#endif

  if (ret < 0) {
    int err = GetLastError();
#ifdef _WIN32
    if (err == WSAEWOULDBLOCK) {
      return TransportError::kTimeout;
    }
#else
    if (err == EAGAIN || err == EWOULDBLOCK) {
      return TransportError::kTimeout;
    }
#endif
    return TransportError::kSocketError;
  }

  *received = static_cast<size_t>(ret);

  if (from) {
    char ip_str[INET6_ADDRSTRLEN];
    if (saddr.ss_family == AF_INET) {
      struct sockaddr_in* sin = reinterpret_cast<struct sockaddr_in*>(&saddr);
      inet_ntop(AF_INET, &sin->sin_addr, ip_str, INET_ADDRSTRLEN);
      from->port = ntohs(sin->sin_port);
    } else if (saddr.ss_family == AF_INET6) {
      struct sockaddr_in6* sin6 = reinterpret_cast<struct sockaddr_in6*>(&saddr);
      inet_ntop(AF_INET6, &sin6->sin6_addr, ip_str, INET6_ADDRSTRLEN);
      from->port = ntohs(sin6->sin6_port);
    }
    from->ip = ip_str;
  }

  return TransportError::kOk;
}

TransportError NetworkInterface::SetOptions(const SocketOptions& opts) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  options_ = opts;

  // Reuse address
  if (opts.reuse_addr) {
    int opt = 1;
#ifdef _WIN32
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
  }

  // Reuse port
  if (opts.reuse_port) {
    int opt = 1;
#ifdef _WIN32
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif
  }

  // Send buffer size
  if (opts.send_buffer_size > 0) {
    int opt = static_cast<int>(opts.send_buffer_size);
    setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
  }

  // Receive buffer size
  if (opts.recv_buffer_size > 0) {
    int opt = static_cast<int>(opts.recv_buffer_size);
    setsockopt(socket_fd_, SOL_SOCKET, SO_RCVBUF,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
  }

  // TTL
  if (opts.ttl > 0) {
    int opt = opts.ttl;
    setsockopt(socket_fd_, IPPROTO_IP, IP_TTL,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
  }

  // TOS
  if (opts.tos > 0) {
    int opt = opts.tos;
    setsockopt(socket_fd_, IPPROTO_IP, IP_TOS,
               reinterpret_cast<const char*>(&opt), sizeof(opt));
  }

  // Non-blocking
  if (opts.non_blocking) {
    SetBlocking(socket_fd_, false);
  }

  return TransportError::kOk;
}

NetworkAddress NetworkInterface::GetLocalAddress() const {
  if (socket_fd_ == kInvalidSocket) {
    return local_addr_;
  }

  struct sockaddr_in saddr;
  socklen_t saddr_len = sizeof(saddr);

  if (getsockname(socket_fd_, reinterpret_cast<struct sockaddr*>(&saddr),
                  &saddr_len) == 0) {
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &saddr.sin_addr, ip_str, INET_ADDRSTRLEN);
    NetworkAddress addr;
    addr.ip = ip_str;
    addr.port = ntohs(saddr.sin_port);
    return addr;
  }

  return local_addr_;
}

NetworkAddress NetworkInterface::GetRemoteAddress() const {
  return remote_addr_;
}

void NetworkInterface::SetCallback(INetworkCallback* callback) {
  callback_ = callback;
}

TransportError NetworkInterface::SetNonBlocking(bool enabled) {
  if (socket_fd_ == kInvalidSocket) {
    return TransportError::kNotInitialized;
  }

  return SetBlocking(socket_fd_, !enabled) == 0
      ? TransportError::kOk
      : TransportError::kSocketError;
}

int NetworkInterface::GetSocketFd() const {
  return static_cast<int>(socket_fd_);
}

NetworkInterfaceType NetworkInterface::GetType() const {
  return type_;
}

bool NetworkInterface::IsValid() const {
  return socket_fd_ != kInvalidSocket;
}

// ============================================================================
// Factory
// ============================================================================

std::shared_ptr<INetworkInterface> CreateNetworkInterface() {
  return std::make_shared<NetworkInterface>();
}

}  // namespace minirtc
