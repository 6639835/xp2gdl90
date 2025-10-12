#ifndef UDP_BROADCASTER_H
#define UDP_BROADCASTER_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * UDP Broadcaster for GDL90 messages
 * Cross-platform UDP socket implementation
 */

namespace udp {

class UDPBroadcaster {
public:
    /**
     * Constructor
     * @param target_ip Target IP address (e.g., "192.168.1.100" or "255.255.255.255")
     * @param target_port Target port number
     */
    UDPBroadcaster(const std::string& target_ip, uint16_t target_port);
    
    /**
     * Destructor - closes socket
     */
    ~UDPBroadcaster();

    /**
     * Initialize and open UDP socket
     * @return true if successful, false otherwise
     */
    bool initialize();

    /**
     * Send data via UDP
     * @param data Data buffer to send
     * @param size Size of data in bytes
     * @return Number of bytes sent, or -1 on error
     */
    int send(const uint8_t* data, size_t size);

    /**
     * Send data via UDP (vector version)
     * @param data Data vector to send
     * @return Number of bytes sent, or -1 on error
     */
    int send(const std::vector<uint8_t>& data);

    /**
     * Check if socket is initialized
     */
    bool isInitialized() const { return initialized_; }

    /**
     * Get last error message
     */
    std::string getLastError() const { return last_error_; }

    /**
     * Close the socket
     */
    void close();

private:
    std::string target_ip_;
    uint16_t target_port_;
    bool initialized_;
    std::string last_error_;
    
#ifdef _WIN32
    // Windows socket handle
    uintptr_t socket_;  // SOCKET type
    bool wsa_initialized_;
#else
    // Unix socket descriptor
    int socket_;
#endif
};

} // namespace udp

#endif // UDP_BROADCASTER_H

