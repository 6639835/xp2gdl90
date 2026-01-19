#ifndef UDP_BROADCASTER_H
#define UDP_BROADCASTER_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * UDP Broadcaster for GDL90 messages.
 * Cross-platform UDP socket implementation.
 */

namespace udp {

class UDPBroadcaster {
 public:
  UDPBroadcaster(const std::string& target_ip, uint16_t target_port);
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

#ifdef _WIN32
  uintptr_t socket_;
  bool wsa_initialized_;
#else
  int socket_;
#endif
};

}  // namespace udp

#endif  // UDP_BROADCASTER_H
