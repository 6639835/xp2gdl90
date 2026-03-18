#ifndef XP2GDL90_SETTINGS_UI_H
#define XP2GDL90_SETTINGS_UI_H

#include <string>

#include "xp2gdl90/settings.h"

namespace xp2gdl90 {

struct SettingsUiState {
  char target_ip[64] = {};
  int target_port = 0;
  bool foreflight_auto_discovery = false;
  int foreflight_broadcast_port = 0;
  char icao_address[16] = {};
  char callsign[16] = {};
  int emitter_category = 0;
  char device_name[32] = {};
  char device_long_name[64] = {};
  int internet_policy = 0;
  bool ahrs_use_magnetic_heading = false;
  float heartbeat_rate = 0.0f;
  float position_rate = 0.0f;
  int nic = 0;
  int nacp = 0;
  bool debug_logging = false;
  bool log_messages = false;
};

void SyncSettingsUiFromConfig(SettingsUiState* ui_state, const Settings& settings);
void LoadDefaultSettingsUiState(SettingsUiState* ui_state);
bool BuildConfigFromSettingsUi(const SettingsUiState& ui_state,
                               const Settings& base_settings,
                               Settings* out_settings,
                               std::string* out_error);

}  // namespace xp2gdl90

#endif  // XP2GDL90_SETTINGS_UI_H
