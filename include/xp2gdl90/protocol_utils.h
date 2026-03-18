#ifndef XP2GDL90_PROTOCOL_UTILS_H
#define XP2GDL90_PROTOCOL_UTILS_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace xp2gdl90::protocol {

std::string SanitizeCallsign(std::string_view input);

bool IsValidNic(uint8_t value);
bool IsValidNacp(uint8_t value);
bool IsValidEmitterCategory(uint8_t value);

bool HasValidOwnshipPosition(double latitude, double longitude);

bool ParseForeFlightDiscoveryBroadcast(const std::vector<uint8_t>& packet,
                                       uint16_t* out_port);

}  // namespace xp2gdl90::protocol

#endif  // XP2GDL90_PROTOCOL_UTILS_H
