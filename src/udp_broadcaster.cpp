#include "udp_broadcaster.h"
#include <cstring>

// Platform-specific includes
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
#endif

namespace udp {

UDPBroadcaster::UDPBroadcaster(const std::string& target_ip, uint16_t target_port)
    : target_ip_(target_ip)
    , target_port_(target_port)
    , initialized_(false)
    , last_error_("")
#ifdef _WIN32
    , socket_(INVALID_SOCKET)
    , wsa_initialized_(false)
#else
    , socket_(INVALID_SOCKET)
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
    // Initialize Winsock
    WSADATA wsa_data;
    int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (result != 0) {
        last_error_ = "WSAStartup failed: " + std::to_string(result);
        return false;
    }
    wsa_initialized_ = true;

    // Create socket
    socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (socket_ == INVALID_SOCKET) {
        last_error_ = "Socket creation failed: " + std::to_string(WSAGetLastError());
        WSACleanup();
        wsa_initialized_ = false;
        return false;
    }

    // Enable broadcast
    BOOL broadcast = TRUE;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcast, sizeof(broadcast)) == SOCKET_ERROR) {
        last_error_ = "Failed to set SO_BROADCAST: " + std::to_string(WSAGetLastError());
        closesocket(socket_);
        WSACleanup();
        wsa_initialized_ = false;
        socket_ = INVALID_SOCKET;
        return false;
    }
#else
    // Unix/Linux/macOS
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ < 0) {
        last_error_ = "Socket creation failed: " + std::string(strerror(errno));
        return false;
    }

    // Enable broadcast
    int broadcast = 1;
    if (setsockopt(socket_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        last_error_ = "Failed to set SO_BROADCAST: " + std::string(strerror(errno));
        ::close(socket_);
        socket_ = INVALID_SOCKET;
        return false;
    }
#endif

    initialized_ = true;
    last_error_ = "";
    return true;
}

int UDPBroadcaster::send(const uint8_t* data, size_t size) {
    if (!initialized_) {
        last_error_ = "Socket not initialized";
        return -1;
    }

    // Setup target address
    struct sockaddr_in target_addr;
    std::memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(target_port_);
    
#ifdef _WIN32
    target_addr.sin_addr.S_un.S_addr = inet_addr(target_ip_.c_str());
#else
    if (inet_pton(AF_INET, target_ip_.c_str(), &target_addr.sin_addr) <= 0) {
        last_error_ = "Invalid IP address: " + target_ip_;
        return -1;
    }
#endif

    // Send data
#ifdef _WIN32
    int sent = sendto(socket_, (const char*)data, static_cast<int>(size), 0,
                      (struct sockaddr*)&target_addr, sizeof(target_addr));
    if (sent == SOCKET_ERROR) {
        last_error_ = "sendto failed: " + std::to_string(WSAGetLastError());
        return -1;
    }
#else
    ssize_t sent = sendto(socket_, data, size, 0,
                          (struct sockaddr*)&target_addr, sizeof(target_addr));
    if (sent < 0) {
        last_error_ = "sendto failed: " + std::string(strerror(errno));
        return -1;
    }
#endif

    return static_cast<int>(sent);
}

int UDPBroadcaster::send(const std::vector<uint8_t>& data) {
    return send(data.data(), data.size());
}

void UDPBroadcaster::close() {
    if (initialized_) {
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
}

} // namespace udp

