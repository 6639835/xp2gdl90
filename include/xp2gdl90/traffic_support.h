#ifndef XP2GDL90_TRAFFIC_SUPPORT_H
#define XP2GDL90_TRAFFIC_SUPPORT_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace xp2gdl90::traffic {

struct TcasPresenceSample {
  size_t slot = 0;
  int raw_address = 0;
  int ssr_mode = 0;
  std::string callsign;
  double local_x = 0.0;
  double local_y = 0.0;
  double local_z = 0.0;
  double velocity_x = 0.0;
  double velocity_y = 0.0;
  double velocity_z = 0.0;
};

bool IsPopulatedTcasTarget(const TcasPresenceSample &sample);
uint32_t SyntheticTrafficAddress(size_t slot, const std::string &callsign,
                                 uint32_t ownship_address);
int32_t CorrectGeometricToPressureAltitude(int32_t geometric_altitude_feet,
                                           double ownship_geometric_feet,
                                           double ownship_pressure_feet);

} // namespace xp2gdl90::traffic

#endif // XP2GDL90_TRAFFIC_SUPPORT_H
