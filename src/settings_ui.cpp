#include "xp2gdl90/settings_ui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "xp2gdl90/protocol_utils.h"

namespace xp2gdl90 {
namespace {

std::string Trim(const char* text) {
  if (!text) {
    return "";
  }

  std::string value(text);
  auto first = std::find_if(value.begin(), value.end(), [](unsigned char ch) {
    return std::isspace(ch) == 0;
  });
  auto last = std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
    return std::isspace(ch) == 0;
  }).base();

  if (first >= last) {
    return "";
  }
  return std::string(first, last);
}

bool ParseHex24(const char* text, uint32_t* out_value) {
  if (!text || !out_value) {
    return false;
  }

  const std::string trimmed = Trim(text);
  if (trimmed.empty()) {
    return false;
  }

  try {
    *out_value = static_cast<uint32_t>(std::stoul(trimmed, nullptr, 16)) &
                 0xFFFFFFu;
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

void SyncSettingsUiFromConfig(SettingsUiState* ui_state,
                              const Settings& settings) {
  if (!ui_state) {
    return;
  }

  std::snprintf(ui_state->target_ip, sizeof(ui_state->target_ip), "%s",
                settings.target_ip.c_str());
  ui_state->target_port = static_cast<int>(settings.target_port);
  ui_state->foreflight_auto_discovery = settings.foreflight_auto_discovery;
  ui_state->foreflight_broadcast_port =
      static_cast<int>(settings.foreflight_broadcast_port);
  std::snprintf(ui_state->icao_address, sizeof(ui_state->icao_address), "0x%06X",
                static_cast<unsigned int>(settings.icao_address & 0xFFFFFFu));
  std::snprintf(ui_state->callsign, sizeof(ui_state->callsign), "%s",
                settings.callsign.c_str());
  ui_state->emitter_category = static_cast<int>(settings.emitter_category);
  std::snprintf(ui_state->device_name, sizeof(ui_state->device_name), "%s",
                settings.device_name.c_str());
  std::snprintf(ui_state->device_long_name, sizeof(ui_state->device_long_name),
                "%s", settings.device_long_name.c_str());
  ui_state->internet_policy = static_cast<int>(settings.internet_policy);
  ui_state->ahrs_use_magnetic_heading = settings.ahrs_use_magnetic_heading;
  ui_state->heartbeat_rate = settings.heartbeat_rate;
  ui_state->position_rate = settings.position_rate;
  ui_state->nic = static_cast<int>(settings.nic);
  ui_state->nacp = static_cast<int>(settings.nacp);
  ui_state->debug_logging = settings.debug_logging;
  ui_state->log_messages = settings.log_messages;
}

void LoadDefaultSettingsUiState(SettingsUiState* ui_state) {
  SyncSettingsUiFromConfig(ui_state, Settings{});
}

bool BuildConfigFromSettingsUi(const SettingsUiState& ui_state,
                               const Settings& base_settings,
                               Settings* out_settings,
                               std::string* out_error) {
  if (!out_settings) {
    if (out_error) {
      *out_error = "Output settings object is required";
    }
    return false;
  }

  Settings settings = base_settings;

  const std::string ip = Trim(ui_state.target_ip);
  if (ip.empty()) {
    if (out_error) {
      *out_error = "Target IP cannot be empty";
    }
    return false;
  }
  if (!protocol::IsValidIpv4Address(ip)) {
    if (out_error) {
      *out_error = "Target IP must be a valid IPv4 address";
    }
    return false;
  }
  settings.target_ip = ip;

  if (ui_state.target_port <= 0 || ui_state.target_port > 65535) {
    if (out_error) {
      *out_error = "Target port must be 1-65535";
    }
    return false;
  }
  settings.target_port = static_cast<uint16_t>(ui_state.target_port);

  settings.foreflight_auto_discovery = ui_state.foreflight_auto_discovery;
  if (ui_state.foreflight_broadcast_port <= 0 ||
      ui_state.foreflight_broadcast_port > 65535) {
    if (out_error) {
      *out_error = "ForeFlight broadcast port must be 1-65535";
    }
    return false;
  }
  settings.foreflight_broadcast_port =
      static_cast<uint16_t>(ui_state.foreflight_broadcast_port);

  uint32_t icao_address = 0;
  if (!ParseHex24(ui_state.icao_address, &icao_address)) {
    if (out_error) {
      *out_error = "ICAO address must be a hex value (e.g. 0xABCDEF)";
    }
    return false;
  }
  settings.icao_address = icao_address;

  settings.callsign = Trim(ui_state.callsign).substr(0, 8);
  settings.device_name = Trim(ui_state.device_name).substr(0, 8);
  settings.device_long_name = Trim(ui_state.device_long_name).substr(0, 16);

  if (ui_state.emitter_category < 0 || ui_state.emitter_category > 39) {
    if (out_error) {
      *out_error = "Emitter category must be 0-39";
    }
    return false;
  }
  settings.emitter_category = static_cast<uint8_t>(ui_state.emitter_category);

  if (ui_state.internet_policy < 0 || ui_state.internet_policy > 2) {
    if (out_error) {
      *out_error = "Internet policy must be 0-2";
    }
    return false;
  }
  settings.internet_policy = static_cast<uint8_t>(ui_state.internet_policy);
  settings.ahrs_use_magnetic_heading = ui_state.ahrs_use_magnetic_heading;

  if (ui_state.heartbeat_rate <= 0.0f) {
    if (out_error) {
      *out_error = "Heartbeat rate must be > 0";
    }
    return false;
  }
  settings.heartbeat_rate = ui_state.heartbeat_rate;

  if (ui_state.position_rate <= 0.0f) {
    if (out_error) {
      *out_error = "Position rate must be > 0";
    }
    return false;
  }
  settings.position_rate = ui_state.position_rate;

  if (ui_state.nic < 0 || ui_state.nic > 11) {
    if (out_error) {
      *out_error = "NIC must be 0-11";
    }
    return false;
  }
  settings.nic = static_cast<uint8_t>(ui_state.nic);

  if (ui_state.nacp < 0 || ui_state.nacp > 11) {
    if (out_error) {
      *out_error = "NACp must be 0-11";
    }
    return false;
  }
  settings.nacp = static_cast<uint8_t>(ui_state.nacp);

  settings.debug_logging = ui_state.debug_logging;
  settings.log_messages = ui_state.log_messages;

  *out_settings = settings;
  if (out_error) {
    out_error->clear();
  }
  return true;
}

}  // namespace xp2gdl90
