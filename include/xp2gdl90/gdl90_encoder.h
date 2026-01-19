#ifndef GDL90_ENCODER_H
#define GDL90_ENCODER_H

#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

/**
 * GDL90 Data Interface Encoder
 * Implements the Garmin GDL90 protocol for ADS-B data transmission.
 * Based on GDL90 Data Interface Specification (560-1058-00 Rev A).
 */

namespace gdl90 {

constexpr uint8_t MSG_ID_HEARTBEAT = 0x00;
constexpr uint8_t MSG_ID_OWNSHIP_REPORT = 0x0A;
constexpr uint8_t MSG_ID_TRAFFIC_REPORT = 0x14;

constexpr uint16_t ALTITUDE_INVALID = 0xFFF;
constexpr uint16_t VELOCITY_INVALID = 0xFFF;
constexpr uint16_t VVELOCITY_INVALID = 0x800;

enum class AddressType : uint8_t {
  ADSB_ICAO = 0,
  ADSB_SELF_ASSIGNED = 1,
  TISB_ICAO = 2,
  TISB_TRACK_FILE = 3,
  SURFACE_VEHICLE = 4,
  GROUND_STATION = 5,
};

enum class EmitterCategory : uint8_t {
  NO_INFO = 0,
  LIGHT = 1,
  SMALL = 2,
  LARGE = 3,
  HIGH_VORTEX_LARGE = 4,
  HEAVY = 5,
  HIGHLY_MANEUVERABLE = 6,
  ROTORCRAFT = 7,
  GLIDER = 9,
  LIGHTER_THAN_AIR = 10,
  PARACHUTIST = 11,
  ULTRA_LIGHT = 12,
  UAV = 14,
  SPACE_VEHICLE = 15,
};

enum class TrackType : uint8_t {
  INVALID = 0,
  TRUE_TRACK = 1,
  MAG_HEADING = 2,
  TRUE_HEADING = 3,
};

struct PositionData {
  double latitude;
  double longitude;
  int32_t altitude;
  uint16_t h_velocity;
  int16_t v_velocity;
  uint16_t track;
  TrackType track_type;
  bool airborne;
  uint8_t nic;
  uint8_t nacp;
  uint32_t icao_address;
  std::string callsign;
  EmitterCategory emitter_category;
  AddressType address_type;
  uint8_t alert_status;
  uint8_t emergency_code;
};

class GDL90Encoder {
 public:
  GDL90Encoder();
  ~GDL90Encoder() = default;

  std::vector<uint8_t> createHeartbeat(bool gps_valid = true,
                                       bool utc_ok = true);
  std::vector<uint8_t> createOwnshipReport(const PositionData& data);
  std::vector<uint8_t> createTrafficReport(const PositionData& data);

 private:
  static const uint16_t crc16_table_[256];

  uint16_t calculateCRC(const std::vector<uint8_t>& data) const;
  std::vector<uint8_t> escapeMessage(const std::vector<uint8_t>& data) const;
  std::vector<uint8_t> prepareMessage(
      const std::vector<uint8_t>& payload) const;

  uint32_t encodeLatitude(double latitude) const;
  uint32_t encodeLongitude(double longitude) const;
  uint16_t encodeAltitude(int32_t altitude) const;
  uint16_t encodeVerticalVelocity(int16_t vv_fpm) const;
  uint8_t encodeTrack(uint16_t track) const;
  void pack24bit(std::vector<uint8_t>& buffer, uint32_t value) const;
  std::vector<uint8_t> createPositionReport(uint8_t msg_id,
                                            const PositionData& data);
  uint32_t getUTCTime() const;
};

}  // namespace gdl90

#endif  // GDL90_ENCODER_H
