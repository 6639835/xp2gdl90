#include "xp2gdl90/msfs_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "xp2gdl90/foreflight_encoder.h"
#include "xp2gdl90/protocol_utils.h"

namespace msfs_bridge {

namespace {

constexpr double kRadiansToDegrees = 57.29577951308232;
constexpr double kFeetPerSecondToFeetPerMinute = 60.0;

template <typename Int, typename Float> Int ClampFloatToInt(Float value) {
  if (!std::isfinite(static_cast<double>(value))) {
    return Int{0};
  }
  const double v = static_cast<double>(value);
  const double lo = static_cast<double>(std::numeric_limits<Int>::min());
  const double hi = static_cast<double>(std::numeric_limits<Int>::max());
  if (v <= lo)
    return std::numeric_limits<Int>::min();
  if (v >= hi)
    return std::numeric_limits<Int>::max();
  return static_cast<Int>(value);
}

} // namespace

double NormalizeDegrees360(double degrees) {
  if (!std::isfinite(degrees)) {
    return std::numeric_limits<double>::quiet_NaN();
  }
  double normalized = std::fmod(degrees, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }
  return normalized;
}

uint16_t NormalizeDegreesToUint16(double degrees) {
  if (!std::isfinite(degrees)) {
    return 0;
  }
  return ClampFloatToInt<uint16_t>(NormalizeDegrees360(degrees));
}

uint16_t ClampKnotsToUint16OrInvalid(double knots) {
  if (!std::isfinite(knots) || knots < 0.0) {
    return gdl90::foreflight::AHRS_AIRSPEED_INVALID;
  }
  const double clamped =
      (std::min)(knots, static_cast<double>(
                            gdl90::foreflight::AHRS_AIRSPEED_INVALID - 1u));
  return static_cast<uint16_t>(clamped);
}

int16_t ClampFpmToInt16OrInvalid(double fpm) {
  if (!std::isfinite(fpm)) {
    return std::numeric_limits<int16_t>::min();
  }
  constexpr double kMin =
      static_cast<double>(std::numeric_limits<int16_t>::min() + 1);
  constexpr double kMax =
      static_cast<double>(std::numeric_limits<int16_t>::max());
  return static_cast<int16_t>((std::max)(kMin, (std::min)(kMax, fpm)));
}

std::string TrimString(const std::string &value) {
  const size_t start = value.find_first_not_of(" \t\r\n\0", 0);
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = value.find_last_not_of(" \t\r\n\0");
  return value.substr(start, end - start + 1);
}

uint32_t SyntheticTrafficAddress(uint32_t object_id) {
  uint32_t x = object_id;
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  x ^= x >> 16;
  return 0xF00000u | (x & 0x0FFFFFu);
}

gdl90::PositionData BuildOwnshipPosition(const OwnshipData &sim,
                                         const xp2gdl90::Settings &cfg) {
  const bool gps_valid = xp2gdl90::protocol::HasValidOwnshipPosition(
      sim.latitude_deg, sim.longitude_deg);

  gdl90::PositionData data;
  data.latitude = gps_valid ? sim.latitude_deg : 0.0;
  data.longitude = gps_valid ? sim.longitude_deg : 0.0;
  data.altitude = ClampFloatToInt<int32_t>(sim.pressure_altitude_ft);
  data.h_velocity =
      ClampFloatToInt<uint16_t>((std::max)(0.0, sim.ground_velocity_kt));
  data.v_velocity = ClampFpmToInt16OrInvalid(sim.vertical_speed_fps *
                                             kFeetPerSecondToFeetPerMinute);
  data.track = NormalizeDegreesToUint16(sim.true_heading_deg);
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = !sim.sim_on_ground;
  data.nic = gps_valid ? cfg.nic : 0;
  data.nacp = cfg.nacp;
  data.icao_address = cfg.icao_address;
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.callsign = xp2gdl90::protocol::SanitizeCallsign(sim.callsign);
  if (data.callsign.empty()) {
    data.callsign = xp2gdl90::protocol::SanitizeCallsign(cfg.callsign);
  }
  data.emitter_category =
      static_cast<gdl90::EmitterCategory>(cfg.emitter_category);
  return data;
}

gdl90::GeoAltitudeData BuildGeoAltitude(const OwnshipData &sim) {
  gdl90::GeoAltitudeData data;
  data.altitude_feet = ClampFloatToInt<int32_t>(sim.altitude_ft);
  data.vertical_warning = false;
  data.vfom_meters = gdl90::GEO_ALTITUDE_VFOM_INVALID;
  return data;
}

gdl90::foreflight::AhrsData BuildAhrs(const OwnshipData &sim,
                                      const xp2gdl90::Settings &cfg) {
  gdl90::foreflight::AhrsData data;
  data.roll_deg = sim.bank_deg;
  data.pitch_deg = sim.pitch_deg;
  data.heading_deg = NormalizeDegrees360(cfg.ahrs_use_magnetic_heading
                                             ? sim.magnetic_heading_deg
                                             : sim.true_heading_deg);
  data.magnetic_heading = cfg.ahrs_use_magnetic_heading;
  data.indicated_airspeed =
      ClampKnotsToUint16OrInvalid(sim.indicated_airspeed_kt);
  data.true_airspeed = ClampKnotsToUint16OrInvalid(sim.true_airspeed_kt);
  return data;
}

gdl90::foreflight::DeviceInfo BuildDeviceInfo(const xp2gdl90::Settings &cfg) {
  gdl90::foreflight::DeviceInfo data;
  data.serial_number = gdl90::foreflight::DEVICE_SERIAL_INVALID;
  data.device_name = cfg.device_name;
  data.device_long_name = cfg.device_long_name;
  data.capabilities_mask =
      0x01u | (static_cast<uint32_t>(cfg.internet_policy & 0x03u) << 1);
  return data;
}

bool BuildTrafficPosition(const TrafficData &traffic,
                          const xp2gdl90::Settings &cfg,
                          gdl90::PositionData *out_data) {
  if (!out_data || !xp2gdl90::protocol::HasValidOwnshipPosition(
                       traffic.latitude_deg, traffic.longitude_deg)) {
    return false;
  }

  const double speed = (std::max)(0.0, traffic.ground_velocity_kt);

  // Prefer velocity-vector track over heading when the aircraft is moving.
  double track = traffic.true_heading_deg;
  if (std::isfinite(traffic.velocity_world_x_fps) &&
      std::isfinite(traffic.velocity_world_z_fps) &&
      std::hypot(traffic.velocity_world_x_fps, traffic.velocity_world_z_fps) >
          1.0) {
    track =
        std::atan2(traffic.velocity_world_x_fps, traffic.velocity_world_z_fps) *
        kRadiansToDegrees;
  }

  gdl90::PositionData data;
  data.latitude = traffic.latitude_deg;
  data.longitude = traffic.longitude_deg;
  data.altitude = ClampFloatToInt<int32_t>(traffic.altitude_ft);
  data.h_velocity = ClampFloatToInt<uint16_t>(speed);
  data.v_velocity = ClampFpmToInt16OrInvalid(traffic.velocity_world_y_fps *
                                             kFeetPerSecondToFeetPerMinute);
  data.track = NormalizeDegreesToUint16(track);
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = !traffic.sim_on_ground;
  data.nic = cfg.nic;
  data.nacp = cfg.nacp;

  if (traffic.icao_address != 0) {
    data.icao_address = traffic.icao_address & 0x00FFFFFFu;
    data.address_type = gdl90::AddressType::ADSB_ICAO;
  } else {
    data.icao_address = SyntheticTrafficAddress(traffic.object_id);
    data.address_type = gdl90::AddressType::ADSB_SELF_ASSIGNED;
  }

  data.callsign = xp2gdl90::protocol::SanitizeCallsign(traffic.callsign);
  if (data.callsign.empty()) {
    char fallback[9] = {};
    std::snprintf(fallback, sizeof(fallback), "M%06X",
                  static_cast<unsigned int>(data.icao_address & 0xFFFFFFu));
    data.callsign = fallback;
  }
  data.emitter_category = gdl90::EmitterCategory::NO_INFO;
  *out_data = data;
  return true;
}

} // namespace msfs_bridge
