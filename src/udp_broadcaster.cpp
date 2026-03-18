#include "xp2gdl90/udp_broadcaster.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
// ws2tcpip.h may define InetPton as a macro (e.g. mapping to inet_pton/InetPtonA),
// which would break our SocketOps::InetPton virtual override (header is included
// before the macro exists). Ensure we keep the identifier stable.
#ifdef InetPton
#undef InetPton
#endif
#else
#include <arpa/inet.h>
#include <errno.h>
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

namespace detail {

class DefaultSocketOpsImpl final : public SocketOps {
 public:
  int Startup() override {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data);
#else
    return 0;
#endif
  }

  void Cleanup() override {
#ifdef _WIN32
    WSACleanup();
#endif
  }

  uintptr_t CreateSocket(int domain, int type, int protocol) override {
#ifdef _WIN32
    const SOCKET socket_handle = ::socket(domain, type, protocol);
    if (socket_handle == INVALID_SOCKET) {
      return UDPBroadcaster::kInvalidSocket;
    }
    return static_cast<uintptr_t>(socket_handle);
#else
    const int socket_handle = ::socket(domain, type, protocol);
    if (socket_handle < 0) {
      return UDPBroadcaster::kInvalidSocket;
    }
    return static_cast<uintptr_t>(socket_handle);
#endif
  }

  int SetSockOpt(uintptr_t socket_handle, int level, int optname,
                 const void* optval, size_t optlen) override {
#ifdef _WIN32
    const SOCKET socket_value = static_cast<SOCKET>(socket_handle);
    return ::setsockopt(socket_value, level, optname,
                        reinterpret_cast<const char*>(optval),
                        static_cast<int>(optlen));
#else
    const int socket_value = static_cast<int>(socket_handle);
    return ::setsockopt(socket_value, level, optname, optval,
                        static_cast<socklen_t>(optlen));
#endif
  }

  int InetPton(int af, const char* src, void* dst) override {
#ifdef _WIN32
    return ::InetPtonA(af, src, dst);
#else
    return ::inet_pton(af, src, dst);
#endif
  }

  intptr_t SendTo(uintptr_t socket_handle, const void* buf, size_t len,
                  int flags, const void* dest_addr,
                  size_t addrlen) override {
#ifdef _WIN32
    const SOCKET socket_value = static_cast<SOCKET>(socket_handle);
    const int sent = ::sendto(
        socket_value, reinterpret_cast<const char*>(buf),
        static_cast<int>(len), flags,
        reinterpret_cast<const sockaddr*>(dest_addr),
        static_cast<int>(addrlen));
    if (sent == SOCKET_ERROR) {
      return -1;
    }
    return static_cast<intptr_t>(sent);
#else
    const int socket_value = static_cast<int>(socket_handle);
    const ssize_t sent =
        ::sendto(socket_value, buf, len, flags,
                 reinterpret_cast<const sockaddr*>(dest_addr),
                 static_cast<socklen_t>(addrlen));
    return static_cast<intptr_t>(sent);
#endif
  }

  int CloseSocket(uintptr_t socket_handle) override {
#ifdef _WIN32
    return ::closesocket(static_cast<SOCKET>(socket_handle));
#else
    return ::close(static_cast<int>(socket_handle));
#endif
  }

  int LastError() override {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
  }
};

SocketOps& DefaultSocketOps() {
  static DefaultSocketOpsImpl ops;
  return ops;
}

}  // namespace detail

UDPBroadcaster::UDPBroadcaster(const std::string& target_ip,
                               uint16_t target_port,
                               detail::SocketOps* socket_ops)
    : target_ip_(target_ip),
      target_port_(target_port),
      initialized_(false),
      last_error_(),
      socket_ops_(socket_ops ? socket_ops : &detail::DefaultSocketOps()),
      socket_(kInvalidSocket)
#ifdef _WIN32
      , wsa_initialized_(false)
#endif
{
}

UDPBroadcaster::~UDPBroadcaster() {
  close();
}

bool UDPBroadcaster::initialize() {
  if (initialized_) {
    return true;
  }

  const int startup_result = socket_ops_->Startup();

#ifdef _WIN32
  if (startup_result != 0) {
    last_error_ = SocketErrorMessage("WSAStartup failed: ", startup_result);
    return false;
  }
  wsa_initialized_ = true;
#else
  (void)startup_result;
#endif

#ifdef _WIN32
  const int socket_protocol = IPPROTO_UDP;
#else
  const int socket_protocol = 0;
#endif
  socket_ = socket_ops_->CreateSocket(AF_INET, SOCK_DGRAM, socket_protocol);

#ifdef _WIN32
  if (socket_ == kInvalidSocket) {
    last_error_ =
        SocketErrorMessage("Socket creation failed: ", socket_ops_->LastError());
    socket_ops_->Cleanup();
    wsa_initialized_ = false;
    return false;
  }

  int broadcast = 1;
  if (socket_ops_->SetSockOpt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast,
                              sizeof(broadcast)) == SOCKET_ERROR) {
    last_error_ = SocketErrorMessage("Failed to set SO_BROADCAST: ",
                                     socket_ops_->LastError());
    socket_ops_->CloseSocket(socket_);
    socket_ops_->Cleanup();
    wsa_initialized_ = false;
    socket_ = kInvalidSocket;
    return false;
  }
#else
  if (socket_ == kInvalidSocket) {
    last_error_ =
        SocketErrorMessage("Socket creation failed: ", socket_ops_->LastError());
    return false;
  }

  int broadcast = 1;
  if (socket_ops_->SetSockOpt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast,
                              sizeof(broadcast)) < 0) {
    last_error_ =
        SocketErrorMessage("Failed to set SO_BROADCAST: ", socket_ops_->LastError());
    socket_ops_->CloseSocket(socket_);
    socket_ = kInvalidSocket;
    return false;
  }
#endif

  initialized_ = true;
  last_error_.clear();
  return true;
}

int UDPBroadcaster::send(const uint8_t* data, size_t size) {
  if (!initialized_) {
    last_error_ = "Socket not initialized";
    return -1;
  }

  sockaddr_in target_addr;
  std::memset(&target_addr, 0, sizeof(target_addr));
  target_addr.sin_family = AF_INET;
  target_addr.sin_port = htons(target_port_);

  if (socket_ops_->InetPton(AF_INET, target_ip_.c_str(),
                            &target_addr.sin_addr) != 1) {
    last_error_ = "Invalid IP address: " + target_ip_;
    return -1;
  }

  const intptr_t sent = socket_ops_->SendTo(socket_, data, size, 0, &target_addr,
                                            sizeof(target_addr));
  if (sent < 0) {
    last_error_ = SocketErrorMessage("sendto failed: ", socket_ops_->LastError());
    return -1;
  }

  last_error_.clear();
  return static_cast<int>(sent);
}

int UDPBroadcaster::send(const std::vector<uint8_t>& data) {
  return send(data.data(), data.size());
}

void UDPBroadcaster::setTarget(const std::string& target_ip,
                               uint16_t target_port) {
  target_ip_ = target_ip;
  target_port_ = target_port;
}

void UDPBroadcaster::close() {
  if (!initialized_) {
    return;
  }

  if (socket_ != kInvalidSocket) {
    socket_ops_->CloseSocket(socket_);
    socket_ = kInvalidSocket;
  }

  socket_ops_->Cleanup();

#ifdef _WIN32
  wsa_initialized_ = false;
#endif

  initialized_ = false;
}

#if defined(XP2GDL90_ENABLE_SOCKET_OPS_TESTS)
namespace detail {

int xp2gdl90_test_default_socket_ops_startup() {
  return DefaultSocketOps().Startup();
}

void xp2gdl90_test_default_socket_ops_cleanup() {
  DefaultSocketOps().Cleanup();
}

uintptr_t xp2gdl90_test_default_socket_ops_create_socket(int domain, int type,
                                                        int protocol) {
  DefaultSocketOpsImpl ops;
  return ops.CreateSocket(domain, type, protocol);
}

}  // namespace detail
#endif

}  // namespace udp
