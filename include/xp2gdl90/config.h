#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>
#include <string>

/**
 * Configuration management for XP2GDL90 plugin.
 * Reads configuration from INI file.
 */

namespace config {

struct Config {
  std::string target_ip;
  uint16_t target_port;

  uint32_t icao_address;
  std::string callsign;
  uint8_t emitter_category;

  float heartbeat_rate;
  float position_rate;

  uint8_t nic;
  uint8_t nacp;

  bool debug_logging;
  bool log_messages;

  Config();
};

class ConfigManager {
 public:
  ConfigManager();
  ~ConfigManager() = default;

  bool load(const std::string& filename);
  bool save(const std::string& filename);

  const Config& getConfig() const { return config_; }
  Config& getConfig() { return config_; }

  std::string getLastError() const { return last_error_; }

 private:
  Config config_;
  std::string last_error_;

  void parseLine(const std::string& line);
  static std::string trim(const std::string& str);
  static bool parseBool(const std::string& value);
};

}  // namespace config

#endif  // CONFIG_H
