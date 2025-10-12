#ifndef GDL90_ENCODER_H
#define GDL90_ENCODER_H

#include <cstdint>
#include <vector>
#include <string>
#include <ctime>

/**
 * GDL90 Data Interface Encoder
 * Implements the Garmin GDL90 protocol for ADS-B data transmission
 * Based on GDL90 Data Interface Specification (560-1058-00 Rev A)
 */

namespace gdl90 {

// GDL90 Message IDs
constexpr uint8_t MSG_ID_HEARTBEAT = 0x00;
constexpr uint8_t MSG_ID_OWNSHIP_REPORT = 0x0A;
constexpr uint8_t MSG_ID_TRAFFIC_REPORT = 0x14;

// Special values
constexpr uint16_t ALTITUDE_INVALID = 0xFFF;
constexpr uint16_t VELOCITY_INVALID = 0xFFF;
constexpr uint16_t VVELOCITY_INVALID = 0x800;

// Address types (field 't')
enum class AddressType : uint8_t {
    ADSB_ICAO = 0,           // ADS-B with ICAO address
    ADSB_SELF_ASSIGNED = 1,  // ADS-B with Self-assigned address
    TISB_ICAO = 2,           // TIS-B with ICAO address
    TISB_TRACK_FILE = 3,     // TIS-B with track file ID
    SURFACE_VEHICLE = 4,     // Surface Vehicle
    GROUND_STATION = 5,      // Ground Station Beacon
};

// Emitter categories (Table 11)
enum class EmitterCategory : uint8_t {
    NO_INFO = 0,              // No aircraft type information
    LIGHT = 1,                // Light (< 15,500 lbs)
    SMALL = 2,                // Small (15,500 to 75,000 lbs)
    LARGE = 3,                // Large (75,000 to 300,000 lbs)
    HIGH_VORTEX_LARGE = 4,    // High Vortex Large
    HEAVY = 5,                // Heavy (> 300,000 lbs)
    HIGHLY_MANEUVERABLE = 6,  // Highly Maneuverable
    ROTORCRAFT = 7,           // Rotorcraft
    GLIDER = 9,               // Glider/sailplane
    LIGHTER_THAN_AIR = 10,    // Lighter than air
    PARACHUTIST = 11,         // Parachutist/sky diver
    ULTRA_LIGHT = 12,         // Ultra light/hang glider/paraglider
    UAV = 14,                 // Unmanned aerial vehicle
    SPACE_VEHICLE = 15,       // Space/transatmospheric vehicle
};

// Track/Heading type (Miscellaneous field bits 1-0)
enum class TrackType : uint8_t {
    INVALID = 0,         // Track/Heading not valid
    TRUE_TRACK = 1,      // True Track Angle
    MAG_HEADING = 2,     // Heading (Magnetic)
    TRUE_HEADING = 3,    // Heading (True)
};

// Position data structure
struct PositionData {
    double latitude;        // Degrees (-90 to +90, North positive)
    double longitude;       // Degrees (-180 to +180, East positive)
    int32_t altitude;       // Feet MSL
    uint16_t h_velocity;    // Knots
    int16_t v_velocity;     // Feet per minute
    uint16_t track;         // Degrees (0-359)
    TrackType track_type;   // Type of track/heading
    bool airborne;          // True if airborne, false if on ground
    uint8_t nic;            // Navigation Integrity Category (0-11)
    uint8_t nacp;           // Navigation Accuracy Category (0-11)
    uint32_t icao_address;  // 24-bit ICAO address
    std::string callsign;   // 8-character callsign
    EmitterCategory emitter_category;
    AddressType address_type;
    uint8_t alert_status;   // Traffic alert status (0=none, 1=alert)
    uint8_t emergency_code; // Emergency/Priority code (0=none)
};

class GDL90Encoder {
public:
    GDL90Encoder();
    ~GDL90Encoder() = default;

    /**
     * Create Heartbeat message (ID 0x00)
     * Sent once per second at the beginning of each UTC second
     * 
     * @param gps_valid GPS position is valid
     * @param utc_ok UTC timing is valid
     * @return Encoded message ready for transmission
     */
    std::vector<uint8_t> createHeartbeat(bool gps_valid = true, bool utc_ok = true);

    /**
     * Create Ownship Report message (ID 0x0A)
     * Reports own aircraft position and status
     * 
     * @param data Position and status data
     * @return Encoded message ready for transmission
     */
    std::vector<uint8_t> createOwnshipReport(const PositionData& data);

    /**
     * Create Traffic Report message (ID 0x14)
     * Reports another aircraft's position and status
     * 
     * @param data Position and status data for traffic
     * @return Encoded message ready for transmission
     */
    std::vector<uint8_t> createTrafficReport(const PositionData& data);

private:
    // CRC-16-CCITT lookup table
    static const uint16_t crc16_table_[256];

    /**
     * Calculate CRC-16-CCITT checksum
     */
    uint16_t calculateCRC(const std::vector<uint8_t>& data) const;

    /**
     * Escape special characters (0x7D and 0x7E)
     */
    std::vector<uint8_t> escapeMessage(const std::vector<uint8_t>& data) const;

    /**
     * Prepare message for transmission:
     * 1. Calculate and append CRC
     * 2. Escape special characters
     * 3. Add flag bytes (0x7E)
     */
    std::vector<uint8_t> prepareMessage(const std::vector<uint8_t>& payload) const;

    /**
     * Encode latitude to 24-bit value
     * Resolution: 180 / 2^23 degrees
     */
    uint32_t encodeLatitude(double latitude) const;

    /**
     * Encode longitude to 24-bit value
     * Resolution: 180 / 2^23 degrees
     */
    uint32_t encodeLongitude(double longitude) const;

    /**
     * Encode altitude to 12-bit value
     * Resolution: 25 feet, offset by -1000 feet
     */
    uint16_t encodeAltitude(int32_t altitude) const;

    /**
     * Encode vertical velocity to 12-bit value
     * Resolution: 64 feet per minute, signed
     */
    uint16_t encodeVerticalVelocity(int16_t vv_fpm) const;

    /**
     * Encode track/heading to 8-bit value
     * Resolution: 360/256 degrees (1.4 degrees)
     */
    uint8_t encodeTrack(uint16_t track) const;

    /**
     * Pack 24-bit value to 3 bytes (big-endian)
     */
    void pack24bit(std::vector<uint8_t>& buffer, uint32_t value) const;

    /**
     * Create position report (common for ownship and traffic)
     */
    std::vector<uint8_t> createPositionReport(uint8_t msg_id, const PositionData& data);

    /**
     * Get current UTC time in seconds since midnight
     */
    uint32_t getUTCTime() const;
};

} // namespace gdl90

#endif // GDL90_ENCODER_H

