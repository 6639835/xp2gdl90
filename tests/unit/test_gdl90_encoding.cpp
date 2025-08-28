/**
 * XP2GDL90 Unit Tests - GDL-90 Encoding Tests
 * 
 * Tests for GDL-90 message encoding and formatting
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>
#include <cstring>

// Mock GDL-90 encoding functions (would normally be in main plugin code)
namespace gdl90 {
    
    // GDL-90 message types
    const uint8_t MSG_HEARTBEAT = 0x00;
    const uint8_t MSG_OWNSHIP_REPORT = 0x0A;
    const uint8_t MSG_TRAFFIC_REPORT = 0x14;
    
    // GDL-90 frame markers
    const uint8_t FLAG_BYTE = 0x7E;
    const uint8_t ESCAPE_BYTE = 0x7D;
    
    struct Position {
        double latitude;    // degrees
        double longitude;   // degrees
        int32_t altitude;   // feet
        float groundSpeed;  // knots
        float track;        // degrees
        int16_t verticalVelocity; // feet/minute
    };
    
    struct TrafficTarget {
        uint32_t icaoAddress;
        Position position;
        uint8_t alertStatus;
        uint8_t addressType;
        uint8_t participantAddress;
    };
    
    // CRC calculation for GDL-90
    uint16_t calculateCRC(const uint8_t* data, size_t length) {
        uint16_t crc = 0;
        for (size_t i = 0; i < length; i++) {
            crc ^= (data[i] << 8);
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc = crc << 1;
                }
            }
        }
        return crc;
    }
    
    // Encode 24-bit signed latitude/longitude
    int32_t encodeCoordinate(double degrees) {
        // GDL-90 uses 24-bit signed integers with 180/2^23 resolution
        const double resolution = 180.0 / (1 << 23);
        int32_t encoded = static_cast<int32_t>(degrees / resolution);
        
        // Clamp to 24-bit signed range
        const int32_t max_24bit = (1 << 23) - 1;
        const int32_t min_24bit = -(1 << 23);
        
        if (encoded > max_24bit) encoded = max_24bit;
        if (encoded < min_24bit) encoded = min_24bit;
        
        return encoded & 0x00FFFFFF; // Mask to 24 bits
    }
    
    // Encode altitude in 12-bit format
    uint16_t encodeAltitude(int32_t altitudeFeet) {
        // GDL-90 altitude encoding: 25-foot resolution, offset by -1000 feet
        int32_t encoded = (altitudeFeet + 1000) / 25;
        
        // Clamp to 12-bit range
        if (encoded < 0) encoded = 0;
        if (encoded > 0xFFE) encoded = 0xFFE; // 0xFFF is reserved for invalid
        
        return static_cast<uint16_t>(encoded);
    }
    
    // Create heartbeat message
    std::vector<uint8_t> createHeartbeat(uint32_t timestamp) {
        std::vector<uint8_t> message;
        
        // Message ID
        message.push_back(MSG_HEARTBEAT);
        
        // Status byte (bit 0 = GPS position valid, bit 1 = maintenance required)
        message.push_back(0x01); // GPS valid
        
        // Timestamp (seconds since 0000Z)
        message.push_back((timestamp >> 16) & 0xFF);
        message.push_back((timestamp >> 8) & 0xFF);
        message.push_back(timestamp & 0xFF);
        
        // Message count (incremental counter)
        message.push_back(0x00);
        message.push_back(0x01);
        
        return message;
    }
    
    // Create ownship position report
    std::vector<uint8_t> createOwnshipReport(const Position& pos, uint32_t icaoAddress) {
        std::vector<uint8_t> message;
        
        // Message ID
        message.push_back(MSG_OWNSHIP_REPORT);
        
        // Alert status and address type (bits 4-7: alert, bits 0-3: address type)
        message.push_back(0x00); // No alert, ICAO address
        
        // Participant address (24-bit ICAO)
        message.push_back((icaoAddress >> 16) & 0xFF);
        message.push_back((icaoAddress >> 8) & 0xFF);
        message.push_back(icaoAddress & 0xFF);
        
        // Latitude (24-bit signed)
        int32_t lat = encodeCoordinate(pos.latitude);
        message.push_back((lat >> 16) & 0xFF);
        message.push_back((lat >> 8) & 0xFF);
        message.push_back(lat & 0xFF);
        
        // Longitude (24-bit signed)
        int32_t lon = encodeCoordinate(pos.longitude);
        message.push_back((lon >> 16) & 0xFF);
        message.push_back((lon >> 8) & 0xFF);
        message.push_back(lon & 0xFF);
        
        // Altitude (12-bit) + Navigation Integrity Category (4-bit)
        uint16_t alt = encodeAltitude(pos.altitude);
        message.push_back((alt >> 4) & 0xFF);
        message.push_back(((alt & 0x0F) << 4) | 0x0A); // NIC = 10
        
        // Navigation Accuracy Category + Horizontal containment radius
        message.push_back(0xA0); // NAC = 10, horizontal containment = 0
        
        // Vertical containment + Speed/Track validity
        message.push_back(0x00);
        
        // Ground speed (12-bit)
        uint16_t speed = static_cast<uint16_t>(pos.groundSpeed);
        if (speed > 0xFFE) speed = 0xFFE;
        message.push_back((speed >> 4) & 0xFF);
        
        // Track angle (8-bit) + speed LSBs
        uint8_t track = static_cast<uint8_t>(pos.track * 256.0f / 360.0f);
        message.push_back(((speed & 0x0F) << 4) | ((track >> 4) & 0x0F));
        message.push_back((track & 0x0F) << 4);
        
        // Vertical velocity (12-bit signed)
        int16_t vv = pos.verticalVelocity / 64; // 64 fpm resolution
        if (vv > 0x1FF) vv = 0x1FF;
        if (vv < -0x200) vv = -0x200;
        message.push_back((vv >> 4) & 0xFF);
        message.push_back((vv & 0x0F) << 4);
        
        // Reserved bytes
        message.push_back(0x00);
        message.push_back(0x00);
        
        return message;
    }
    
    // Frame message with flag bytes and CRC
    std::vector<uint8_t> frameMessage(const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> framed;
        
        // Calculate CRC
        uint16_t crc = calculateCRC(payload.data(), payload.size());
        
        // Create complete message (payload + CRC)
        std::vector<uint8_t> complete = payload;
        complete.push_back((crc >> 8) & 0xFF);
        complete.push_back(crc & 0xFF);
        
        // Add opening flag
        framed.push_back(FLAG_BYTE);
        
        // Escape payload and CRC
        for (uint8_t byte : complete) {
            if (byte == FLAG_BYTE || byte == ESCAPE_BYTE) {
                framed.push_back(ESCAPE_BYTE);
                framed.push_back(byte ^ 0x20);
            } else {
                framed.push_back(byte);
            }
        }
        
        // Add closing flag
        framed.push_back(FLAG_BYTE);
        
        return framed;
    }
}

// Test fixture for GDL-90 encoding
class GDL90EncodingTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test data
        testPosition.latitude = 37.524;
        testPosition.longitude = -122.063;
        testPosition.altitude = 100;
        testPosition.groundSpeed = 0.0f;
        testPosition.track = 90.0f;
        testPosition.verticalVelocity = 0;
        
        testIcaoAddress = 0xABCDEF;
    }
    
    gdl90::Position testPosition;
    uint32_t testIcaoAddress;
};

// Test coordinate encoding
TEST_F(GDL90EncodingTest, CoordinateEncoding) {
    // Test known coordinate values
    EXPECT_EQ(gdl90::encodeCoordinate(0.0), 0x000000);
    EXPECT_EQ(gdl90::encodeCoordinate(90.0), 0x400000);
    EXPECT_EQ(gdl90::encodeCoordinate(-90.0), 0xC00000);
    EXPECT_EQ(gdl90::encodeCoordinate(180.0), 0x7FFFFF); // Max positive (clamped)
    EXPECT_EQ(gdl90::encodeCoordinate(-180.0), 0x800000); // Max negative (clamped)
    
    // Test precision
    double testLat = 37.524;
    int32_t encoded = gdl90::encodeCoordinate(testLat);
    double decoded = (static_cast<int32_t>(encoded << 8) >> 8) * (180.0 / (1 << 23));
    EXPECT_NEAR(decoded, testLat, 0.000001); // Should be very close
}

// Test altitude encoding
TEST_F(GDL90EncodingTest, AltitudeEncoding) {
    EXPECT_EQ(gdl90::encodeAltitude(-1000), 0x000); // Minimum
    EXPECT_EQ(gdl90::encodeAltitude(0), 40);        // Sea level
    EXPECT_EQ(gdl90::encodeAltitude(100), 44);      // 100 feet
    EXPECT_EQ(gdl90::encodeAltitude(1000), 80);     // 1000 feet
    EXPECT_EQ(gdl90::encodeAltitude(50000), 0xFFE); // High altitude (clamped)
}

// Test heartbeat message creation
TEST_F(GDL90EncodingTest, HeartbeatMessage) {
    uint32_t timestamp = 3661; // 01:01:01 UTC
    std::vector<uint8_t> message = gdl90::createHeartbeat(timestamp);
    
    EXPECT_EQ(message.size(), 7);
    EXPECT_EQ(message[0], gdl90::MSG_HEARTBEAT);
    EXPECT_EQ(message[1], 0x01); // GPS valid
    
    // Check timestamp bytes
    EXPECT_EQ(message[2], 0x00); // High byte
    EXPECT_EQ(message[3], 0x0E); // Middle byte
    EXPECT_EQ(message[4], 0x4D); // Low byte
}

// Test ownship report creation
TEST_F(GDL90EncodingTest, OwnshipReport) {
    std::vector<uint8_t> message = gdl90::createOwnshipReport(testPosition, testIcaoAddress);
    
    EXPECT_EQ(message.size(), 28); // Standard ownship report size
    EXPECT_EQ(message[0], gdl90::MSG_OWNSHIP_REPORT);
    
    // Check ICAO address
    EXPECT_EQ(message[2], 0xAB);
    EXPECT_EQ(message[3], 0xCD);
    EXPECT_EQ(message[4], 0xEF);
    
    // Verify position encoding
    int32_t encodedLat = (message[5] << 16) | (message[6] << 8) | message[7];
    int32_t encodedLon = (message[8] << 16) | (message[9] << 8) | message[10];
    
    // Convert back to check
    double decodedLat = (static_cast<int32_t>(encodedLat << 8) >> 8) * (180.0 / (1 << 23));
    double decodedLon = (static_cast<int32_t>(encodedLon << 8) >> 8) * (180.0 / (1 << 23));
    
    EXPECT_NEAR(decodedLat, testPosition.latitude, 0.000001);
    EXPECT_NEAR(decodedLon, testPosition.longitude, 0.000001);
}

// Test CRC calculation
TEST_F(GDL90EncodingTest, CRCCalculation) {
    std::vector<uint8_t> testData = {0x00, 0x01, 0x02, 0x03};
    uint16_t crc = gdl90::calculateCRC(testData.data(), testData.size());
    
    // CRC should be deterministic
    EXPECT_EQ(crc, gdl90::calculateCRC(testData.data(), testData.size()));
    
    // Different data should produce different CRC
    std::vector<uint8_t> differentData = {0x00, 0x01, 0x02, 0x04};
    uint16_t differentCrc = gdl90::calculateCRC(differentData.data(), differentData.size());
    EXPECT_NE(crc, differentCrc);
}

// Test message framing
TEST_F(GDL90EncodingTest, MessageFraming) {
    std::vector<uint8_t> payload = {gdl90::MSG_HEARTBEAT, 0x01, 0x00, 0x0E, 0x4D, 0x00, 0x01};
    std::vector<uint8_t> framed = gdl90::frameMessage(payload);
    
    // Should start and end with flag bytes
    EXPECT_EQ(framed.front(), gdl90::FLAG_BYTE);
    EXPECT_EQ(framed.back(), gdl90::FLAG_BYTE);
    
    // Should be longer than payload (flag bytes + CRC)
    EXPECT_GT(framed.size(), payload.size() + 2);
}

// Test escape byte handling
TEST_F(GDL90EncodingTest, EscapeBytes) {
    // Create payload with flag and escape bytes
    std::vector<uint8_t> payload = {gdl90::FLAG_BYTE, gdl90::ESCAPE_BYTE, 0x01};
    std::vector<uint8_t> framed = gdl90::frameMessage(payload);
    
    // Count escape sequences
    int escapeCount = 0;
    for (size_t i = 1; i < framed.size() - 1; i++) { // Skip flag bytes
        if (framed[i] == gdl90::ESCAPE_BYTE) {
            escapeCount++;
        }
    }
    
    // Should have at least 2 escape sequences (for the original flag and escape bytes)
    EXPECT_GE(escapeCount, 2);
}

// Test traffic report creation
TEST_F(GDL90EncodingTest, TrafficReport) {
    gdl90::TrafficTarget traffic;
    traffic.icaoAddress = 0x123456;
    traffic.position = testPosition;
    traffic.position.altitude = 2000; // Different altitude from ownship
    traffic.alertStatus = 0x01; // Traffic alert
    traffic.addressType = 0x00; // ICAO address
    
    // For now, we'll test that the structure can be created
    // (Full implementation would be in main plugin code)
    EXPECT_EQ(traffic.icaoAddress, 0x123456);
    EXPECT_EQ(traffic.position.altitude, 2000);
    EXPECT_EQ(traffic.alertStatus, 0x01);
}

// Performance test for encoding operations
TEST_F(GDL90EncodingTest, EncodingPerformance) {
    const int iterations = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        // Test coordinate encoding performance
        gdl90::encodeCoordinate(testPosition.latitude + i * 0.0001);
        gdl90::encodeCoordinate(testPosition.longitude + i * 0.0001);
        
        // Test message creation performance
        std::vector<uint8_t> message = gdl90::createOwnshipReport(testPosition, testIcaoAddress);
        std::vector<uint8_t> framed = gdl90::frameMessage(message);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Should complete in reasonable time (less than 1 second for 10k iterations)
    EXPECT_LT(duration.count(), 1000000); // 1 second in microseconds
    
    std::cout << "Encoding performance: " << iterations << " iterations in " 
              << duration.count() << " microseconds" << std::endl;
}
