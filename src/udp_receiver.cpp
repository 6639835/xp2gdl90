#include "xp2gdl90/udp_receiver.h"

#include <array>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#ifdef InetPton
#undef InetPton
#endif
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace udp {

namespace {

#ifdef _WIN32
std::string SocketErrorMessage(const char* prefix, int code) {
  return std::string(prefix) + std::to_string(code);
}
#else
std::string SocketErrorMessage(const char* prefix, int code) {
  return std::string(prefix) + std::string(strerror(code));
}
#endif

}  // namespace

UDPReceiver::UDPReceiver(uint16_t listen_port)
    : listen_port_(listen_port),
      initialized_(false),
      last_error_(),
      socket_(kInvalidSocket)
#ifdef _WIN32
      ,
      wsa_initialized_(false)
#endif
{
}

UDPReceiver::~UDPReceiver() {
  close();
}

bool UDPReceiver::initialize() {
  if (initialized_) {
    return true;
  }

#ifdef _WIN32
  WSADATA wsa_data;
  const int startup_result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    last_error_ = SocketErrorMessage("WSAStartup failed: ", startup_result);
    return false;
  }
  wsa_initialized_ = true;
  socket_ = static_cast<uintptr_t>(::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP));
  if (socket_ == static_cast<uintptr_t>(INVALID_SOCKET)) {
    last_error_ = SocketErrorMessage("Socket creation failed: ",
                                     WSAGetLastError());
    WSACleanup();
    wsa_initialized_ = false;
    socket_ = kInvalidSocket;
    return false;
  }
#else
  socket_ = static_cast<uintptr_t>(::socket(AF_INET, SOCK_DGRAM, 0));
  if (static_cast<int>(socket_) < 0) {
    last_error_ =
        SocketErrorMessage("Socket creation failed: ", errno);
    socket_ = kInvalidSocket;
    return false;
  }
#endif

  int reuse = 1;
#ifdef _WIN32
  const int set_reuse = ::setsockopt(static_cast<SOCKET>(socket_), SOL_SOCKET,
                                     SO_REUSEADDR,
                                     reinterpret_cast<const char*>(&reuse),
                                     static_cast<int>(sizeof(reuse)));
  if (set_reuse == SOCKET_ERROR) {
    last_error_ = SocketErrorMessage("Failed to set SO_REUSEADDR: ",
                                     WSAGetLastError());
    close();
    return false;
  }
#else
  if (::setsockopt(static_cast<int>(socket_), SOL_SOCKET, SO_REUSEADDR, &reuse,
                   static_cast<socklen_t>(sizeof(reuse))) < 0) {
    last_error_ =
        SocketErrorMessage("Failed to set SO_REUSEADDR: ", errno);
    close();
    return false;
  }
#endif

  sockaddr_in bind_addr{};
  bind_addr.sin_family = AF_INET;
  bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  bind_addr.sin_port = htons(listen_port_);

#ifdef _WIN32
  if (::bind(static_cast<SOCKET>(socket_),
             reinterpret_cast<const sockaddr*>(&bind_addr),
             static_cast<int>(sizeof(bind_addr))) == SOCKET_ERROR) {
    last_error_ = SocketErrorMessage("bind failed: ", WSAGetLastError());
    close();
    return false;
  }

  u_long non_blocking = 1;
  if (::ioctlsocket(static_cast<SOCKET>(socket_), FIONBIO,
                    &non_blocking) == SOCKET_ERROR) {
    last_error_ =
        SocketErrorMessage("ioctlsocket failed: ", WSAGetLastError());
    close();
    return false;
  }
#else
  if (::bind(static_cast<int>(socket_),
             reinterpret_cast<const sockaddr*>(&bind_addr),
             static_cast<socklen_t>(sizeof(bind_addr))) < 0) {
    last_error_ = SocketErrorMessage("bind failed: ", errno);
    close();
    return false;
  }

  const int flags = ::fcntl(static_cast<int>(socket_), F_GETFL, 0);
  if (flags < 0 ||
      ::fcntl(static_cast<int>(socket_), F_SETFL, flags | O_NONBLOCK) < 0) {
    last_error_ = SocketErrorMessage("fcntl failed: ", errno);
    close();
    return false;
  }
#endif

  initialized_ = true;
  last_error_.clear();
  return true;
}

int UDPReceiver::receive(std::vector<uint8_t>* out_data,
                         std::string* out_source_ip,
                         uint16_t* out_source_port) {
  if (!initialized_) {
    last_error_ = "Socket not initialized";
    return -1;
  }
  if (!out_data) {
    last_error_ = "Output buffer is required";
    return -1;
  }

  std::array<uint8_t, 2048> buffer{};
  sockaddr_in source_addr{};
#ifdef _WIN32
  int source_len = static_cast<int>(sizeof(source_addr));
  const int received =
      ::recvfrom(static_cast<SOCKET>(socket_),
                 reinterpret_cast<char*>(buffer.data()),
                 static_cast<int>(buffer.size()), 0,
                 reinterpret_cast<sockaddr*>(&source_addr), &source_len);
  if (received == SOCKET_ERROR) {
    const int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) {
      last_error_.clear();
      return 0;
    }
    last_error_ = SocketErrorMessage("recvfrom failed: ", err);
    return -1;
  }
#else
  socklen_t source_len = static_cast<socklen_t>(sizeof(source_addr));
  const ssize_t received =
      ::recvfrom(static_cast<int>(socket_), buffer.data(), buffer.size(), 0,
                 reinterpret_cast<sockaddr*>(&source_addr), &source_len);
  if (received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      last_error_.clear();
      return 0;
    }
    last_error_ = SocketErrorMessage("recvfrom failed: ", errno);
    return -1;
  }
#endif

  out_data->assign(buffer.begin(), buffer.begin() + received);

  if (out_source_ip) {
    char ip_buffer[INET_ADDRSTRLEN] = {};
#ifdef _WIN32
    if (::InetNtopA(AF_INET, &source_addr.sin_addr, ip_buffer,
                    static_cast<DWORD>(sizeof(ip_buffer))) != nullptr) {
      *out_source_ip = ip_buffer;
    } else {
      out_source_ip->clear();
    }
#else
    if (::inet_ntop(AF_INET, &source_addr.sin_addr, ip_buffer,
                    sizeof(ip_buffer)) != nullptr) {
      *out_source_ip = ip_buffer;
    } else {
      out_source_ip->clear();
    }
#endif
  }

  if (out_source_port) {
    *out_source_port = ntohs(source_addr.sin_port);
  }

  last_error_.clear();
  return static_cast<int>(received);
}

void UDPReceiver::close() {
  if (!initialized_ && socket_ == kInvalidSocket) {
    return;
  }

  if (socket_ != kInvalidSocket) {
#ifdef _WIN32
    ::closesocket(static_cast<SOCKET>(socket_));
#else
    ::close(static_cast<int>(socket_));
#endif
    socket_ = kInvalidSocket;
  }

#ifdef _WIN32
  if (wsa_initialized_) {
    WSACleanup();
    wsa_initialized_ = false;
  }
#endif

  initialized_ = false;
}

}  // namespace udp
