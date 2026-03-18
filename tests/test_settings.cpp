#include "test_harness.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "xp2gdl90/settings.h"

namespace {

std::filesystem::path MakeTempPath(const char* suffix) {
  const auto now =
      std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         ("xp2gdl90_" + std::to_string(now) + "_" + suffix);
}

struct ScopedFileCleanup {
  explicit ScopedFileCleanup(std::filesystem::path file_path)
      : path(std::move(file_path)) {}

  ~ScopedFileCleanup() {
    std::error_code error;
    std::filesystem::remove(path, error);
  }

  std::filesystem::path path;
};

}  // namespace

TEST_CASE("Settings save and load round-trip through JSON") {
  const std::filesystem::path path = MakeTempPath("settings.json");
  ScopedFileCleanup cleanup(path);

  xp2gdl90::Settings saved;
  saved.target_ip = "10.0.0.42";
  saved.target_port = 4567;
  saved.foreflight_auto_discovery = false;
  saved.foreflight_broadcast_port = 63094;
  saved.icao_address = 0x102030;
  saved.callsign = "N123TEST";
  saved.emitter_category = 7;
  saved.device_name = "BRIDGE";
  saved.device_long_name = "Bridge Device";
  saved.internet_policy = 2;
  saved.ahrs_use_magnetic_heading = true;
  saved.heartbeat_rate = 3.5f;
  saved.position_rate = 1.25f;
  saved.nic = 10;
  saved.nacp = 9;
  saved.debug_logging = true;
  saved.log_messages = true;

  std::string error;
  ASSERT_TRUE(
      xp2gdl90::SaveSettingsToJsonFile(path.string(), saved, &error));
  ASSERT_EQ(std::string(""), error);

  xp2gdl90::Settings loaded;
  ASSERT_TRUE(
      xp2gdl90::LoadSettingsFromJsonFile(path.string(), &loaded, &error));
  ASSERT_EQ(std::string(""), error);

  ASSERT_EQ(saved.target_ip, loaded.target_ip);
  ASSERT_EQ(saved.target_port, loaded.target_port);
  ASSERT_EQ(saved.foreflight_auto_discovery, loaded.foreflight_auto_discovery);
  ASSERT_EQ(saved.foreflight_broadcast_port, loaded.foreflight_broadcast_port);
  ASSERT_EQ(saved.icao_address, loaded.icao_address);
  ASSERT_EQ(saved.callsign, loaded.callsign);
  ASSERT_EQ(saved.emitter_category, loaded.emitter_category);
  ASSERT_EQ(saved.device_name, loaded.device_name);
  ASSERT_EQ(saved.device_long_name, loaded.device_long_name);
  ASSERT_EQ(saved.internet_policy, loaded.internet_policy);
  ASSERT_EQ(saved.ahrs_use_magnetic_heading, loaded.ahrs_use_magnetic_heading);
  ASSERT_EQ(saved.heartbeat_rate, loaded.heartbeat_rate);
  ASSERT_EQ(saved.position_rate, loaded.position_rate);
  ASSERT_EQ(saved.nic, loaded.nic);
  ASSERT_EQ(saved.nacp, loaded.nacp);
  ASSERT_EQ(saved.debug_logging, loaded.debug_logging);
  ASSERT_EQ(saved.log_messages, loaded.log_messages);
}

TEST_CASE("Settings loader ignores invalid values and unknown keys") {
  const std::filesystem::path path = MakeTempPath("settings_invalid.json");
  ScopedFileCleanup cleanup(path);

  std::ofstream file(path);
  file
      << "{\n"
      << "  \"target_ip\": \"192.168.0.10\",\n"
      << "  \"target_port\": 70000,\n"
      << "  \"emitter_category\": 99,\n"
      << "  \"nic\": 12,\n"
      << "  \"nacp\": 15,\n"
      << "  \"debug_logging\": true,\n"
      << "  \"unknown_object\": {\"nested\": true},\n"
      << "  \"unknown_array\": [1, 2, 3]\n"
      << "}\n";
  file.close();

  xp2gdl90::Settings loaded;
  loaded.target_port = 4000;
  loaded.emitter_category = 1;
  loaded.nic = 11;
  loaded.nacp = 11;

  std::string error;
  ASSERT_TRUE(
      xp2gdl90::LoadSettingsFromJsonFile(path.string(), &loaded, &error));
  ASSERT_EQ(std::string(""), error);

  ASSERT_EQ(std::string("192.168.0.10"), loaded.target_ip);
  ASSERT_EQ(static_cast<uint16_t>(4000), loaded.target_port);
  ASSERT_EQ(static_cast<uint8_t>(1), loaded.emitter_category);
  ASSERT_EQ(static_cast<uint8_t>(11), loaded.nic);
  ASSERT_EQ(static_cast<uint8_t>(11), loaded.nacp);
  ASSERT_TRUE(loaded.debug_logging);
}

TEST_CASE("Settings loader reports malformed JSON") {
  const std::filesystem::path path = MakeTempPath("settings_broken.json");
  ScopedFileCleanup cleanup(path);

  std::ofstream file(path);
  file << "{\"target_ip\": [}";
  file.close();

  xp2gdl90::Settings loaded;
  std::string error;
  ASSERT_TRUE(
      !xp2gdl90::LoadSettingsFromJsonFile(path.string(), &loaded, &error));
  ASSERT_TRUE(error.find("Invalid settings JSON:") != std::string::npos);
}
