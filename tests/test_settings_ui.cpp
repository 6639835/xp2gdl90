#include "test_harness.h"

#include <cstdio>
#include <string>

#include "xp2gdl90/settings_ui.h"

TEST_CASE("Settings UI sync reflects config values") {
  xp2gdl90::Settings settings;
  settings.target_ip = "172.16.1.12";
  settings.target_port = 4242;
  settings.foreflight_auto_discovery = false;
  settings.foreflight_broadcast_port = 63094;
  settings.icao_address = 0xA0B1C2;
  settings.callsign = "N42";
  settings.emitter_category = 3;
  settings.device_name = "XPTEST";
  settings.device_long_name = "XP Test Device";
  settings.internet_policy = 1;
  settings.ahrs_use_magnetic_heading = true;
  settings.heartbeat_rate = 5.0f;
  settings.position_rate = 2.5f;
  settings.nic = 10;
  settings.nacp = 9;
  settings.debug_logging = true;
  settings.log_messages = true;

  xp2gdl90::SettingsUiState ui_state;
  xp2gdl90::SyncSettingsUiFromConfig(&ui_state, settings);

  ASSERT_EQ(std::string("172.16.1.12"), std::string(ui_state.target_ip));
  ASSERT_EQ(4242, ui_state.target_port);
  ASSERT_TRUE(!ui_state.foreflight_auto_discovery);
  ASSERT_EQ(63094, ui_state.foreflight_broadcast_port);
  ASSERT_EQ(std::string("0xA0B1C2"), std::string(ui_state.icao_address));
  ASSERT_EQ(std::string("N42"), std::string(ui_state.callsign));
  ASSERT_EQ(3, ui_state.emitter_category);
  ASSERT_EQ(std::string("XPTEST"), std::string(ui_state.device_name));
  ASSERT_EQ(std::string("XP Test Device"), std::string(ui_state.device_long_name));
  ASSERT_EQ(1, ui_state.internet_policy);
  ASSERT_TRUE(ui_state.ahrs_use_magnetic_heading);
  ASSERT_EQ(5.0f, ui_state.heartbeat_rate);
  ASSERT_EQ(2.5f, ui_state.position_rate);
  ASSERT_EQ(10, ui_state.nic);
  ASSERT_EQ(9, ui_state.nacp);
  ASSERT_TRUE(ui_state.debug_logging);
  ASSERT_TRUE(ui_state.log_messages);
}

TEST_CASE("Settings UI defaults mirror default config") {
  xp2gdl90::SyncSettingsUiFromConfig(nullptr, xp2gdl90::Settings{});

  xp2gdl90::SettingsUiState ui_state;
  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);

  ASSERT_EQ(std::string("192.168.1.100"), std::string(ui_state.target_ip));
  ASSERT_EQ(4000, ui_state.target_port);
  ASSERT_TRUE(ui_state.foreflight_auto_discovery);
  ASSERT_EQ(63093, ui_state.foreflight_broadcast_port);
  ASSERT_EQ(std::string("0xABCDEF"), std::string(ui_state.icao_address));
}

TEST_CASE("Settings UI builder validates fields and trims text") {
  xp2gdl90::SettingsUiState ui_state;
  std::snprintf(ui_state.target_ip, sizeof(ui_state.target_ip), " 10.1.1.5 ");
  ui_state.target_port = 4000;
  ui_state.foreflight_auto_discovery = true;
  ui_state.foreflight_broadcast_port = 63093;
  std::snprintf(ui_state.icao_address, sizeof(ui_state.icao_address), "0xABCDEF");
  std::snprintf(ui_state.callsign, sizeof(ui_state.callsign), " N123456789 ");
  ui_state.emitter_category = 7;
  std::snprintf(ui_state.device_name, sizeof(ui_state.device_name), " DEVICE01 ");
  std::snprintf(ui_state.device_long_name, sizeof(ui_state.device_long_name),
                " Long Device Name ");
  ui_state.internet_policy = 2;
  ui_state.ahrs_use_magnetic_heading = true;
  ui_state.heartbeat_rate = 2.0f;
  ui_state.position_rate = 1.0f;
  ui_state.nic = 11;
  ui_state.nacp = 10;
  ui_state.debug_logging = true;
  ui_state.log_messages = false;

  xp2gdl90::Settings built;
  std::string error;
  ASSERT_TRUE(xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_EQ(std::string(""), error);
  ASSERT_EQ(std::string("10.1.1.5"), built.target_ip);
  ASSERT_EQ(std::string("N1234567"), built.callsign);
  ASSERT_EQ(std::string("DEVICE01"), built.device_name);
  ASSERT_EQ(std::string("Long Device Name"), built.device_long_name);
  ASSERT_EQ(static_cast<uint8_t>(7), built.emitter_category);
  ASSERT_EQ(static_cast<uint8_t>(2), built.internet_policy);
  ASSERT_TRUE(built.ahrs_use_magnetic_heading);
}

TEST_CASE("Settings UI builder requires output settings object") {
  xp2gdl90::SettingsUiState ui_state;
  std::string error;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, nullptr, &error));
  ASSERT_TRUE(error.find("Output settings object is required") !=
              std::string::npos);
}

TEST_CASE("Settings UI builder rejects invalid numeric ranges") {
  xp2gdl90::SettingsUiState ui_state;
  std::snprintf(ui_state.target_ip, sizeof(ui_state.target_ip), "127.0.0.1");
  ui_state.target_port = 0;
  ui_state.foreflight_broadcast_port = 63093;
  std::snprintf(ui_state.icao_address, sizeof(ui_state.icao_address), "0xABCDEF");
  ui_state.emitter_category = 1;
  ui_state.internet_policy = 0;
  ui_state.heartbeat_rate = 1.0f;
  ui_state.position_rate = 1.0f;
  ui_state.nic = 11;
  ui_state.nacp = 11;

  xp2gdl90::Settings built;
  std::string error;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Target port must be 1-65535") != std::string::npos);
}

TEST_CASE("Settings UI builder rejects invalid text and remaining numeric fields") {
  xp2gdl90::SettingsUiState ui_state;
  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  xp2gdl90::Settings built;
  std::string error;

  std::snprintf(ui_state.target_ip, sizeof(ui_state.target_ip), "   ");
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Target IP cannot be empty") != std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  std::snprintf(ui_state.target_ip, sizeof(ui_state.target_ip), "example.com");
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(
      error.find("Target IP must be a valid IPv4 address") != std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.foreflight_broadcast_port = 70000;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(
      error.find("ForeFlight broadcast port must be 1-65535") !=
      std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  std::snprintf(ui_state.icao_address, sizeof(ui_state.icao_address), "   ");
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(
      error.find("ICAO address must be a hex value") != std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  std::snprintf(ui_state.icao_address, sizeof(ui_state.icao_address), "XYZ");
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(
      error.find("ICAO address must be a hex value") != std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.emitter_category = 40;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Emitter category must be 0-39") !=
              std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.internet_policy = 3;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Internet policy must be 0-2") !=
              std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.heartbeat_rate = 0.0f;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Heartbeat rate must be > 0") !=
              std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.position_rate = 0.0f;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("Position rate must be > 0") !=
              std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.nic = 12;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("NIC must be 0-11") != std::string::npos);

  xp2gdl90::LoadDefaultSettingsUiState(&ui_state);
  ui_state.nacp = 12;
  ASSERT_TRUE(!xp2gdl90::BuildConfigFromSettingsUi(
      ui_state, xp2gdl90::Settings{}, &built, &error));
  ASSERT_TRUE(error.find("NACp must be 0-11") != std::string::npos);
}
