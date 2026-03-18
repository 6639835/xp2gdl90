#ifndef UDP_RECEIVER_H
#define UDP_RECEIVER_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace udp {

class UDPReceiver {
 public:
  static constexpr uintptr_t kInvalidSocket = static_cast<uintptr_t>(-1);

  explicit UDPReceiver(uint16_t listen_port);
  ~UDPReceiver();

  bool initialize();
  int receive(std::vector<uint8_t>* out_data,
              std::string* out_source_ip = nullptr,
              uint16_t* out_source_port = nullptr);

  void close();

  bool isInitialized() const { return initialized_; }
  uint16_t getListenPort() const { return listen_port_; }
  std::string getLastError() const { return last_error_; }

 private:
  uint16_t listen_port_;
  bool initialized_;
  std::string last_error_;
  uintptr_t socket_;
#ifdef _WIN32
  bool wsa_initialized_;
#endif
};

}  // namespace udp

#endif  // UDP_RECEIVER_H
