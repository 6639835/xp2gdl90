#include "xp2gdl90/udp_broadcaster.h"

#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
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

UDPBroadcaster::UDPBroadcaster(const std::string& target_ip,
                               uint16_t target_port)
    : target_ip_(target_ip),
      target_port_(target_port),
      initialized_(false),
      last_error_(),
#ifdef _WIN32
      socket_(INVALID_SOCKET),
      wsa_initialized_(false)
#else
      socket_(INVALID_SOCKET)
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

#ifdef _WIN32
  WSADATA wsa_data;
  const int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (result != 0) {
    last_error_ = SocketErrorMessage("WSAStartup failed: ", result);
    return false;
  }
  wsa_initialized_ = true;

  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket_ == INVALID_SOCKET) {
    last_error_ = SocketErrorMessage("Socket creation failed: ",
                                     WSAGetLastError());
    WSACleanup();
    wsa_initialized_ = false;
    return false;
  }

  BOOL broadcast = TRUE;
  if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
                 reinterpret_cast<const char*>(&broadcast),
                 sizeof(broadcast)) == SOCKET_ERROR) {
    last_error_ = SocketErrorMessage("Failed to set SO_BROADCAST: ",
                                     WSAGetLastError());
    closesocket(socket_);
    WSACleanup();
    wsa_initialized_ = false;
    socket_ = INVALID_SOCKET;
    return false;
  }
#else
  socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_ < 0) {
    last_error_ = SocketErrorMessage("Socket creation failed: ", errno);
    return false;
  }

  int broadcast = 1;
  if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast,
                 sizeof(broadcast)) < 0) {
    last_error_ = SocketErrorMessage("Failed to set SO_BROADCAST: ", errno);
    ::close(socket_);
    socket_ = INVALID_SOCKET;
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

#ifdef _WIN32
  if (inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr) != 1) {
    last_error_ = "Invalid IP address: " + target_ip_;
    return -1;
  }
#else
  if (inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr) <= 0) {
    last_error_ = "Invalid IP address: " + target_ip_;
    return -1;
  }
#endif

#ifdef _WIN32
  const int sent = sendto(socket_, reinterpret_cast<const char*>(data),
                          static_cast<int>(size), 0,
                          reinterpret_cast<sockaddr*>(&target_addr),
                          sizeof(target_addr));
  if (sent == SOCKET_ERROR) {
    last_error_ = SocketErrorMessage("sendto failed: ", WSAGetLastError());
    return -1;
  }
#else
  const ssize_t sent =
      sendto(socket_, data, size, 0,
             reinterpret_cast<sockaddr*>(&target_addr), sizeof(target_addr));
  if (sent < 0) {
    last_error_ = SocketErrorMessage("sendto failed: ", errno);
    return -1;
  }
#endif

  return static_cast<int>(sent);
}

int UDPBroadcaster::send(const std::vector<uint8_t>& data) {
  return send(data.data(), data.size());
}

void UDPBroadcaster::close() {
  if (!initialized_) {
    return;
  }

#ifdef _WIN32
  if (socket_ != INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
  if (wsa_initialized_) {
    WSACleanup();
    wsa_initialized_ = false;
  }
#else
  if (socket_ >= 0) {
    ::close(socket_);
    socket_ = INVALID_SOCKET;
  }
#endif

  initialized_ = false;
}

}  // namespace udp
