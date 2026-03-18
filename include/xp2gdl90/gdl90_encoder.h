#ifndef GDL90_ENCODER_H
#define GDL90_ENCODER_H

#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

/**
 * GDL90 Data Interface Encoder
 * Implements the Garmin GDL90 protocol for ADS-B data transmission.
 * Based on GDL90 Data Interface Specification (560-1058-00 Rev A).
 */

namespace gdl90 {

using UtcTimeProvider = std::function<uint32_t()>;
using CheckedUtcTimeProvider = std::function<bool(uint32_t*)>;

constexpr uint8_t MSG_ID_HEARTBEAT = 0x00;
constexpr uint8_t MSG_ID_OWNSHIP_REPORT = 0x0A;
constexpr uint8_t MSG_ID_OWNSHIP_GEO_ALTITUDE = 0x0B;
constexpr uint8_t MSG_ID_TRAFFIC_REPORT = 0x14;

constexpr uint16_t ALTITUDE_INVALID = 0xFFF;
constexpr uint16_t VELOCITY_INVALID = 0xFFF;
constexpr uint16_t VVELOCITY_INVALID = 0x800;
constexpr uint16_t GEO_ALTITUDE_VFOM_INVALID = 0x7FFF;
constexpr uint16_t GEO_ALTITUDE_VFOM_EXCESSIVE = 0x7FFEu;

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
  double latitude = 0.0;
  double longitude = 0.0;
  int32_t altitude = 0;
  uint16_t h_velocity = 0;
  int16_t v_velocity = 0;
  uint16_t track = 0;
  TrackType track_type = TrackType::INVALID;
  bool airborne = false;
  uint8_t nic = 0;
  uint8_t nacp = 0;
  uint32_t icao_address = 0;
  std::string callsign;
  EmitterCategory emitter_category = EmitterCategory::NO_INFO;
  AddressType address_type = AddressType::ADSB_ICAO;
  uint8_t alert_status = 0;
  uint8_t emergency_code = 0;
};

struct GeoAltitudeData {
  int32_t altitude_feet = 0;
  bool vertical_warning = false;
  uint16_t vfom_meters = GEO_ALTITUDE_VFOM_INVALID;
};

class GDL90Encoder {
 public:
  GDL90Encoder();
  explicit GDL90Encoder(UtcTimeProvider utc_time_provider);
  explicit GDL90Encoder(CheckedUtcTimeProvider utc_time_provider);
  ~GDL90Encoder() = default;

  std::vector<uint8_t> createHeartbeat(bool gps_valid = true,
                                       bool utc_ok = true) const;
  std::vector<uint8_t> createOwnshipReport(const PositionData& data) const;
  std::vector<uint8_t> createOwnshipGeometricAltitude(
      const GeoAltitudeData& data) const;
  std::vector<uint8_t> createTrafficReport(const PositionData& data) const;

 private:
  CheckedUtcTimeProvider utc_time_provider_;

  uint32_t encodeLatitude(double latitude) const;
  uint32_t encodeLongitude(double longitude) const;
  uint16_t encodeAltitude(int32_t altitude) const;
  uint16_t encodeVerticalVelocity(int16_t vv_fpm) const;
  uint8_t encodeTrack(uint16_t track) const;
  int16_t encodeGeoAltitude(int32_t altitude_feet) const;
  uint16_t encodeGeoVerticalMetrics(bool vertical_warning,
                                    uint16_t vfom_meters) const;
  std::vector<uint8_t> createPositionReport(uint8_t msg_id,
                                            const PositionData& data) const;
  bool getUTCTime(uint32_t* out_time) const;
};

}  // namespace gdl90

#endif  // GDL90_ENCODER_H
