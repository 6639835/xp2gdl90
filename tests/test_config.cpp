#include "test_harness.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "xp2gdl90/config.h"

namespace {

std::filesystem::path MakeTempPath(const std::string& suffix) {
  const auto base = std::filesystem::temp_directory_path();
  const auto unique = std::to_string(
      static_cast<unsigned long long>(
          std::chrono::high_resolution_clock::now()
              .time_since_epoch()
              .count()));
  return base / ("xp2gdl90_" + suffix + "_" + unique + ".ini");
}

}  // namespace

TEST_CASE("Config defaults are initialized") {
  config::ConfigManager manager;
  const config::Config& cfg = manager.getConfig();

  ASSERT_EQ(std::string("192.168.1.100"), cfg.target_ip);
  ASSERT_EQ(static_cast<uint16_t>(4000), cfg.target_port);
  ASSERT_EQ(static_cast<uint32_t>(0xABCDEF), cfg.icao_address);
  ASSERT_EQ(std::string("N12345"), cfg.callsign);
  ASSERT_EQ(static_cast<uint8_t>(1), cfg.emitter_category);
  ASSERT_EQ(1.0f, cfg.heartbeat_rate);
  ASSERT_EQ(2.0f, cfg.position_rate);
  ASSERT_EQ(static_cast<uint8_t>(11), cfg.nic);
  ASSERT_EQ(static_cast<uint8_t>(11), cfg.nacp);
  ASSERT_EQ(false, cfg.debug_logging);
  ASSERT_EQ(false, cfg.log_messages);
}

TEST_CASE("Config load parses values and comments") {
  const auto path = MakeTempPath("parse");
  std::ofstream file(path);
  file << "# comment\n";
  file << "target_ip = 10.0.0.5 # inline\n";
  file << "target_port = 5000\n";
  file << "icao_address = 0x00FF00\n";
  file << "callsign = TESTCALLSIGN\n";
  file << "emitter_category = 7\n";
  file << "heartbeat_rate = 0.5\n";
  file << "position_rate = 5.0\n";
  file << "nic = 3\n";
  file << "nacp = 9\n";
  file << "debug_logging = YES\n";
  file << "log_messages = on\n";
  file.close();

  config::ConfigManager manager;
  ASSERT_TRUE(manager.load(path.string()));

  const config::Config& cfg = manager.getConfig();
  ASSERT_EQ(std::string("10.0.0.5"), cfg.target_ip);
  ASSERT_EQ(static_cast<uint16_t>(5000), cfg.target_port);
  ASSERT_EQ(static_cast<uint32_t>(0x00FF00), cfg.icao_address);
  ASSERT_EQ(std::string("TESTCALL"), cfg.callsign);
  ASSERT_EQ(static_cast<uint8_t>(7), cfg.emitter_category);
  ASSERT_EQ(0.5f, cfg.heartbeat_rate);
  ASSERT_EQ(5.0f, cfg.position_rate);
  ASSERT_EQ(static_cast<uint8_t>(3), cfg.nic);
  ASSERT_EQ(static_cast<uint8_t>(9), cfg.nacp);
  ASSERT_EQ(true, cfg.debug_logging);
  ASSERT_EQ(true, cfg.log_messages);
}

TEST_CASE("Config load rolls back on parse errors") {
  const auto path = MakeTempPath("bad");
  std::ofstream file(path);
  file << "target_ip = 192.168.1.55\n";
  file << "target_port = 0\n";
  file.close();

  config::ConfigManager manager;
  manager.getConfig().target_ip = "1.2.3.4";
  manager.getConfig().target_port = 1234;

  ASSERT_TRUE(!manager.load(path.string()));
  const config::Config& cfg = manager.getConfig();
  ASSERT_EQ(std::string("1.2.3.4"), cfg.target_ip);
  ASSERT_EQ(static_cast<uint16_t>(1234), cfg.target_port);
  ASSERT_NE(std::string(""), manager.getLastError());
}

TEST_CASE("Config save and reload round-trip") {
  const auto path = MakeTempPath("roundtrip");

  config::ConfigManager manager;
  config::Config& cfg = manager.getConfig();
  cfg.target_ip = "172.16.0.10";
  cfg.target_port = 4020;
  cfg.icao_address = 0x0A0B0C;
  cfg.callsign = "UNITTEST";
  cfg.emitter_category = 5;
  cfg.heartbeat_rate = 2.5f;
  cfg.position_rate = 1.25f;
  cfg.nic = 10;
  cfg.nacp = 8;
  cfg.debug_logging = true;
  cfg.log_messages = false;

  ASSERT_TRUE(manager.save(path.string()));

  config::ConfigManager manager2;
  ASSERT_TRUE(manager2.load(path.string()));
  const config::Config& loaded = manager2.getConfig();

  ASSERT_EQ(std::string("172.16.0.10"), loaded.target_ip);
  ASSERT_EQ(static_cast<uint16_t>(4020), loaded.target_port);
  ASSERT_EQ(static_cast<uint32_t>(0x0A0B0C), loaded.icao_address);
  ASSERT_EQ(std::string("UNITTEST"), loaded.callsign);
  ASSERT_EQ(static_cast<uint8_t>(5), loaded.emitter_category);
  ASSERT_EQ(2.5f, loaded.heartbeat_rate);
  ASSERT_EQ(1.25f, loaded.position_rate);
  ASSERT_EQ(static_cast<uint8_t>(10), loaded.nic);
  ASSERT_EQ(static_cast<uint8_t>(8), loaded.nacp);
  ASSERT_EQ(true, loaded.debug_logging);
  ASSERT_EQ(false, loaded.log_messages);
}
