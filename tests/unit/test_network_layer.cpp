/**
 * XP2GDL90 Unit Tests - Network Layer Tests
 * 
 * Tests for UDP networking functionality
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define close_socket closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define close_socket close
#endif

// Mock network layer (would be in main plugin)
namespace network {
    
    class UDPBroadcaster {
    private:
        socket_t socket_;
        struct sockaddr_in targetAddr_;
        bool initialized_;
        std::string targetIP_;
        int targetPort_;
        
    public:
        UDPBroadcaster() : socket_(INVALID_SOCKET_VALUE), initialized_(false), targetPort_(0) {
            memset(&targetAddr_, 0, sizeof(targetAddr_));
        }
        
        ~UDPBroadcaster() {
            close();
        }
        
        bool initialize(const std::string& targetIP, int targetPort) {
            targetIP_ = targetIP;
            targetPort_ = targetPort;
            
#ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                return false;
            }
#endif
            
            // Create UDP socket
            socket_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (socket_ == INVALID_SOCKET_VALUE) {
                return false;
            }
            
            // Set up target address
            targetAddr_.sin_family = AF_INET;
            targetAddr_.sin_port = htons(static_cast<uint16_t>(targetPort));
            
            if (inet_pton(AF_INET, targetIP.c_str(), &targetAddr_.sin_addr) <= 0) {
                close_socket(socket_);
                socket_ = INVALID_SOCKET_VALUE;
                return false;
            }
            
            initialized_ = true;
            return true;
        }
        
        void close() {
            if (socket_ != INVALID_SOCKET_VALUE) {
                close_socket(socket_);
                socket_ = INVALID_SOCKET_VALUE;
            }
            initialized_ = false;
            
#ifdef _WIN32
            WSACleanup();
#endif
        }
        
        bool send(const std::vector<uint8_t>& data) {
            if (!initialized_ || socket_ == INVALID_SOCKET_VALUE) {
                return false;
            }
            
            int result = sendto(socket_, 
                              reinterpret_cast<const char*>(data.data()), 
                              static_cast<int>(data.size()),
                              0,
                              reinterpret_cast<const struct sockaddr*>(&targetAddr_),
                              sizeof(targetAddr_));
            
            return result != SOCKET_ERROR_VALUE;
        }
        
        bool send(const uint8_t* data, size_t length) {
            if (!initialized_ || socket_ == INVALID_SOCKET_VALUE || !data) {
                return false;
            }
            
            int result = sendto(socket_, 
                              reinterpret_cast<const char*>(data), 
                              static_cast<int>(length),
                              0,
                              reinterpret_cast<const struct sockaddr*>(&targetAddr_),
                              sizeof(targetAddr_));
            
            return result != SOCKET_ERROR_VALUE;
        }
        
        bool isInitialized() const {
            return initialized_;
        }
        
        std::string getTargetIP() const {
            return targetIP_;
        }
        
        int getTargetPort() const {
            return targetPort_;
        }
        
        socket_t getSocket() const {
            return socket_;
        }
    };
    
    // Simple UDP receiver for testing
    class UDPReceiver {
    private:
        socket_t socket_;
        struct sockaddr_in listenAddr_;
        bool initialized_;
        int listenPort_;
        
    public:
        UDPReceiver() : socket_(INVALID_SOCKET_VALUE), initialized_(false), listenPort_(0) {
            memset(&listenAddr_, 0, sizeof(listenAddr_));
        }
        
        ~UDPReceiver() {
            close();
        }
        
        bool initialize(int listenPort) {
            listenPort_ = listenPort;
            
#ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                return false;
            }
#endif
            
            socket_ = socket(AF_INET, SOCK_DGRAM, 0);
            if (socket_ == INVALID_SOCKET_VALUE) {
                return false;
            }
            
            // Allow socket reuse
            int reuse = 1;
            setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<const char*>(&reuse), sizeof(reuse));
            
            // Set up listen address
            listenAddr_.sin_family = AF_INET;
            listenAddr_.sin_addr.s_addr = INADDR_ANY;
            listenAddr_.sin_port = htons(static_cast<uint16_t>(listenPort));
            
            if (bind(socket_, reinterpret_cast<const struct sockaddr*>(&listenAddr_), 
                    sizeof(listenAddr_)) == SOCKET_ERROR_VALUE) {
                close_socket(socket_);
                socket_ = INVALID_SOCKET_VALUE;
                return false;
            }
            
            initialized_ = true;
            return true;
        }
        
        void close() {
            if (socket_ != INVALID_SOCKET_VALUE) {
                close_socket(socket_);
                socket_ = INVALID_SOCKET_VALUE;
            }
            initialized_ = false;
            
#ifdef _WIN32
            WSACleanup();
#endif
        }
        
        bool receive(std::vector<uint8_t>& data, int timeoutMs = 1000) {
            if (!initialized_ || socket_ == INVALID_SOCKET_VALUE) {
                return false;
            }
            
            // Set receive timeout
            struct timeval timeout;
            timeout.tv_sec = timeoutMs / 1000;
            timeout.tv_usec = (timeoutMs % 1000) * 1000;
            
            setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, 
                      reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            
            // Buffer for incoming data
            uint8_t buffer[2048];
            struct sockaddr_in senderAddr;
            socklen_t senderLen = sizeof(senderAddr);
            
            int bytesReceived = recvfrom(socket_, 
                                       reinterpret_cast<char*>(buffer), 
                                       sizeof(buffer),
                                       0,
                                       reinterpret_cast<struct sockaddr*>(&senderAddr),
                                       &senderLen);
            
            if (bytesReceived > 0) {
                data.assign(buffer, buffer + bytesReceived);
                return true;
            }
            
            return false;
        }
        
        bool isInitialized() const {
            return initialized_;
        }
        
        int getListenPort() const {
            return listenPort_;
        }
    };
    
    // Network utilities
    class NetworkUtils {
    public:
        static bool isValidIP(const std::string& ip) {
            struct sockaddr_in sa;
            int result = inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr));
            return result != 0;
        }
        
        static bool isValidPort(int port) {
            return port > 0 && port <= 65535;
        }
        
        static std::string getLocalIP() {
            // Simplified - return localhost for testing
            return "127.0.0.1";
        }
        
        static bool isPortAvailable(int port) {
            socket_t testSocket = socket(AF_INET, SOCK_DGRAM, 0);
            if (testSocket == INVALID_SOCKET_VALUE) {
                return false;
            }
            
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = INADDR_ANY;
            addr.sin_port = htons(static_cast<uint16_t>(port));
            
            bool available = (bind(testSocket, 
                                 reinterpret_cast<const struct sockaddr*>(&addr), 
                                 sizeof(addr)) != SOCKET_ERROR_VALUE);
            
            close_socket(testSocket);
            return available;
        }
    };
}

// Test fixture for network tests
class NetworkLayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Find an available port for testing
        testPort_ = findAvailablePort();
        ASSERT_GT(testPort_, 0) << "Could not find available port for testing";
    }
    
    void TearDown() override {
        // Clean up any network resources
    }
    
    int findAvailablePort() {
        // Try ports starting from 4001
        for (int port = 4001; port < 4100; port++) {
            if (network::NetworkUtils::isPortAvailable(port)) {
                return port;
            }
        }
        return -1;
    }
    
    int testPort_;
};

// Test UDP broadcaster initialization
TEST_F(NetworkLayerTest, BroadcasterInitialization) {
    network::UDPBroadcaster broadcaster;
    
    EXPECT_FALSE(broadcaster.isInitialized());
    
    bool result = broadcaster.initialize("127.0.0.1", testPort_);
    EXPECT_TRUE(result);
    EXPECT_TRUE(broadcaster.isInitialized());
    EXPECT_EQ(broadcaster.getTargetIP(), "127.0.0.1");
    EXPECT_EQ(broadcaster.getTargetPort(), testPort_);
}

// Test invalid IP address
TEST_F(NetworkLayerTest, InvalidIPAddress) {
    network::UDPBroadcaster broadcaster;
    
    bool result = broadcaster.initialize("invalid.ip.address", testPort_);
    EXPECT_FALSE(result);
    EXPECT_FALSE(broadcaster.isInitialized());
}

// Test invalid port numbers
TEST_F(NetworkLayerTest, InvalidPortNumbers) {
    network::UDPBroadcaster broadcaster;
    
    // Test port 0
    bool result = broadcaster.initialize("127.0.0.1", 0);
    EXPECT_FALSE(result);
    
    // Test negative port
    result = broadcaster.initialize("127.0.0.1", -1);
    EXPECT_FALSE(result);
    
    // Test port too high
    result = broadcaster.initialize("127.0.0.1", 65536);
    EXPECT_FALSE(result);
}

// Test UDP receiver initialization
TEST_F(NetworkLayerTest, ReceiverInitialization) {
    network::UDPReceiver receiver;
    
    EXPECT_FALSE(receiver.isInitialized());
    
    bool result = receiver.initialize(testPort_);
    EXPECT_TRUE(result);
    EXPECT_TRUE(receiver.isInitialized());
    EXPECT_EQ(receiver.getListenPort(), testPort_);
}

// Test basic UDP send and receive
TEST_F(NetworkLayerTest, BasicSendReceive) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    // Initialize receiver first
    ASSERT_TRUE(receiver.initialize(testPort_));
    
    // Initialize broadcaster
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    // Test data
    std::vector<uint8_t> testData = {0x7E, 0x00, 0x01, 0x02, 0x03, 0x7E};
    
    // Send data
    EXPECT_TRUE(broadcaster.send(testData));
    
    // Receive data
    std::vector<uint8_t> receivedData;
    EXPECT_TRUE(receiver.receive(receivedData, 1000));
    
    // Verify data
    EXPECT_EQ(receivedData.size(), testData.size());
    EXPECT_EQ(receivedData, testData);
}

// Test sending with raw buffer
TEST_F(NetworkLayerTest, SendRawBuffer) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    uint8_t testData[] = {0x7E, 0x0A, 0x01, 0x02, 0x03, 0x04, 0x05, 0x7E};
    size_t dataSize = sizeof(testData);
    
    EXPECT_TRUE(broadcaster.send(testData, dataSize));
    
    std::vector<uint8_t> receivedData;
    EXPECT_TRUE(receiver.receive(receivedData, 1000));
    
    EXPECT_EQ(receivedData.size(), dataSize);
    for (size_t i = 0; i < dataSize; i++) {
        EXPECT_EQ(receivedData[i], testData[i]);
    }
}

// Test multiple messages
TEST_F(NetworkLayerTest, MultipleMessages) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    const int numMessages = 10;
    
    for (int i = 0; i < numMessages; i++) {
        std::vector<uint8_t> testData = {0x7E, static_cast<uint8_t>(i), 0x01, 0x02, 0x7E};
        
        EXPECT_TRUE(broadcaster.send(testData));
        
        std::vector<uint8_t> receivedData;
        EXPECT_TRUE(receiver.receive(receivedData, 1000));
        
        EXPECT_EQ(receivedData.size(), testData.size());
        EXPECT_EQ(receivedData, testData);
    }
}

// Test large message
TEST_F(NetworkLayerTest, LargeMessage) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    // Create large message (but within UDP limits)
    std::vector<uint8_t> testData(1400); // Typical MTU is ~1500 bytes
    for (size_t i = 0; i < testData.size(); i++) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }
    
    EXPECT_TRUE(broadcaster.send(testData));
    
    std::vector<uint8_t> receivedData;
    EXPECT_TRUE(receiver.receive(receivedData, 1000));
    
    EXPECT_EQ(receivedData.size(), testData.size());
    EXPECT_EQ(receivedData, testData);
}

// Test network utilities
TEST_F(NetworkLayerTest, NetworkUtilities) {
    // Test IP validation
    EXPECT_TRUE(network::NetworkUtils::isValidIP("127.0.0.1"));
    EXPECT_TRUE(network::NetworkUtils::isValidIP("192.168.1.1"));
    EXPECT_TRUE(network::NetworkUtils::isValidIP("0.0.0.0"));
    EXPECT_FALSE(network::NetworkUtils::isValidIP("invalid"));
    EXPECT_FALSE(network::NetworkUtils::isValidIP("256.256.256.256"));
    EXPECT_FALSE(network::NetworkUtils::isValidIP(""));
    
    // Test port validation
    EXPECT_TRUE(network::NetworkUtils::isValidPort(1));
    EXPECT_TRUE(network::NetworkUtils::isValidPort(4000));
    EXPECT_TRUE(network::NetworkUtils::isValidPort(65535));
    EXPECT_FALSE(network::NetworkUtils::isValidPort(0));
    EXPECT_FALSE(network::NetworkUtils::isValidPort(-1));
    EXPECT_FALSE(network::NetworkUtils::isValidPort(65536));
    
    // Test local IP
    std::string localIP = network::NetworkUtils::getLocalIP();
    EXPECT_FALSE(localIP.empty());
    EXPECT_TRUE(network::NetworkUtils::isValidIP(localIP));
}

// Test timeout behavior
TEST_F(NetworkLayerTest, ReceiveTimeout) {
    network::UDPReceiver receiver;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    
    auto start = std::chrono::steady_clock::now();
    
    std::vector<uint8_t> receivedData;
    bool result = receiver.receive(receivedData, 500); // 500ms timeout
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_FALSE(result); // Should timeout
    EXPECT_GE(duration.count(), 450); // Should wait at least 450ms
    EXPECT_LE(duration.count(), 600); // Should not wait more than 600ms
}

// Test concurrent access
TEST_F(NetworkLayerTest, ConcurrentAccess) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    const int numThreads = 5;
    const int messagesPerThread = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount(0);
    
    // Start sender threads
    for (int t = 0; t < numThreads; t++) {
        threads.emplace_back([&broadcaster, &successCount, t, messagesPerThread]() {
            for (int i = 0; i < messagesPerThread; i++) {
                std::vector<uint8_t> data = {0x7E, static_cast<uint8_t>(t), static_cast<uint8_t>(i), 0x7E};
                if (broadcaster.send(data)) {
                    successCount++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(successCount.load(), numThreads * messagesPerThread);
}

// Test resource cleanup
TEST_F(NetworkLayerTest, ResourceCleanup) {
    {
        network::UDPBroadcaster broadcaster;
        EXPECT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
        EXPECT_TRUE(broadcaster.isInitialized());
        // Destructor should clean up automatically
    }
    
    {
        network::UDPReceiver receiver;
        EXPECT_TRUE(receiver.initialize(testPort_));
        EXPECT_TRUE(receiver.isInitialized());
        // Destructor should clean up automatically
    }
    
    // Port should be available again after cleanup
    EXPECT_TRUE(network::NetworkUtils::isPortAvailable(testPort_));
}

// Performance test
TEST_F(NetworkLayerTest, SendPerformance) {
    network::UDPReceiver receiver;
    network::UDPBroadcaster broadcaster;
    
    ASSERT_TRUE(receiver.initialize(testPort_));
    ASSERT_TRUE(broadcaster.initialize("127.0.0.1", testPort_));
    
    const int numMessages = 1000;
    std::vector<uint8_t> testData = {0x7E, 0x00, 0x01, 0x02, 0x03, 0x7E};
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int successCount = 0;
    for (int i = 0; i < numMessages; i++) {
        if (broadcaster.send(testData)) {
            successCount++;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    EXPECT_EQ(successCount, numMessages);
    
    // Should be able to send 1000 small messages in less than 1 second
    EXPECT_LT(duration.count(), 1000000);
    
    std::cout << "Network performance: " << numMessages << " messages in " 
              << duration.count() << " microseconds" << std::endl;
    std::cout << "Average: " << (duration.count() / numMessages) << " microseconds per message" << std::endl;
}
