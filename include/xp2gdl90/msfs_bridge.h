#pragma once

#include <cstdint>
#include <string>

#include "xp2gdl90/foreflight_encoder.h"
#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/settings.h"

// Portable computation helpers for the MSFS SimConnect bridge.
// This header is intentionally free of Windows/SimConnect dependencies so the
// logic can be unit-tested on any platform.
namespace msfs_bridge {

struct OwnshipData {
  double latitude_deg = 0.0;
  double longitude_deg = 0.0;
  double altitude_ft = 0.0;
  double pressure_altitude_ft = 0.0;
  double ground_velocity_kt = 0.0;
  double vertical_speed_fps = 0.0;
  double true_heading_deg = 0.0;
  double magnetic_heading_deg = 0.0;
  double pitch_deg = 0.0;
  double bank_deg = 0.0;
  double indicated_airspeed_kt = 0.0;
  double true_airspeed_kt = 0.0;
  bool sim_on_ground = false;
  std::string callsign;
};

struct TrafficData {
  uint32_t object_id = 0;
  double latitude_deg = 0.0;
  double longitude_deg = 0.0;
  double altitude_ft = 0.0;
  double ground_velocity_kt = 0.0;
  double velocity_world_x_fps = 0.0;
  double velocity_world_y_fps = 0.0;
  double velocity_world_z_fps = 0.0;
  double true_heading_deg = 0.0;
  bool sim_on_ground = false;
  std::string callsign;
  // Non-zero when the simulator exposes a real ICAO address (e.g. MSFS 2024
  // via AI TRAFFIC ICAO ADDRESS).  Zero triggers synthetic address generation.
  uint32_t icao_address = 0;
};

// Pure math helpers exposed for testing.
double NormalizeDegrees360(double degrees);
uint16_t NormalizeDegreesToUint16(double degrees);
uint16_t ClampKnotsToUint16OrInvalid(double knots);
int16_t ClampFpmToInt16OrInvalid(double fpm);
std::string TrimString(const std::string &value);
uint32_t SyntheticTrafficAddress(uint32_t object_id);

// GDL90 / ForeFlight data builders.
gdl90::PositionData BuildOwnshipPosition(const OwnshipData &sim,
                                         const xp2gdl90::Settings &cfg);
gdl90::GeoAltitudeData BuildGeoAltitude(const OwnshipData &sim);
gdl90::foreflight::AhrsData BuildAhrs(const OwnshipData &sim,
                                      const xp2gdl90::Settings &cfg);
gdl90::foreflight::DeviceInfo BuildDeviceInfo(const xp2gdl90::Settings &cfg);

// Returns false and leaves *out_data unchanged if the traffic position is
// invalid (out-of-range coordinates).
bool BuildTrafficPosition(const TrafficData &traffic,
                          const xp2gdl90::Settings &cfg,
                          gdl90::PositionData *out_data);

} // namespace msfs_bridge
