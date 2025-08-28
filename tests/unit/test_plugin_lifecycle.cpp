/**
 * XP2GDL90 Unit Tests - Plugin Lifecycle Tests
 * 
 * Tests for X-Plane plugin lifecycle management
 */

#include <gtest/gtest.h>
#include "../mocks/XPLMMock.h"
#include <vector>
#include <string>
#include <memory>

// Mock plugin implementation (would be in main plugin)
namespace plugin {
    
    // Plugin state
    enum class PluginState {
        UNLOADED,
        LOADED,
        ENABLED,
        DISABLED,
        ERROR
    };
    
    // Mock plugin class
    class XP2GDL90Plugin {
    private:
        PluginState state_;
        std::string name_;
        std::string signature_;
        std::string description_;
        bool flightLoopRegistered_;
        float flightLoopInterval_;
        std::vector<std::string> errorMessages_;
        
        // Mock resources
        struct Resources {
            bool networkInitialized;
            bool dataRefsFound;
            int numDataRefs;
            
            Resources() : networkInitialized(false), dataRefsFound(false), numDataRefs(0) {}
        } resources_;
        
    public:
        XP2GDL90Plugin() 
            : state_(PluginState::UNLOADED)
            , name_("XP2GDL90")
            , signature_("com.example.xp2gdl90")
            , description_("XP2GDL90 - GDL-90 Data Broadcasting Plugin")
            , flightLoopRegistered_(false)
            , flightLoopInterval_(0.5f) // 2Hz default
        {
        }
        
        // Plugin lifecycle functions
        bool start() {
            if (state_ != PluginState::UNLOADED) {
                errorMessages_.push_back("Plugin already started");
                return false;
            }
            
            // Simulate plugin initialization
            XPLMDebugString("XP2GDL90: Plugin starting...\n");
            
            // Find required datarefs
            std::vector<std::string> requiredDataRefs = {
                "sim/flightmodel/position/latitude",
                "sim/flightmodel/position/longitude",
                "sim/flightmodel/position/elevation",
                "sim/flightmodel/position/groundspeed",
                "sim/flightmodel/position/psi",
                "sim/flightmodel/position/vh_ind_fpm"
            };
            
            int foundRefs = 0;
            for (const auto& refName : requiredDataRefs) {
                XPLMDataRef ref = XPLMFindDataRef(refName.c_str());
                if (ref != nullptr) {
                    foundRefs++;
                }
            }
            
            resources_.numDataRefs = foundRefs;
            resources_.dataRefsFound = (foundRefs == static_cast<int>(requiredDataRefs.size()));
            
            if (!resources_.dataRefsFound) {
                errorMessages_.push_back("Failed to find required datarefs");
                state_ = PluginState::ERROR;
                return false;
            }
            
            state_ = PluginState::LOADED;
            XPLMDebugString("XP2GDL90: Plugin started successfully\n");
            return true;
        }
        
        bool enable() {
            if (state_ != PluginState::LOADED && state_ != PluginState::DISABLED) {
                errorMessages_.push_back("Plugin not in correct state for enabling");
                return false;
            }
            
            XPLMDebugString("XP2GDL90: Plugin enabling...\n");
            
            // Initialize network
            resources_.networkInitialized = true; // Mock success
            
            if (!resources_.networkInitialized) {
                errorMessages_.push_back("Failed to initialize network");
                state_ = PluginState::ERROR;
                return false;
            }
            
            // Register flight loop
            XPLMRegisterFlightLoopCallback(flightLoopCallback, flightLoopInterval_, this);
            flightLoopRegistered_ = true;
            
            state_ = PluginState::ENABLED;
            XPLMDebugString("XP2GDL90: Plugin enabled successfully\n");
            return true;
        }
        
        void disable() {
            if (state_ != PluginState::ENABLED) {
                return;
            }
            
            XPLMDebugString("XP2GDL90: Plugin disabling...\n");
            
            // Unregister flight loop
            if (flightLoopRegistered_) {
                XPLMUnregisterFlightLoopCallback(flightLoopCallback, this);
                flightLoopRegistered_ = false;
            }
            
            // Cleanup network resources
            resources_.networkInitialized = false;
            
            state_ = PluginState::DISABLED;
            XPLMDebugString("XP2GDL90: Plugin disabled\n");
        }
        
        void stop() {
            if (state_ == PluginState::ENABLED) {
                disable();
            }
            
            if (state_ == PluginState::UNLOADED) {
                return;
            }
            
            XPLMDebugString("XP2GDL90: Plugin stopping...\n");
            
            // Cleanup all resources
            resources_ = Resources();
            errorMessages_.clear();
            
            state_ = PluginState::UNLOADED;
            XPLMDebugString("XP2GDL90: Plugin stopped\n");
        }
        
        // Flight loop callback
        static float flightLoopCallback(float elapsedSinceLastCall,
                                      float elapsedTimeSinceLastFlightLoop,
                                      int counter,
                                      void* refcon) {
            (void)elapsedSinceLastCall;
            (void)elapsedTimeSinceLastFlightLoop;
            (void)counter;
            
            XP2GDL90Plugin* plugin = static_cast<XP2GDL90Plugin*>(refcon);
            if (!plugin || plugin->state_ != PluginState::ENABLED) {
                return 0.0f; // Stop callback
            }
            
            // Mock flight loop processing
            plugin->processFlightLoop();
            
            return plugin->flightLoopInterval_; // Continue with same interval
        }
        
        void processFlightLoop() {
            // Mock flight loop processing
            // In real implementation, this would:
            // 1. Read flight data
            // 2. Encode GDL-90 messages
            // 3. Send UDP packets
        }
        
        // Accessors
        PluginState getState() const { return state_; }
        const std::string& getName() const { return name_; }
        const std::string& getSignature() const { return signature_; }
        const std::string& getDescription() const { return description_; }
        bool isFlightLoopRegistered() const { return flightLoopRegistered_; }
        float getFlightLoopInterval() const { return flightLoopInterval_; }
        const std::vector<std::string>& getErrorMessages() const { return errorMessages_; }
        
        // Resource status
        bool isNetworkInitialized() const { return resources_.networkInitialized; }
        bool areDataRefsFound() const { return resources_.dataRefsFound; }
        int getDataRefCount() const { return resources_.numDataRefs; }
        
        // Configuration
        void setFlightLoopInterval(float interval) { 
            flightLoopInterval_ = interval;
            if (flightLoopRegistered_) {
                // Re-register with new interval
                XPLMUnregisterFlightLoopCallback(flightLoopCallback, this);
                XPLMRegisterFlightLoopCallback(flightLoopCallback, flightLoopInterval_, this);
            }
        }
    };
}

// Test fixture for plugin lifecycle tests
class PluginLifecycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state
        XPLMockState::getInstance().reset();
        
        // Set up mock X-Plane environment
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 37.524);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -122.063);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/elevation", 100.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/groundspeed", 25.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/psi", 90.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/vh_ind_fpm", 0.0f);
        
        plugin_ = std::make_unique<plugin::XP2GDL90Plugin>();
    }
    
    void TearDown() override {
        plugin_.reset();
        XPLMockState::getInstance().reset();
    }
    
    std::unique_ptr<plugin::XP2GDL90Plugin> plugin_;
};

// Test plugin creation and initial state
TEST_F(PluginLifecycleTest, InitialState) {
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::UNLOADED);
    EXPECT_EQ(plugin_->getName(), "XP2GDL90");
    EXPECT_EQ(plugin_->getSignature(), "com.example.xp2gdl90");
    EXPECT_FALSE(plugin_->getDescription().empty());
    EXPECT_FALSE(plugin_->isFlightLoopRegistered());
    EXPECT_FALSE(plugin_->isNetworkInitialized());
    EXPECT_FALSE(plugin_->areDataRefsFound());
    EXPECT_EQ(plugin_->getDataRefCount(), 0);
    EXPECT_TRUE(plugin_->getErrorMessages().empty());
}

// Test plugin start
TEST_F(PluginLifecycleTest, PluginStart) {
    bool result = plugin_->start();
    
    EXPECT_TRUE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::LOADED);
    EXPECT_TRUE(plugin_->areDataRefsFound());
    EXPECT_EQ(plugin_->getDataRefCount(), 6); // All required datarefs found
    EXPECT_TRUE(plugin_->getErrorMessages().empty());
    
    // Check debug output
    std::vector<std::string> debugStrings = XPLMockState::getInstance().getDebugStrings();
    bool foundStartMessage = false;
    bool foundSuccessMessage = false;
    
    for (const auto& msg : debugStrings) {
        if (msg.find("Plugin starting") != std::string::npos) {
            foundStartMessage = true;
        }
        if (msg.find("started successfully") != std::string::npos) {
            foundSuccessMessage = true;
        }
    }
    
    EXPECT_TRUE(foundStartMessage);
    EXPECT_TRUE(foundSuccessMessage);
}

// Test plugin enable
TEST_F(PluginLifecycleTest, PluginEnable) {
    ASSERT_TRUE(plugin_->start());
    
    bool result = plugin_->enable();
    
    EXPECT_TRUE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::ENABLED);
    EXPECT_TRUE(plugin_->isNetworkInitialized());
    EXPECT_TRUE(plugin_->isFlightLoopRegistered());
    EXPECT_TRUE(plugin_->getErrorMessages().empty());
}

// Test full lifecycle (start -> enable -> disable -> stop)
TEST_F(PluginLifecycleTest, FullLifecycle) {
    // Start
    ASSERT_TRUE(plugin_->start());
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::LOADED);
    
    // Enable
    ASSERT_TRUE(plugin_->enable());
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::ENABLED);
    EXPECT_TRUE(plugin_->isFlightLoopRegistered());
    
    // Disable
    plugin_->disable();
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::DISABLED);
    EXPECT_FALSE(plugin_->isFlightLoopRegistered());
    EXPECT_FALSE(plugin_->isNetworkInitialized());
    
    // Stop
    plugin_->stop();
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::UNLOADED);
    EXPECT_FALSE(plugin_->areDataRefsFound());
}

// Test invalid state transitions
TEST_F(PluginLifecycleTest, InvalidStateTransitions) {
    // Try to enable before starting
    bool result = plugin_->enable();
    EXPECT_FALSE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::UNLOADED);
    EXPECT_FALSE(plugin_->getErrorMessages().empty());
    
    // Try to start twice
    ASSERT_TRUE(plugin_->start());
    result = plugin_->start();
    EXPECT_FALSE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::LOADED);
}

// Test flight loop registration
TEST_F(PluginLifecycleTest, FlightLoopRegistration) {
    ASSERT_TRUE(plugin_->start());
    ASSERT_TRUE(plugin_->enable());
    
    EXPECT_TRUE(plugin_->isFlightLoopRegistered());
    EXPECT_GT(plugin_->getFlightLoopInterval(), 0.0f);
    
    // Test flight loop execution
    float testInterval = 0.5f;
    XPLMockState::getInstance().executeFlightLoops(testInterval);
    
    // Plugin should still be enabled after flight loop execution
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::ENABLED);
}

// Test flight loop interval changes
TEST_F(PluginLifecycleTest, FlightLoopIntervalChange) {
    ASSERT_TRUE(plugin_->start());
    ASSERT_TRUE(plugin_->enable());
    
    float originalInterval = plugin_->getFlightLoopInterval();
    float newInterval = 1.0f; // 1Hz
    
    plugin_->setFlightLoopInterval(newInterval);
    
    EXPECT_EQ(plugin_->getFlightLoopInterval(), newInterval);
    EXPECT_NE(plugin_->getFlightLoopInterval(), originalInterval);
    EXPECT_TRUE(plugin_->isFlightLoopRegistered()); // Should still be registered
}

// Test error handling with missing datarefs
TEST_F(PluginLifecycleTest, MissingDataRefsError) {
    // Clear mock datarefs to simulate missing datarefs
    XPLMockState::getInstance().reset();
    
    bool result = plugin_->start();
    
    EXPECT_FALSE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::ERROR);
    EXPECT_FALSE(plugin_->areDataRefsFound());
    EXPECT_LT(plugin_->getDataRefCount(), 6);
    EXPECT_FALSE(plugin_->getErrorMessages().empty());
    
    // Should contain error message about datarefs
    const auto& errors = plugin_->getErrorMessages();
    bool foundDataRefError = false;
    for (const auto& error : errors) {
        if (error.find("dataref") != std::string::npos) {
            foundDataRefError = true;
            break;
        }
    }
    EXPECT_TRUE(foundDataRefError);
}

// Test plugin restart capability
TEST_F(PluginLifecycleTest, PluginRestart) {
    // First lifecycle
    ASSERT_TRUE(plugin_->start());
    ASSERT_TRUE(plugin_->enable());
    plugin_->disable();
    plugin_->stop();
    
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::UNLOADED);
    
    // Second lifecycle
    bool result = plugin_->start();
    EXPECT_TRUE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::LOADED);
    
    result = plugin_->enable();
    EXPECT_TRUE(result);
    EXPECT_EQ(plugin_->getState(), plugin::PluginState::ENABLED);
}

// Test resource cleanup
TEST_F(PluginLifecycleTest, ResourceCleanup) {
    ASSERT_TRUE(plugin_->start());
    ASSERT_TRUE(plugin_->enable());
    
    // Verify resources are allocated
    EXPECT_TRUE(plugin_->isNetworkInitialized());
    EXPECT_TRUE(plugin_->isFlightLoopRegistered());
    EXPECT_TRUE(plugin_->areDataRefsFound());
    EXPECT_GT(plugin_->getDataRefCount(), 0);
    
    // Stop plugin
    plugin_->stop();
    
    // Verify resources are cleaned up
    EXPECT_FALSE(plugin_->isNetworkInitialized());
    EXPECT_FALSE(plugin_->isFlightLoopRegistered());
    EXPECT_FALSE(plugin_->areDataRefsFound());
    EXPECT_EQ(plugin_->getDataRefCount(), 0);
    EXPECT_TRUE(plugin_->getErrorMessages().empty());
}

// Test multiple enable/disable cycles
TEST_F(PluginLifecycleTest, MultipleEnableDisableCycles) {
    ASSERT_TRUE(plugin_->start());
    
    const int cycles = 5;
    
    for (int i = 0; i < cycles; i++) {
        // Enable
        bool result = plugin_->enable();
        EXPECT_TRUE(result) << "Enable failed on cycle " << i;
        EXPECT_EQ(plugin_->getState(), plugin::PluginState::ENABLED);
        EXPECT_TRUE(plugin_->isNetworkInitialized());
        EXPECT_TRUE(plugin_->isFlightLoopRegistered());
        
        // Disable
        plugin_->disable();
        EXPECT_EQ(plugin_->getState(), plugin::PluginState::DISABLED);
        EXPECT_FALSE(plugin_->isNetworkInitialized());
        EXPECT_FALSE(plugin_->isFlightLoopRegistered());
    }
}

// Test debug message generation
TEST_F(PluginLifecycleTest, DebugMessages) {
    XPLMockState::getInstance().clearDebugStrings();
    
    plugin_->start();
    plugin_->enable();
    plugin_->disable();
    plugin_->stop();
    
    std::vector<std::string> debugStrings = XPLMockState::getInstance().getDebugStrings();
    
    // Should have messages for each lifecycle event
    bool hasStarting = false, hasStarted = false;
    bool hasEnabling = false, hasEnabled = false;
    bool hasDisabling = false, hasDisabled = false;
    bool hasStopping = false, hasStopped = false;
    
    for (const auto& msg : debugStrings) {
        if (msg.find("starting") != std::string::npos) hasStarting = true;
        if (msg.find("started successfully") != std::string::npos) hasStarted = true;
        if (msg.find("enabling") != std::string::npos) hasEnabling = true;
        if (msg.find("enabled successfully") != std::string::npos) hasEnabled = true;
        if (msg.find("disabling") != std::string::npos) hasDisabling = true;
        if (msg.find("disabled") != std::string::npos) hasDisabled = true;
        if (msg.find("stopping") != std::string::npos) hasStopping = true;
        if (msg.find("stopped") != std::string::npos) hasStopped = true;
    }
    
    EXPECT_TRUE(hasStarting);
    EXPECT_TRUE(hasStarted);
    EXPECT_TRUE(hasEnabling);
    EXPECT_TRUE(hasEnabled);
    EXPECT_TRUE(hasDisabling);
    EXPECT_TRUE(hasDisabled);
    EXPECT_TRUE(hasStopping);
    EXPECT_TRUE(hasStopped);
}

// Performance test for lifecycle operations
TEST_F(PluginLifecycleTest, LifecyclePerformance) {
    const int iterations = 100;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        plugin_->start();
        plugin_->enable();
        plugin_->disable();
        plugin_->stop();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should complete lifecycle operations quickly
    EXPECT_LT(duration.count(), 1000); // Less than 1 second for 100 cycles
    
    std::cout << "Lifecycle performance: " << iterations << " full cycles in " 
              << duration.count() << " milliseconds" << std::endl;
}
