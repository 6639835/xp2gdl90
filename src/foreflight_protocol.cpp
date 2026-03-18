#include "xp2gdl90/foreflight_protocol.h"

#include <cmath>
#include <string>

#include "xp2gdl90/simple_json.h"

namespace xp2gdl90::foreflight {

bool ParseDiscoveryBroadcast(const std::vector<uint8_t>& packet,
                             uint16_t* out_port) {
  if (!out_port || packet.empty()) {
    return false;
  }

  json::Value root;
  if (!json::Parse(std::string(packet.begin(), packet.end()), &root, nullptr) ||
      !root.IsObject()) {
    return false;
  }

  const json::Value* app_value = root.Find("App");
  if (!app_value || !app_value->IsString() ||
      app_value->string_value != "ForeFlight") {
    return false;
  }

  const json::Value* gdl90_value = root.Find("GDL90");
  if (!gdl90_value || !gdl90_value->IsObject()) {
    return false;
  }

  const json::Value* port_value = gdl90_value->Find("port");
  if (!port_value || !port_value->IsNumber() ||
      !std::isfinite(port_value->number_value) ||
      port_value->number_value < 1.0 || port_value->number_value > 65535.0) {
    return false;
  }

  *out_port =
      static_cast<uint16_t>(static_cast<unsigned int>(port_value->number_value));
  return true;
}

}  // namespace xp2gdl90::foreflight
