#ifndef XP2GDL90_SETTINGS_H
#define XP2GDL90_SETTINGS_H

#include <cstdint>
#include <string>

namespace xp2gdl90 {

struct Settings {
  std::string target_ip = "192.168.1.100";
  uint16_t target_port = 4000;
  bool foreflight_auto_discovery = true;
  uint16_t foreflight_broadcast_port = 63093;
  uint32_t icao_address = 0xABCDEF;
  std::string callsign = "N12345";
  uint8_t emitter_category = 1;
  std::string device_name = "XP2GDL90";
  std::string device_long_name = "XP2GDL90 AHRS";
  uint8_t internet_policy = 0;
  bool ahrs_use_magnetic_heading = false;

  float heartbeat_rate = 1.0f;
  float position_rate = 2.0f;

  uint8_t nic = 11;
  uint8_t nacp = 11;

  bool debug_logging = false;
  bool log_messages = false;
};

bool LoadSettingsFromJsonFile(const std::string& path,
                              Settings* out_settings,
                              std::string* out_error);
bool SaveSettingsToJsonFile(const std::string& path,
                            const Settings& settings,
                            std::string* out_error);

}  // namespace xp2gdl90

#endif  // XP2GDL90_SETTINGS_H
