/**
 * XP2GDL90 Integration Tests - Full Workflow Tests
 * 
 * End-to-end integration tests for complete plugin workflow
 */

#include <gtest/gtest.h>
#include "../mocks/XPLMMock.h"
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include <fstream>

// Integration test combines all components
namespace integration {
    
    // Mock integrated system combining all components
    class IntegratedXP2GDL90System {
    private:
        struct FlightData {
            double latitude;
            double longitude;
            double elevation;
            float groundSpeed;
            float heading;
            float verticalSpeed;
            bool onGround;
            uint32_t icaoAddress;
        };
        
        struct TrafficTarget {
            uint32_t icaoAddress;
            double latitude;
            double longitude;
            double elevation;
        };
        
        struct GDL90Message {
            uint8_t messageId;
            std::vector<uint8_t> payload;
            std::chrono::steady_clock::time_point timestamp;
        };
        
        bool initialized_;
        bool enabled_;
        std::vector<GDL90Message> sentMessages_;
        std::chrono::steady_clock::time_point lastHeartbeat_;
        std::chrono::steady_clock::time_point lastOwnshipReport_;
        int messageCounter_;
        
        // Simulated network endpoint
        std::string targetIP_;
        int targetPort_;
        
        // Mock flight loop callback
        static float flightLoopCallback(float elapsedSinceLastCall,
                                      float elapsedTimeSinceLastFlightLoop,
                                      int counter,
                                      void* refcon) {
            (void)elapsedSinceLastCall;
            (void)elapsedTimeSinceLastFlightLoop;
            (void)counter;
            
            IntegratedXP2GDL90System* system = static_cast<IntegratedXP2GDL90System*>(refcon);
            if (system && system->enabled_) {
                system->processFlightLoop();
            }
            
            return 0.5f; // 2Hz
        }
        
    public:
        IntegratedXP2GDL90System() 
            : initialized_(false)
            , enabled_(false)
            , messageCounter_(0)
            , targetIP_("127.0.0.1")
            , targetPort_(4000)
        {
        }
        
        bool initialize() {
            if (initialized_) return true;
            
            XPLMDebugString("XP2GDL90 Integration: Initializing system\n");
            
            // Check required datarefs
            std::vector<const char*> requiredRefs = {
                "sim/flightmodel/position/latitude",
                "sim/flightmodel/position/longitude", 
                "sim/flightmodel/position/elevation",
                "sim/flightmodel/position/groundspeed",
                "sim/flightmodel/position/psi"
            };
            
            for (const char* refName : requiredRefs) {
                XPLMDataRef ref = XPLMFindDataRef(refName);
                if (ref == nullptr) {
                    XPLMDebugString(("XP2GDL90 Integration: Failed to find dataref: " + std::string(refName) + "\n").c_str());
                    return false;
                }
            }
            
            initialized_ = true;
            XPLMDebugString("XP2GDL90 Integration: System initialized\n");
            return true;
        }
        
        bool enable() {
            if (!initialized_ || enabled_) return false;
            
            XPLMDebugString("XP2GDL90 Integration: Enabling system\n");
            
            // Register flight loop
            XPLMRegisterFlightLoopCallback(flightLoopCallback, 0.5f, this);
            
            enabled_ = true;
            lastHeartbeat_ = std::chrono::steady_clock::now();
            lastOwnshipReport_ = std::chrono::steady_clock::now();
            
            XPLMDebugString("XP2GDL90 Integration: System enabled\n");
            return true;
        }
        
        void disable() {
            if (!enabled_) return;
            
            XPLMDebugString("XP2GDL90 Integration: Disabling system\n");
            
            // Unregister flight loop
            XPLMUnregisterFlightLoopCallback(flightLoopCallback, this);
            
            enabled_ = false;
            XPLMDebugString("XP2GDL90 Integration: System disabled\n");
        }
        
        void shutdown() {
            if (enabled_) {
                disable();
            }
            
            sentMessages_.clear();
            initialized_ = false;
            messageCounter_ = 0;
            
            XPLMDebugString("XP2GDL90 Integration: System shutdown\n");
        }
        
        void processFlightLoop() {
            auto now = std::chrono::steady_clock::now();
            
            // Send heartbeat every 1 second
            auto heartbeatInterval = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastHeartbeat_);
            if (heartbeatInterval.count() >= 1000) {
                sendHeartbeat();
                lastHeartbeat_ = now;
            }
            
            // Send ownship report every 500ms (2Hz)
            auto ownshipInterval = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - lastOwnshipReport_);
            if (ownshipInterval.count() >= 500) {
                sendOwnshipReport();
                lastOwnshipReport_ = now;
            }
            
            // Process traffic data
            processTrafficData();
        }
        
        void sendHeartbeat() {
            GDL90Message message;
            message.messageId = 0x00; // Heartbeat
            message.timestamp = std::chrono::steady_clock::now();
            
            // Mock heartbeat payload
            message.payload = {
                0x00,  // Message ID
                0x01,  // Status byte (GPS valid)
                0x00, 0x0E, 0x4D, // Timestamp (mock)
                0x00, 0x01         // Message count
            };
            
            sentMessages_.push_back(message);
            messageCounter_++;
            
            XPLMDebugString("XP2GDL90 Integration: Sent heartbeat\n");
        }
        
        void sendOwnshipReport() {
            FlightData data = readFlightData();
            
            GDL90Message message;
            message.messageId = 0x0A; // Ownship report
            message.timestamp = std::chrono::steady_clock::now();
            
            // Mock ownship report payload (simplified)
            message.payload = {
                0x0A,  // Message ID
                0x00,  // Alert status
                0xAB, 0xCD, 0xEF, // ICAO address
                // Position data would be encoded here
                0x01, 0x02, 0x03, // Latitude (mock)
                0x04, 0x05, 0x06, // Longitude (mock)
                0x07, 0x08,       // Altitude (mock)
                // Additional fields...
            };
            
            sentMessages_.push_back(message);
            messageCounter_++;
            
            XPLMDebugString(("XP2GDL90 Integration: Sent ownship report - LAT=" + 
                           std::to_string(data.latitude) + ", LON=" + 
                           std::to_string(data.longitude) + "\n").c_str());
        }
        
        void processTrafficData() {
            // Read TCAS data
            XPLMDataRef tcasCountRef = XPLMFindDataRef("sim/cockpit2/tcas/num_acf");
            if (tcasCountRef) {
                int numTargets = XPLMGetDatai(tcasCountRef);
                if (numTargets > 0) {
                    sendTrafficReports(numTargets);
                }
            }
        }
        
        void sendTrafficReports(int numTargets) {
            // Limit to reasonable number
            numTargets = std::min(numTargets, 63);
            
            for (int i = 0; i < numTargets; i++) {
                GDL90Message message;
                message.messageId = 0x14; // Traffic report
                message.timestamp = std::chrono::steady_clock::now();
                
                // Mock traffic report payload
                message.payload = {
                    0x14, // Message ID
                    0x00, // Alert status
                    static_cast<uint8_t>(0x10), 
                    static_cast<uint8_t>(0x00 + i), 
                    static_cast<uint8_t>(0x00), // ICAO address (mock)
                    // Position data for traffic target...
                };
                
                sentMessages_.push_back(message);
                messageCounter_++;
            }
            
            XPLMDebugString(("XP2GDL90 Integration: Sent " + 
                           std::to_string(numTargets) + " traffic reports\n").c_str());
        }
        
        FlightData readFlightData() {
            FlightData data = {};
            
            XPLMDataRef latRef = XPLMFindDataRef("sim/flightmodel/position/latitude");
            XPLMDataRef lonRef = XPLMFindDataRef("sim/flightmodel/position/longitude");
            XPLMDataRef elevRef = XPLMFindDataRef("sim/flightmodel/position/elevation");
            XPLMDataRef speedRef = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
            XPLMDataRef headingRef = XPLMFindDataRef("sim/flightmodel/position/psi");
            
            if (latRef) data.latitude = XPLMGetDatad(latRef);
            if (lonRef) data.longitude = XPLMGetDatad(lonRef);
            if (elevRef) data.elevation = XPLMGetDataf(elevRef);
            if (speedRef) data.groundSpeed = XPLMGetDataf(speedRef);
            if (headingRef) data.heading = XPLMGetDataf(headingRef);
            
            data.icaoAddress = 0xABCDEF;
            
            return data;
        }
        
        // Test accessors
        bool isInitialized() const { return initialized_; }
        bool isEnabled() const { return enabled_; }
        size_t getSentMessageCount() const { return sentMessages_.size(); }
        const std::vector<GDL90Message>& getSentMessages() const { return sentMessages_; }
        int getMessageCounter() const { return messageCounter_; }
        
        void clearMessages() { 
            sentMessages_.clear(); 
            messageCounter_ = 0;
        }
        
        size_t getHeartbeatCount() const {
            size_t count = 0;
            for (const auto& msg : sentMessages_) {
                if (msg.messageId == 0x00) count++;
            }
            return count;
        }
        
        size_t getOwnshipReportCount() const {
            size_t count = 0;
            for (const auto& msg : sentMessages_) {
                if (msg.messageId == 0x0A) count++;
            }
            return count;
        }
        
        size_t getTrafficReportCount() const {
            size_t count = 0;
            for (const auto& msg : sentMessages_) {
                if (msg.messageId == 0x14) count++;
            }
            return count;
        }
        
        void setTargetEndpoint(const std::string& ip, int port) {
            targetIP_ = ip;
            targetPort_ = port;
        }
        
        std::string getTargetIP() const { return targetIP_; }
        int getTargetPort() const { return targetPort_; }
    };
}

// Integration test fixture
class FullWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        XPLMockState::getInstance().reset();
        
        // Set up complete flight environment
        setupFlightEnvironment();
        
        system_ = std::make_unique<integration::IntegratedXP2GDL90System>();
    }
    
    void TearDown() override {
        system_.reset();
        XPLMockState::getInstance().reset();
    }
    
    void setupFlightEnvironment() {
        // Set up realistic aircraft state
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 37.524);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -122.063);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/elevation", 100.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/groundspeed", 25.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/psi", 90.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/vh_ind_fpm", 0.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/failures/onground_any", 0);
        
        // Set up traffic environment
        XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", 2);
        
        std::vector<float> trafficLats = {37.525, 37.526};
        std::vector<float> trafficLons = {-122.064, -122.065};
        std::vector<float> trafficElevs = {200.0f, 300.0f};
        
        XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lat", trafficLats);
        XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lon", trafficLons);
        XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/ele", trafficElevs);
    }
    
    std::unique_ptr<integration::IntegratedXP2GDL90System> system_;
};

// Test complete system initialization
TEST_F(FullWorkflowTest, SystemInitialization) {
    EXPECT_FALSE(system_->isInitialized());
    EXPECT_FALSE(system_->isEnabled());
    
    bool result = system_->initialize();
    EXPECT_TRUE(result);
    EXPECT_TRUE(system_->isInitialized());
    EXPECT_FALSE(system_->isEnabled());
}

// Test full system lifecycle
TEST_F(FullWorkflowTest, FullSystemLifecycle) {
    // Initialize
    ASSERT_TRUE(system_->initialize());
    
    // Enable
    ASSERT_TRUE(system_->enable());
    EXPECT_TRUE(system_->isEnabled());
    
    // Disable
    system_->disable();
    EXPECT_FALSE(system_->isEnabled());
    
    // Shutdown
    system_->shutdown();
    EXPECT_FALSE(system_->isInitialized());
}

// Test message generation timing
TEST_F(FullWorkflowTest, MessageGenerationTiming) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Simulate 3 seconds of operation
    const int simulationTimeMs = 3000;
    const int stepMs = 100;
    
    for (int elapsed = 0; elapsed < simulationTimeMs; elapsed += stepMs) {
        XPLMockState::getInstance().executeFlightLoops(0.1f); // 100ms
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay
    }
    
    // Check message counts
    size_t heartbeats = system_->getHeartbeatCount();
    size_t ownshipReports = system_->getOwnshipReportCount();
    size_t trafficReports = system_->getTrafficReportCount();
    
    // Expect approximately:
    // - 3 heartbeats (1Hz for 3 seconds)
    // - 6 ownship reports (2Hz for 3 seconds) 
    // - Multiple traffic reports (2 targets * multiple updates)
    
    EXPECT_GE(heartbeats, 2); // At least 2 heartbeats
    EXPECT_LE(heartbeats, 4); // At most 4 heartbeats
    
    EXPECT_GE(ownshipReports, 4); // At least 4 ownship reports
    EXPECT_LE(ownshipReports, 8); // At most 8 ownship reports
    
    EXPECT_GT(trafficReports, 0); // Should have traffic reports
    
    std::cout << "Message counts after 3s: Heartbeats=" << heartbeats 
              << ", Ownship=" << ownshipReports 
              << ", Traffic=" << trafficReports << std::endl;
}

// Test flight data integration
TEST_F(FullWorkflowTest, FlightDataIntegration) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Change flight data during operation
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 38.0);
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -123.0);
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/elevation", 500.0f);
    
    // Execute flight loop to process new data
    XPLMockState::getInstance().executeFlightLoops(0.5f);
    
    // Verify system processed the updated data
    EXPECT_GT(system_->getSentMessageCount(), 0);
    
    // Check debug messages for updated coordinates
    std::vector<std::string> debugStrings = XPLMockState::getInstance().getDebugStrings();
    bool foundUpdatedPosition = false;
    
    for (const auto& msg : debugStrings) {
        if (msg.find("LAT=38") != std::string::npos && msg.find("LON=-123") != std::string::npos) {
            foundUpdatedPosition = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundUpdatedPosition);
}

// Test traffic data processing
TEST_F(FullWorkflowTest, TrafficDataProcessing) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Execute flight loop to process traffic data
    XPLMockState::getInstance().executeFlightLoops(0.5f);
    
    // Should have traffic reports for the 2 configured targets
    size_t trafficReports = system_->getTrafficReportCount();
    EXPECT_EQ(trafficReports, 2);
    
    // Verify debug messages mention traffic
    std::vector<std::string> debugStrings = XPLMockState::getInstance().getDebugStrings();
    bool foundTrafficMessage = false;
    
    for (const auto& msg : debugStrings) {
        if (msg.find("traffic reports") != std::string::npos) {
            foundTrafficMessage = true;
            break;
        }
    }
    
    EXPECT_TRUE(foundTrafficMessage);
}

// Test no traffic scenario
TEST_F(FullWorkflowTest, NoTrafficScenario) {
    // Set up environment with no traffic
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", 0);
    
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    XPLMockState::getInstance().executeFlightLoops(1.0f);
    
    // Should have heartbeat and ownship reports, but no traffic reports
    EXPECT_GT(system_->getHeartbeatCount(), 0);
    EXPECT_GT(system_->getOwnshipReportCount(), 0);
    EXPECT_EQ(system_->getTrafficReportCount(), 0);
}

// Test high traffic scenario
TEST_F(FullWorkflowTest, HighTrafficScenario) {
    // Set up environment with maximum traffic
    const int maxTraffic = 63; // GDL-90 limit
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", maxTraffic);
    
    // Create arrays for all traffic targets
    std::vector<float> trafficLats(maxTraffic);
    std::vector<float> trafficLons(maxTraffic);
    std::vector<float> trafficElevs(maxTraffic);
    
    for (int i = 0; i < maxTraffic; i++) {
        trafficLats[i] = 37.524f + (i * 0.001f);
        trafficLons[i] = -122.063f + (i * 0.001f);
        trafficElevs[i] = 1000.0f + (i * 100.0f);
    }
    
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lat", trafficLats);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lon", trafficLons);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/ele", trafficElevs);
    
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    XPLMockState::getInstance().executeFlightLoops(1.0f);
    
    // Should handle all traffic targets
    size_t trafficReports = system_->getTrafficReportCount();
    EXPECT_EQ(trafficReports, maxTraffic);
}

// Test system recovery from errors
TEST_F(FullWorkflowTest, ErrorRecovery) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Simulate error condition by clearing all datarefs
    XPLMockState::getInstance().reset();
    
    // Execute flight loop (should handle missing data gracefully)
    XPLMockState::getInstance().executeFlightLoops(0.5f);
    
    // System should still be enabled but may have fewer messages
    EXPECT_TRUE(system_->isEnabled());
    
    // Restore datarefs
    setupFlightEnvironment();
    
    // Execute flight loop again
    XPLMockState::getInstance().executeFlightLoops(0.5f);
    
    // Should resume normal operation
    EXPECT_GT(system_->getSentMessageCount(), 0);
}

// Test configuration changes
TEST_F(FullWorkflowTest, ConfigurationChanges) {
    ASSERT_TRUE(system_->initialize());
    
    // Test default configuration
    EXPECT_EQ(system_->getTargetIP(), "127.0.0.1");
    EXPECT_EQ(system_->getTargetPort(), 4000);
    
    // Change configuration
    system_->setTargetEndpoint("192.168.1.100", 5000);
    
    EXPECT_EQ(system_->getTargetIP(), "192.168.1.100");
    EXPECT_EQ(system_->getTargetPort(), 5000);
}

// Performance integration test
TEST_F(FullWorkflowTest, SystemPerformance) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    const int testDurationMs = 1000; // 1 second
    const int stepMs = 10;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int elapsed = 0; elapsed < testDurationMs; elapsed += stepMs) {
        XPLMockState::getInstance().executeFlightLoops(0.01f); // 10ms
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete within reasonable time
    EXPECT_LT(duration.count(), testDurationMs + 500); // Allow 500ms overhead
    
    // Should have generated messages
    EXPECT_GT(system_->getSentMessageCount(), 0);
    
    std::cout << "Performance test: " << system_->getSentMessageCount() 
              << " messages generated in " << duration.count() 
              << "ms (target " << testDurationMs << "ms)" << std::endl;
}

// Test memory usage patterns
TEST_F(FullWorkflowTest, MemoryUsagePattern) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Generate many messages
    for (int i = 0; i < 1000; i++) {
        XPLMockState::getInstance().executeFlightLoops(0.001f);
    }
    
    size_t messageCount = system_->getSentMessageCount();
    EXPECT_GT(messageCount, 0);
    
    // Clear messages to test memory cleanup
    system_->clearMessages();
    EXPECT_EQ(system_->getSentMessageCount(), 0);
    
    // System should still be operational
    EXPECT_TRUE(system_->isEnabled());
    
    // Generate more messages
    XPLMockState::getInstance().executeFlightLoops(0.5f);
    EXPECT_GT(system_->getSentMessageCount(), 0);
}

// Test concurrent operation stress
TEST_F(FullWorkflowTest, ConcurrentOperationStress) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Simulate rapid data changes
    std::vector<std::thread> threads;
    
    // Thread 1: Rapid position updates
    threads.emplace_back([this]() {
        for (int i = 0; i < 100; i++) {
            double lat = 37.524 + (i * 0.0001);
            double lon = -122.063 + (i * 0.0001);
            XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", lat);
            XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", lon);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Thread 2: Flight loop execution
    threads.emplace_back([this]() {
        for (int i = 0; i < 100; i++) {
            XPLMockState::getInstance().executeFlightLoops(0.01f);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Wait for threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // System should still be operational
    EXPECT_TRUE(system_->isEnabled());
    EXPECT_GT(system_->getSentMessageCount(), 0);
}

// Long-running stability test
TEST_F(FullWorkflowTest, LongRunningStability) {
    ASSERT_TRUE(system_->initialize());
    ASSERT_TRUE(system_->enable());
    
    // Simulate longer operation (10 seconds compressed)
    const int cycles = 1000;
    
    for (int i = 0; i < cycles; i++) {
        // Vary flight data
        double lat = 37.524 + sin(i * 0.1) * 0.01;
        double lon = -122.063 + cos(i * 0.1) * 0.01;
        float alt = 100.0f + (i % 100) * 10.0f;
        
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", lat);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", lon);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/elevation", alt);
        
        XPLMockState::getInstance().executeFlightLoops(0.01f);
        
        // Periodically clear messages to prevent unlimited growth
        if (i % 100 == 0) {
            size_t beforeClear = system_->getSentMessageCount();
            system_->clearMessages();
            EXPECT_LT(system_->getSentMessageCount(), beforeClear);
        }
    }
    
    // System should remain stable
    EXPECT_TRUE(system_->isEnabled());
    EXPECT_TRUE(system_->isInitialized());
}
