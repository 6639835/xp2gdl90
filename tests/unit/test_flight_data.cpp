/**
 * XP2GDL90 Unit Tests - Flight Data Tests
 * 
 * Tests for flight data reading and processing from X-Plane datarefs
 */

#include <gtest/gtest.h>
#include "../mocks/XPLMMock.h"
#include <vector>
#include <memory>

// Mock flight data structures (would be in main plugin)
namespace flightdata {
    
    struct AircraftState {
        double latitude;
        double longitude;
        double elevation;
        float groundSpeed;
        float heading;
        float verticalSpeed;
        bool onGround;
        bool enginesRunning;
        uint32_t icaoAddress;
    };
    
    struct TrafficTarget {
        uint32_t icaoAddress;
        double latitude;
        double longitude;
        double elevation;
        bool valid;
    };
    
    class FlightDataReader {
    private:
        // DataRef handles (in real implementation)
        XPLMDataRef latRef_;
        XPLMDataRef lonRef_;
        XPLMDataRef elevRef_;
        XPLMDataRef speedRef_;
        XPLMDataRef headingRef_;
        XPLMDataRef vertSpeedRef_;
        XPLMDataRef onGroundRef_;
        XPLMDataRef enginesRunningRef_;
        XPLMDataRef tcasLatRef_;
        XPLMDataRef tcasLonRef_;
        XPLMDataRef tcasElevRef_;
        XPLMDataRef tcasCountRef_;
        
    public:
        FlightDataReader() {
            // Initialize dataref handles
            latRef_ = XPLMFindDataRef("sim/flightmodel/position/latitude");
            lonRef_ = XPLMFindDataRef("sim/flightmodel/position/longitude");
            elevRef_ = XPLMFindDataRef("sim/flightmodel/position/elevation");
            speedRef_ = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
            headingRef_ = XPLMFindDataRef("sim/flightmodel/position/psi");
            vertSpeedRef_ = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm");
            onGroundRef_ = XPLMFindDataRef("sim/flightmodel/failures/onground_any");
            enginesRunningRef_ = XPLMFindDataRef("sim/aircraft/engine/engn_running");
            tcasLatRef_ = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/lat");
            tcasLonRef_ = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/lon");
            tcasElevRef_ = XPLMFindDataRef("sim/cockpit2/tcas/targets/position/ele");
            tcasCountRef_ = XPLMFindDataRef("sim/cockpit2/tcas/num_acf");
        }
        
        bool isValid() const {
            return latRef_ != nullptr && lonRef_ != nullptr && elevRef_ != nullptr;
        }
        
        AircraftState readOwnshipData() {
            AircraftState state = {};
            
            if (!isValid()) return state;
            
            state.latitude = XPLMGetDatad(latRef_);
            state.longitude = XPLMGetDatad(lonRef_);
            state.elevation = XPLMGetDataf(elevRef_);
            state.groundSpeed = XPLMGetDataf(speedRef_);
            state.heading = XPLMGetDataf(headingRef_);
            state.verticalSpeed = XPLMGetDataf(vertSpeedRef_);
            state.onGround = XPLMGetDatai(onGroundRef_) != 0;
            
            // Check engine status (simplified - just check first engine)
            int engineRunning = 0;
            XPLMGetDatavi(enginesRunningRef_, &engineRunning, 0, 1);
            state.enginesRunning = engineRunning != 0;
            
            state.icaoAddress = 0xABCDEF; // Default test ICAO
            
            return state;
        }
        
        std::vector<TrafficTarget> readTrafficData() {
            std::vector<TrafficTarget> targets;
            
            if (!tcasCountRef_ || !tcasLatRef_ || !tcasLonRef_ || !tcasElevRef_) {
                return targets;
            }
            
            int numTargets = XPLMGetDatai(tcasCountRef_);
            if (numTargets <= 0) return targets;
            
            // Limit to reasonable number
            const int maxTargets = 63; // GDL-90 limit
            numTargets = std::min(numTargets, maxTargets);
            
            std::vector<float> latitudes(numTargets);
            std::vector<float> longitudes(numTargets);
            std::vector<float> elevations(numTargets);
            
            int actualLats = XPLMGetDatavf(tcasLatRef_, latitudes.data(), 0, numTargets);
            int actualLons = XPLMGetDatavf(tcasLonRef_, longitudes.data(), 0, numTargets);
            int actualElevs = XPLMGetDatavf(tcasElevRef_, elevations.data(), 0, numTargets);
            
            int actualCount = std::min({actualLats, actualLons, actualElevs});
            
            for (int i = 0; i < actualCount; i++) {
                TrafficTarget target;
                target.latitude = latitudes[i];
                target.longitude = longitudes[i];
                target.elevation = elevations[i];
                target.icaoAddress = 0x100000 + i; // Generate mock ICAO addresses
                target.valid = (latitudes[i] != 0.0f || longitudes[i] != 0.0f);
                
                if (target.valid) {
                    targets.push_back(target);
                }
            }
            
            return targets;
        }
        
        bool isDataValid(const AircraftState& state) const {
            // Basic validity checks
            if (state.latitude < -90.0 || state.latitude > 90.0) return false;
            if (state.longitude < -180.0 || state.longitude > 180.0) return false;
            if (state.elevation < -1000.0 || state.elevation > 100000.0) return false;
            if (state.groundSpeed < 0.0 || state.groundSpeed > 1000.0) return false;
            if (state.heading < 0.0 || state.heading >= 360.0) return false;
            
            return true;
        }
        
        static double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
            // Haversine formula for distance calculation
            const double R = 6371000; // Earth radius in meters
            const double PI = 3.14159265359;
            
            double dLat = (lat2 - lat1) * PI / 180.0;
            double dLon = (lon2 - lon1) * PI / 180.0;
            
            double a = sin(dLat/2) * sin(dLat/2) +
                      cos(lat1 * PI / 180.0) * cos(lat2 * PI / 180.0) *
                      sin(dLon/2) * sin(dLon/2);
            double c = 2 * atan2(sqrt(a), sqrt(1-a));
            
            return R * c;
        }
    };
}

// Test fixture for flight data tests
class FlightDataTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset mock state before each test
        XPLMockState::getInstance().reset();
        
        // Set up realistic flight data
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", 37.524);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", -122.063);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/elevation", 100.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/groundspeed", 25.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/psi", 90.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/vh_ind_fpm", 0.0f);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/failures/onground_any", 0);
        
        // Set up engine data
        std::vector<int> engines = {1, 0, 0, 0}; // First engine running
        XPLMockState::getInstance().setDataRefValue("sim/aircraft/engine/engn_running", engines);
        
        reader_ = std::make_unique<flightdata::FlightDataReader>();
    }
    
    void TearDown() override {
        reader_.reset();
    }
    
    std::unique_ptr<flightdata::FlightDataReader> reader_;
};

// Test flight data reader initialization
TEST_F(FlightDataTest, ReaderInitialization) {
    EXPECT_TRUE(reader_->isValid());
}

// Test ownship data reading
TEST_F(FlightDataTest, ReadOwnshipData) {
    flightdata::AircraftState state = reader_->readOwnshipData();
    
    EXPECT_DOUBLE_EQ(state.latitude, 37.524);
    EXPECT_DOUBLE_EQ(state.longitude, -122.063);
    EXPECT_FLOAT_EQ(state.elevation, 100.0f);
    EXPECT_FLOAT_EQ(state.groundSpeed, 25.0f);
    EXPECT_FLOAT_EQ(state.heading, 90.0f);
    EXPECT_FLOAT_EQ(state.verticalSpeed, 0.0f);
    EXPECT_FALSE(state.onGround);
    EXPECT_TRUE(state.enginesRunning);
    EXPECT_EQ(state.icaoAddress, 0xABCDEF);
}

// Test data validation
TEST_F(FlightDataTest, DataValidation) {
    flightdata::AircraftState validState = reader_->readOwnshipData();
    EXPECT_TRUE(reader_->isDataValid(validState));
    
    // Test invalid latitude
    flightdata::AircraftState invalidState = validState;
    invalidState.latitude = 91.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    invalidState.latitude = -91.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    // Test invalid longitude
    invalidState = validState;
    invalidState.longitude = 181.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    invalidState.longitude = -181.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    // Test invalid elevation
    invalidState = validState;
    invalidState.elevation = -2000.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    invalidState.elevation = 200000.0;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    // Test invalid ground speed
    invalidState = validState;
    invalidState.groundSpeed = -10.0f;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
    
    invalidState.groundSpeed = 2000.0f;
    EXPECT_FALSE(reader_->isDataValid(invalidState));
}

// Test traffic data reading
TEST_F(FlightDataTest, ReadTrafficData) {
    // Set up mock traffic data
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", 2);
    
    std::vector<float> trafficLats = {37.525, 37.526};
    std::vector<float> trafficLons = {-122.064, -122.065};
    std::vector<float> trafficElevs = {200.0f, 300.0f};
    
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lat", trafficLats);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lon", trafficLons);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/ele", trafficElevs);
    
    std::vector<flightdata::TrafficTarget> targets = reader_->readTrafficData();
    
    EXPECT_EQ(targets.size(), 2);
    
    if (targets.size() >= 2) {
        EXPECT_DOUBLE_EQ(targets[0].latitude, 37.525);
        EXPECT_DOUBLE_EQ(targets[0].longitude, -122.064);
        EXPECT_DOUBLE_EQ(targets[0].elevation, 200.0);
        EXPECT_TRUE(targets[0].valid);
        EXPECT_EQ(targets[0].icaoAddress, 0x100000);
        
        EXPECT_DOUBLE_EQ(targets[1].latitude, 37.526);
        EXPECT_DOUBLE_EQ(targets[1].longitude, -122.065);
        EXPECT_DOUBLE_EQ(targets[1].elevation, 300.0);
        EXPECT_TRUE(targets[1].valid);
        EXPECT_EQ(targets[1].icaoAddress, 0x100001);
    }
}

// Test no traffic data
TEST_F(FlightDataTest, NoTrafficData) {
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", 0);
    
    std::vector<flightdata::TrafficTarget> targets = reader_->readTrafficData();
    EXPECT_EQ(targets.size(), 0);
}

// Test ground state detection
TEST_F(FlightDataTest, GroundStateDetection) {
    // Test on ground
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/failures/onground_any", 1);
    
    flightdata::AircraftState state = reader_->readOwnshipData();
    EXPECT_TRUE(state.onGround);
    
    // Test in air
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/failures/onground_any", 0);
    
    state = reader_->readOwnshipData();
    EXPECT_FALSE(state.onGround);
}

// Test engine state detection
TEST_F(FlightDataTest, EngineStateDetection) {
    // Test engines running
    std::vector<int> enginesOn = {1, 1, 0, 0};
    XPLMockState::getInstance().setDataRefValue("sim/aircraft/engine/engn_running", enginesOn);
    
    flightdata::AircraftState state = reader_->readOwnshipData();
    EXPECT_TRUE(state.enginesRunning);
    
    // Test engines off
    std::vector<int> enginesOff = {0, 0, 0, 0};
    XPLMockState::getInstance().setDataRefValue("sim/aircraft/engine/engn_running", enginesOff);
    
    state = reader_->readOwnshipData();
    EXPECT_FALSE(state.enginesRunning);
}

// Test distance calculation utility
TEST_F(FlightDataTest, DistanceCalculation) {
    // Test known distance (approximately)
    double lat1 = 37.524;
    double lon1 = -122.063;
    double lat2 = 37.525;
    double lon2 = -122.064;
    
    double distance = flightdata::FlightDataReader::calculateDistance(lat1, lon1, lat2, lon2);
    
    // Should be approximately 140 meters (rough calculation)
    EXPECT_GT(distance, 100.0);
    EXPECT_LT(distance, 200.0);
    
    // Test zero distance
    distance = flightdata::FlightDataReader::calculateDistance(lat1, lon1, lat1, lon1);
    EXPECT_NEAR(distance, 0.0, 0.001);
}

// Test coordinate precision
TEST_F(FlightDataTest, CoordinatePrecision) {
    // Test high precision coordinates
    double highPrecLat = 37.52400123456789;
    double highPrecLon = -122.06300987654321;
    
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", highPrecLat);
    XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", highPrecLon);
    
    flightdata::AircraftState state = reader_->readOwnshipData();
    
    // Should maintain reasonable precision
    EXPECT_NEAR(state.latitude, highPrecLat, 0.000001);
    EXPECT_NEAR(state.longitude, highPrecLon, 0.000001);
}

// Test rapid data updates
TEST_F(FlightDataTest, RapidDataUpdates) {
    const int numUpdates = 1000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numUpdates; i++) {
        // Simulate changing position
        double lat = 37.524 + (i * 0.0001);
        double lon = -122.063 + (i * 0.0001);
        
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/latitude", lat);
        XPLMockState::getInstance().setDataRefValue("sim/flightmodel/position/longitude", lon);
        
        flightdata::AircraftState state = reader_->readOwnshipData();
        
        EXPECT_NEAR(state.latitude, lat, 0.000001);
        EXPECT_NEAR(state.longitude, lon, 0.000001);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete quickly (less than 100ms for 1000 updates)
    EXPECT_LT(duration.count(), 100000); // 100ms in microseconds
    
    std::cout << "Data update performance: " << numUpdates << " updates in " 
              << duration.count() << " microseconds" << std::endl;
}

// Test large number of traffic targets
TEST_F(FlightDataTest, LargeTrafficCount) {
    const int maxTargets = 63; // GDL-90 limit
    
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/num_acf", maxTargets);
    
    std::vector<float> trafficLats(maxTargets);
    std::vector<float> trafficLons(maxTargets);
    std::vector<float> trafficElevs(maxTargets);
    
    for (int i = 0; i < maxTargets; i++) {
        trafficLats[i] = 37.524f + (i * 0.001f);
        trafficLons[i] = -122.063f + (i * 0.001f);
        trafficElevs[i] = 1000.0f + (i * 100.0f);
    }
    
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lat", trafficLats);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/lon", trafficLons);
    XPLMockState::getInstance().setDataRefValue("sim/cockpit2/tcas/targets/position/ele", trafficElevs);
    
    std::vector<flightdata::TrafficTarget> targets = reader_->readTrafficData();
    
    EXPECT_EQ(targets.size(), maxTargets);
    
    // Verify all targets are valid and unique
    for (size_t i = 0; i < targets.size(); i++) {
        EXPECT_TRUE(targets[i].valid);
        EXPECT_EQ(targets[i].icaoAddress, 0x100000 + i);
        EXPECT_NEAR(targets[i].latitude, 37.524 + (i * 0.001), 0.0001);
        EXPECT_NEAR(targets[i].longitude, -122.063 + (i * 0.001), 0.0001);
        EXPECT_NEAR(targets[i].elevation, 1000.0 + (i * 100.0), 1.0);
    }
}
