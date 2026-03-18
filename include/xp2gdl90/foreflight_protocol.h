#ifndef XP2GDL90_FOREFLIGHT_PROTOCOL_H
#define XP2GDL90_FOREFLIGHT_PROTOCOL_H

#include <cstdint>
#include <vector>

namespace xp2gdl90::foreflight {

bool ParseDiscoveryBroadcast(const std::vector<uint8_t>& packet,
                             uint16_t* out_port);

}  // namespace xp2gdl90::foreflight

#endif  // XP2GDL90_FOREFLIGHT_PROTOCOL_H
