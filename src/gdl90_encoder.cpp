#include "gdl90_encoder.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace gdl90 {

// GDL90 CRC-16-CCITT lookup table (same as Python version)
const uint16_t GDL90Encoder::crc16_table_[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

GDL90Encoder::GDL90Encoder() {
}

uint16_t GDL90Encoder::calculateCRC(const std::vector<uint8_t>& data) const {
    uint16_t crc = 0;
    for (uint8_t byte : data) {
        crc = crc16_table_[crc >> 8] ^ (crc << 8) ^ byte;
    }
    return crc;
}

std::vector<uint8_t> GDL90Encoder::escapeMessage(const std::vector<uint8_t>& data) const {
    std::vector<uint8_t> escaped;
    escaped.reserve(data.size() + 10); // Reserve extra space for escapes
    
    for (uint8_t byte : data) {
        if (byte == 0x7D || byte == 0x7E) {
            escaped.push_back(0x7D);      // Escape character
            escaped.push_back(byte ^ 0x20); // XOR with 0x20
        } else {
            escaped.push_back(byte);
        }
    }
    
    return escaped;
}

std::vector<uint8_t> GDL90Encoder::prepareMessage(const std::vector<uint8_t>& payload) const {
    // Calculate CRC
    uint16_t crc = calculateCRC(payload);
    
    // Append CRC (little-endian: LSB first)
    std::vector<uint8_t> msg_with_crc = payload;
    msg_with_crc.push_back(crc & 0xFF);         // LSB
    msg_with_crc.push_back((crc >> 8) & 0xFF);  // MSB
    
    // Escape special characters
    std::vector<uint8_t> escaped = escapeMessage(msg_with_crc);
    
    // Add flag bytes
    std::vector<uint8_t> final_msg;
    final_msg.reserve(escaped.size() + 2);
    final_msg.push_back(0x7E);  // Start flag
    final_msg.insert(final_msg.end(), escaped.begin(), escaped.end());
    final_msg.push_back(0x7E);  // End flag
    
    return final_msg;
}

uint32_t GDL90Encoder::encodeLatitude(double latitude) const {
    // Clamp to valid range
    latitude = std::max(-90.0, std::min(90.0, latitude));
    
    // Convert to 24-bit signed value
    // Resolution: 180 / 2^23 degrees
    int32_t value = static_cast<int32_t>(latitude * (0x800000 / 180.0));
    
    // Convert to 2's complement if negative
    if (value < 0) {
        value = (0x1000000 + value) & 0xFFFFFF;
    }
    
    return static_cast<uint32_t>(value);
}

uint32_t GDL90Encoder::encodeLongitude(double longitude) const {
    // Clamp to valid range
    longitude = std::max(-180.0, std::min(180.0, longitude));
    
    // Convert to 24-bit signed value
    int32_t value = static_cast<int32_t>(longitude * (0x800000 / 180.0));
    
    // Convert to 2's complement if negative
    if (value < 0) {
        value = (0x1000000 + value) & 0xFFFFFF;
    }
    
    return static_cast<uint32_t>(value);
}

uint16_t GDL90Encoder::encodeAltitude(int32_t altitude) const {
    // Altitude in 25 feet increments, offset by +1000 feet
    int32_t encoded = (altitude + 1000) / 25;
    
    // Clamp to valid range
    if (encoded < 0) encoded = 0;
    if (encoded > 0xFFE) encoded = 0xFFE;
    
    return static_cast<uint16_t>(encoded);
}

uint16_t GDL90Encoder::encodeVerticalVelocity(int16_t vv_fpm) const {
    // Special case: invalid
    if (vv_fpm == INT16_MIN) {
        return VVELOCITY_INVALID;
    }
    
    // Clamp to valid range
    if (vv_fpm > 32576) return 0x1FE;
    if (vv_fpm < -32576) return 0xE02;
    
    // Convert to 64 fpm increments
    int16_t value = vv_fpm / 64;
    
    // Convert to 2's complement if negative
    if (value < 0) {
        value = (0x1000 + value) & 0xFFF;
    }
    
    return static_cast<uint16_t>(value);
}

uint8_t GDL90Encoder::encodeTrack(uint16_t track) const {
    // Convert degrees to 360/256 resolution
    return static_cast<uint8_t>((track % 360) * 256 / 360);
}

void GDL90Encoder::pack24bit(std::vector<uint8_t>& buffer, uint32_t value) const {
    // Big-endian (MSB first)
    buffer.push_back((value >> 16) & 0xFF);
    buffer.push_back((value >> 8) & 0xFF);
    buffer.push_back(value & 0xFF);
}

uint32_t GDL90Encoder::getUTCTime() const {
    std::time_t now = std::time(nullptr);
    std::tm* utc = std::gmtime(&now);
    return (utc->tm_hour * 3600) + (utc->tm_min * 60) + utc->tm_sec;
}

std::vector<uint8_t> GDL90Encoder::createHeartbeat(bool gps_valid, bool utc_ok) {
    std::vector<uint8_t> payload;
    
    // Message ID
    payload.push_back(MSG_ID_HEARTBEAT);
    
    // Status Byte 1
    uint8_t status1 = 0x01;  // UAT Initialized
    if (gps_valid) {
        status1 |= 0x80;  // GPS Position Valid
    }
    payload.push_back(status1);
    
    // Status Byte 2
    uint32_t timestamp = getUTCTime();
    uint8_t status2 = 0x00;
    if (utc_ok) {
        status2 |= 0x01;  // UTC OK
    }
    // Bit 16 of timestamp (MSB)
    if (timestamp & 0x10000) {
        status2 |= 0x80;
    }
    payload.push_back(status2);
    
    // Timestamp (bits 15-0, little-endian)
    payload.push_back(timestamp & 0xFF);         // LSB
    payload.push_back((timestamp >> 8) & 0xFF);  // MSB
    
    // Message Counts (2 bytes) - simplified, no actual counting
    payload.push_back(0x00);
    payload.push_back(0x00);
    
    return prepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createPositionReport(uint8_t msg_id, const PositionData& data) {
    std::vector<uint8_t> payload;
    
    // Message ID
    payload.push_back(msg_id);
    
    // Status and Address Type (st field)
    uint8_t st = ((data.alert_status & 0x0F) << 4) | (static_cast<uint8_t>(data.address_type) & 0x0F);
    payload.push_back(st);
    
    // ICAO Address (24-bit, big-endian)
    pack24bit(payload, data.icao_address);
    
    // Latitude (24-bit)
    pack24bit(payload, encodeLatitude(data.latitude));
    
    // Longitude (24-bit)
    pack24bit(payload, encodeLongitude(data.longitude));
    
    // Altitude (12-bit) + Miscellaneous (4-bit)
    uint16_t alt = encodeAltitude(data.altitude);
    uint8_t misc = (static_cast<uint8_t>(data.airborne) << 3) |  // Airborne flag
                   (0 << 2) |  // Report updated (not extrapolated)
                   (static_cast<uint8_t>(data.track_type) & 0x03);  // Track type
    
    payload.push_back((alt >> 4) & 0xFF);        // Altitude high 8 bits
    payload.push_back(((alt & 0x0F) << 4) | (misc & 0x0F));  // Altitude low 4 bits + misc
    
    // NIC and NACp (4 bits each)
    payload.push_back(((data.nic & 0x0F) << 4) | (data.nacp & 0x0F));
    
    // Horizontal Velocity (12-bit) + Vertical Velocity high 4 bits
    uint16_t h_vel = std::min(data.h_velocity, static_cast<uint16_t>(0xFFE));
    uint16_t v_vel = encodeVerticalVelocity(data.v_velocity);
    
    payload.push_back((h_vel >> 4) & 0xFF);      // H velocity high 8 bits
    payload.push_back(((h_vel & 0x0F) << 4) | ((v_vel >> 8) & 0x0F));  // H vel low 4 + V vel high 4
    payload.push_back(v_vel & 0xFF);             // V velocity low 8 bits
    
    // Track/Heading
    payload.push_back(encodeTrack(data.track));
    
    // Emitter Category
    payload.push_back(static_cast<uint8_t>(data.emitter_category));
    
    // Call Sign (8 bytes, ASCII, space-padded)
    std::string callsign = data.callsign.substr(0, 8);
    callsign.resize(8, ' ');  // Pad with spaces
    for (char c : callsign) {
        payload.push_back(static_cast<uint8_t>(c));
    }
    
    // Emergency/Priority Code (4 bits) + Spare (4 bits)
    payload.push_back((data.emergency_code & 0x0F) << 4);
    
    return prepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createOwnshipReport(const PositionData& data) {
    return createPositionReport(MSG_ID_OWNSHIP_REPORT, data);
}

std::vector<uint8_t> GDL90Encoder::createTrafficReport(const PositionData& data) {
    return createPositionReport(MSG_ID_TRAFFIC_REPORT, data);
}

} // namespace gdl90

