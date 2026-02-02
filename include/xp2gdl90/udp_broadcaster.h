#ifndef UDP_BROADCASTER_H
#define UDP_BROADCASTER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

/**
 * UDP Broadcaster for GDL90 messages.
 * Cross-platform UDP socket implementation.
 */

namespace udp {

namespace detail {

struct SocketOps {
  virtual ~SocketOps() = default;

  virtual int Startup() { return 0; }
  virtual void Cleanup() {}

  virtual uintptr_t CreateSocket(int domain, int type, int protocol) = 0;
  virtual int SetSockOpt(uintptr_t socket, int level, int optname,
                         const void* optval, size_t optlen) = 0;
  virtual int InetPton(int af, const char* src, void* dst) = 0;
  virtual intptr_t SendTo(uintptr_t socket, const void* buf, size_t len,
                          int flags, const void* dest_addr,
                          size_t addrlen) = 0;
  virtual int CloseSocket(uintptr_t socket) = 0;
  virtual int LastError() = 0;
};

SocketOps& DefaultSocketOps();

}  // namespace detail

class UDPBroadcaster {
 public:
  static constexpr uintptr_t kInvalidSocket = static_cast<uintptr_t>(-1);

  UDPBroadcaster(const std::string& target_ip, uint16_t target_port,
                 detail::SocketOps* socket_ops = nullptr);
  ~UDPBroadcaster();

  bool initialize();
  int send(const uint8_t* data, size_t size);
  int send(const std::vector<uint8_t>& data);

  bool isInitialized() const { return initialized_; }
  std::string getLastError() const { return last_error_; }

  void close();

 private:
  std::string target_ip_;
  uint16_t target_port_;
  bool initialized_;
  std::string last_error_;
  detail::SocketOps* socket_ops_;

  uintptr_t socket_;
#ifdef _WIN32
  bool wsa_initialized_;
#endif
};

}  // namespace udp

#endif  // UDP_BROADCASTER_H
