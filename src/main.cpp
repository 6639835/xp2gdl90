/**
 * XP2GDL90 - X-Plane to GDL90 Bridge Plugin
 *
 * Broadcasts X-Plane flight data in GDL90 format via UDP.
 * Compatible with ForeFlight, Garmin Pilot, and other EFB applications.
 *
 * Author: XP2GDL90 Project
 * License: MIT
 */

#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>

#include <array>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "xp2gdl90/config.h"
#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/udp_broadcaster.h"

namespace {

constexpr double kMetersToFeet = 3.28084;
constexpr float kMetersPerSecondToKnots = 1.94384f;

enum class FieldId {
  TargetIp = 0,
  TargetPort,
  IcaoAddress,
  Callsign,
  EmitterCategory,
  HeartbeatRate,
  PositionRate,
  Nic,
  Nacp,
  DebugLogging,
  LogMessages,
  Count
};

struct FieldDef {
  FieldId id;
  const char* label;
};

constexpr std::array<FieldDef, 11> kFieldDefs = {{
    {FieldId::TargetIp, "Target IP"},
    {FieldId::TargetPort, "Target Port"},
    {FieldId::IcaoAddress, "ICAO Address"},
    {FieldId::Callsign, "Callsign"},
    {FieldId::EmitterCategory, "Emitter Category"},
    {FieldId::HeartbeatRate, "Heartbeat Rate"},
    {FieldId::PositionRate, "Position Rate"},
    {FieldId::Nic, "NIC"},
    {FieldId::Nacp, "NACp"},
    {FieldId::DebugLogging, "Debug Logging"},
    {FieldId::LogMessages, "Log Messages"},
}};

struct Rect {
  int l = 0;
  int t = 0;
  int r = 0;
  int b = 0;

  bool Contains(int x, int y) const {
    return x >= l && x <= r && y <= t && y >= b;
  }
};

struct PluginState {
  std::unique_ptr<gdl90::GDL90Encoder> encoder;
  std::unique_ptr<udp::UDPBroadcaster> broadcaster;
  std::unique_ptr<config::ConfigManager> config_manager;

  XPLMDataRef lat_ref = nullptr;
  XPLMDataRef lon_ref = nullptr;
  XPLMDataRef alt_ref = nullptr;
  XPLMDataRef speed_ref = nullptr;
  XPLMDataRef track_ref = nullptr;
  XPLMDataRef vs_ref = nullptr;
  XPLMDataRef airborne_ref = nullptr;
  XPLMDataRef sim_time_ref = nullptr;
  XPLMDataRef tailnum_ref = nullptr;

  float last_heartbeat = 0.0f;
  float last_position = 0.0f;

  bool initialized = false;
  bool enabled = false;

  XPLMMenuID menu_id = nullptr;
  int menu_item_enable = 0;
  int menu_item_settings = 0;

  XPLMWindowID window_id = nullptr;
  bool window_visible = false;
  int active_field = -1;
  std::string edit_buffer;
  std::array<std::string, static_cast<size_t>(FieldId::Count)> field_values;
  std::string config_path;
};

PluginState g_state;

float FlightLoopCallback(float in_elapsed_since_last_call,
                         float in_elapsed_time_since_last_flight_loop,
                         int in_counter,
                         void* in_refcon);
void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref);
void DrawSettingsWindow(XPLMWindowID in_window_id, void* in_refcon);
int HandleSettingsMouseClick(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             XPLMMouseStatus status,
                             void* in_refcon);
void HandleSettingsKey(XPLMWindowID in_window_id,
                       char in_key,
                       XPLMKeyFlags in_flags,
                       char in_virtual_key,
                       void* in_refcon,
                       int losing_focus);
XPLMCursorStatus HandleSettingsCursor(XPLMWindowID in_window_id,
                                      int x,
                                      int y,
                                      void* in_refcon);
int HandleSettingsMouseWheel(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             int wheel,
                             int clicks,
                             void* in_refcon);
int HandleSettingsRightClick(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             XPLMMouseStatus status,
                             void* in_refcon);

void SyncFieldsFromConfig();
bool ApplyFieldsToConfig();
bool SaveConfig();
void ShowSettingsWindow(bool show);
Rect GetFieldRect(int index, int left, int top);
Rect GetButtonRect(int index, int left, int bottom);
std::string FormatHex24(uint32_t value);
std::string ToLower(const std::string& value);
bool ReloadConfigFromDisk();

void LogMessage(const std::string& message) {
  XPLMDebugString(("[XP2GDL90] " + message + "\n").c_str());
}

std::string Trim(const std::string& input) {
  const size_t start = input.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = input.find_last_not_of(" \t\r\n");
  return input.substr(start, end - start + 1);
}

std::string ReadTailNumber() {
  if (!g_state.tailnum_ref) {
    return "";
  }

  char buffer[40];
  const int bytes_read = XPLMGetDatab(g_state.tailnum_ref, buffer, 0,
                                     static_cast<int>(sizeof(buffer) - 1));
  if (bytes_read <= 0) {
    return "";
  }

  buffer[bytes_read] = '\0';
  return Trim(std::string(buffer));
}

std::string NormalizeSystemPath(const std::string& raw_path) {
  std::string path = raw_path;
#if APL
  if (!path.empty() && path.find(':') != std::string::npos) {
    const size_t first_colon = path.find(':');
    if (first_colon != std::string::npos) {
      path = "/" + path.substr(first_colon + 1);
      for (char& ch : path) {
        if (ch == ':') {
          ch = '/';
        }
      }
    }
  }
#endif

  if (!path.empty() && path.back() != '/') {
    path.push_back('/');
  }

  return path;
}

bool VerifyDataRef(XPLMDataRef ref, const char* name) {
  if (ref) {
    return true;
  }
  LogMessage(std::string("ERROR: Missing dataref ") + name);
  return false;
}

gdl90::PositionData GetOwnshipData(const config::Config& cfg) {
  gdl90::PositionData data;

  data.latitude = XPLMGetDatad(g_state.lat_ref);
  data.longitude = XPLMGetDatad(g_state.lon_ref);

  const double altitude_meters = XPLMGetDatad(g_state.alt_ref);
  data.altitude = static_cast<int32_t>(altitude_meters * kMetersToFeet);

  const float speed_ms = XPLMGetDataf(g_state.speed_ref);
  data.h_velocity = static_cast<uint16_t>(speed_ms * kMetersPerSecondToKnots);

  data.v_velocity = static_cast<int16_t>(XPLMGetDataf(g_state.vs_ref));

  data.track = static_cast<uint16_t>(XPLMGetDataf(g_state.track_ref));
  data.track_type = gdl90::TrackType::TRUE_TRACK;

  const int on_ground = XPLMGetDatai(g_state.airborne_ref);
  data.airborne = (on_ground == 0);

  data.icao_address = cfg.icao_address;

  const std::string tail_number = ReadTailNumber();
  data.callsign = tail_number.empty() ? cfg.callsign : tail_number;

  data.emitter_category =
      static_cast<gdl90::EmitterCategory>(cfg.emitter_category);
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.nic = cfg.nic;
  data.nacp = cfg.nacp;
  data.alert_status = 0;
  data.emergency_code = 0;

  return data;
}

std::string FormatHex24(uint32_t value) {
  std::ostringstream oss;
  oss << "0x" << std::uppercase << std::hex << std::setw(6)
      << std::setfill('0') << (value & 0xFFFFFF);
  return oss.str();
}

std::string ToLower(const std::string& value) {
  std::string lowered = value;
  for (char& ch : lowered) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return lowered;
}

void SyncFieldsFromConfig() {
  const config::Config& cfg = g_state.config_manager->getConfig();
  g_state.field_values[static_cast<size_t>(FieldId::TargetIp)] = cfg.target_ip;
  g_state.field_values[static_cast<size_t>(FieldId::TargetPort)] =
      std::to_string(cfg.target_port);
  g_state.field_values[static_cast<size_t>(FieldId::IcaoAddress)] =
      FormatHex24(cfg.icao_address);
  g_state.field_values[static_cast<size_t>(FieldId::Callsign)] = cfg.callsign;
  g_state.field_values[static_cast<size_t>(FieldId::EmitterCategory)] =
      std::to_string(static_cast<int>(cfg.emitter_category));
  g_state.field_values[static_cast<size_t>(FieldId::HeartbeatRate)] =
      std::to_string(cfg.heartbeat_rate);
  g_state.field_values[static_cast<size_t>(FieldId::PositionRate)] =
      std::to_string(cfg.position_rate);
  g_state.field_values[static_cast<size_t>(FieldId::Nic)] =
      std::to_string(static_cast<int>(cfg.nic));
  g_state.field_values[static_cast<size_t>(FieldId::Nacp)] =
      std::to_string(static_cast<int>(cfg.nacp));
  g_state.field_values[static_cast<size_t>(FieldId::DebugLogging)] =
      cfg.debug_logging ? "true" : "false";
  g_state.field_values[static_cast<size_t>(FieldId::LogMessages)] =
      cfg.log_messages ? "true" : "false";
}

bool ParseBool(const std::string& value, bool* out_value) {
  const std::string lowered = ToLower(Trim(value));
  if (lowered == "true" || lowered == "1" || lowered == "yes" ||
      lowered == "on") {
    *out_value = true;
    return true;
  }
  if (lowered == "false" || lowered == "0" || lowered == "no" ||
      lowered == "off") {
    *out_value = false;
    return true;
  }
  return false;
}

bool ApplyFieldsToConfig() {
  config::Config new_cfg = g_state.config_manager->getConfig();

  try {
    new_cfg.target_ip =
        Trim(g_state.field_values[static_cast<size_t>(FieldId::TargetIp)]);
    if (new_cfg.target_ip.empty()) {
      LogMessage("ERROR: Target IP cannot be empty");
      return false;
    }

    const unsigned long port =
        std::stoul(g_state.field_values[static_cast<size_t>(FieldId::TargetPort)]);
    if (port == 0 || port > 65535) {
      LogMessage("ERROR: Target port must be 1-65535");
      return false;
    }
    new_cfg.target_port = static_cast<uint16_t>(port);

    std::string icao_value =
        Trim(g_state.field_values[static_cast<size_t>(FieldId::IcaoAddress)]);
    if (icao_value.rfind("0x", 0) == 0 || icao_value.rfind("0X", 0) == 0) {
      new_cfg.icao_address = std::stoul(icao_value, nullptr, 16);
    } else {
      new_cfg.icao_address = std::stoul(icao_value);
    }
    new_cfg.icao_address &= 0xFFFFFF;

    new_cfg.callsign =
        Trim(g_state.field_values[static_cast<size_t>(FieldId::Callsign)])
            .substr(0, 8);

    new_cfg.emitter_category = static_cast<uint8_t>(std::stoi(
        g_state.field_values[static_cast<size_t>(FieldId::EmitterCategory)]));

    const float heartbeat_rate = std::stof(
        g_state.field_values[static_cast<size_t>(FieldId::HeartbeatRate)]);
    if (heartbeat_rate <= 0.0f) {
      LogMessage("ERROR: Heartbeat rate must be > 0");
      return false;
    }
    new_cfg.heartbeat_rate = heartbeat_rate;

    const float position_rate = std::stof(
        g_state.field_values[static_cast<size_t>(FieldId::PositionRate)]);
    if (position_rate <= 0.0f) {
      LogMessage("ERROR: Position rate must be > 0");
      return false;
    }
    new_cfg.position_rate = position_rate;

    new_cfg.nic = static_cast<uint8_t>(
        std::stoi(g_state.field_values[static_cast<size_t>(FieldId::Nic)]));
    new_cfg.nacp = static_cast<uint8_t>(
        std::stoi(g_state.field_values[static_cast<size_t>(FieldId::Nacp)]));

    bool debug_logging = false;
    if (!ParseBool(g_state.field_values[static_cast<size_t>(FieldId::DebugLogging)],
                   &debug_logging)) {
      LogMessage("ERROR: Debug logging must be true/false");
      return false;
    }
    new_cfg.debug_logging = debug_logging;

    bool log_messages = false;
    if (!ParseBool(g_state.field_values[static_cast<size_t>(FieldId::LogMessages)],
                   &log_messages)) {
      LogMessage("ERROR: Log messages must be true/false");
      return false;
    }
    new_cfg.log_messages = log_messages;
  } catch (const std::exception& e) {
    LogMessage(std::string("ERROR: Invalid setting: ") + e.what());
    return false;
  }

  const config::Config old_cfg = g_state.config_manager->getConfig();
  const bool target_changed = (old_cfg.target_ip != new_cfg.target_ip) ||
                              (old_cfg.target_port != new_cfg.target_port);

  if (target_changed) {
    auto new_broadcaster =
        std::make_unique<udp::UDPBroadcaster>(new_cfg.target_ip,
                                              new_cfg.target_port);
    if (!new_broadcaster->initialize()) {
      LogMessage("ERROR: Failed to reinitialize UDP broadcaster: " +
                 new_broadcaster->getLastError());
      return false;
    }
    g_state.broadcaster = std::move(new_broadcaster);
    LogMessage("UDP broadcaster updated: " + new_cfg.target_ip + ":" +
               std::to_string(new_cfg.target_port));
  }

  g_state.config_manager->getConfig() = new_cfg;
  return true;
}

bool SaveConfig() {
  if (!g_state.config_manager->save(g_state.config_path)) {
    LogMessage("ERROR: Failed to save config: " +
               g_state.config_manager->getLastError());
    return false;
  }
  LogMessage("Configuration saved");
  return true;
}

bool ReloadConfigFromDisk() {
  config::Config previous = g_state.config_manager->getConfig();
  if (!g_state.config_manager->load(g_state.config_path)) {
    LogMessage("ERROR: Failed to reload config: " +
               g_state.config_manager->getLastError());
    return false;
  }

  const config::Config& cfg = g_state.config_manager->getConfig();
  const bool target_changed = (previous.target_ip != cfg.target_ip) ||
                              (previous.target_port != cfg.target_port);
  if (target_changed) {
    auto new_broadcaster =
        std::make_unique<udp::UDPBroadcaster>(cfg.target_ip, cfg.target_port);
    if (!new_broadcaster->initialize()) {
      LogMessage("ERROR: Failed to reinitialize UDP broadcaster: " +
                 new_broadcaster->getLastError());
      g_state.config_manager->getConfig() = previous;
      return false;
    }
    g_state.broadcaster = std::move(new_broadcaster);
    LogMessage("UDP broadcaster updated: " + cfg.target_ip + ":" +
               std::to_string(cfg.target_port));
  }

  SyncFieldsFromConfig();
  LogMessage("Configuration reloaded from disk");
  return true;
}

Rect GetFieldRect(int index, int left, int top) {
  const int row_height = 22;
  const int field_left = left + 160;
  const int field_right = left + 360;
  const int field_top = top - 50 - (index * row_height);
  return {field_left, field_top, field_right, field_top - 18};
}

Rect GetButtonRect(int index, int left, int bottom) {
  const int button_width = 90;
  const int button_height = 22;
  const int spacing = 10;
  const int start_left = left + 20;
  const int button_left = start_left + index * (button_width + spacing);
  return {button_left, bottom + 12 + button_height, button_left + button_width,
          bottom + 12};
}

void ShowSettingsWindow(bool show) {
  if (!g_state.window_id) {
    return;
  }
  g_state.window_visible = show;
  XPLMSetWindowIsVisible(g_state.window_id, show ? 1 : 0);
  if (show) {
    SyncFieldsFromConfig();
    g_state.active_field = -1;
    g_state.edit_buffer.clear();
    XPLMBringWindowToFront(g_state.window_id);
  }
}

void DrawSettingsWindow(XPLMWindowID in_window_id, void* in_refcon) {
  (void)in_refcon;

  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  XPLMGetWindowGeometry(in_window_id, &left, &top, &right, &bottom);

  XPLMDrawTranslucentDarkBox(left, top, right, bottom);

  float color_white[] = {1.0f, 1.0f, 1.0f};
  float color_yellow[] = {1.0f, 0.85f, 0.1f};

  XPLMDrawString(color_white, left + 14, top - 24,
                 const_cast<char*>("XP2GDL90 Settings"), nullptr,
                 xplmFont_Proportional);

  for (size_t i = 0; i < kFieldDefs.size(); ++i) {
    const Rect rect = GetFieldRect(static_cast<int>(i), left, top);
    XPLMDrawString(color_white, left + 20, rect.t - 12,
                   const_cast<char*>(kFieldDefs[i].label), nullptr,
                   xplmFont_Proportional);

    const bool active = (g_state.active_field == static_cast<int>(i));
    XPLMDrawTranslucentDarkBox(rect.l, rect.t, rect.r, rect.b);

    const std::string& value = active ? g_state.edit_buffer
                                      : g_state.field_values[i];
    XPLMDrawString(active ? color_yellow : color_white, rect.l + 6,
                   rect.t - 12, const_cast<char*>(value.c_str()), nullptr,
                   xplmFont_Proportional);
  }

  const Rect apply_rect = GetButtonRect(0, left, bottom);
  const Rect save_rect = GetButtonRect(1, left, bottom);
  const Rect close_rect = GetButtonRect(2, left, bottom);

  XPLMDrawTranslucentDarkBox(apply_rect.l, apply_rect.t, apply_rect.r,
                            apply_rect.b);
  XPLMDrawTranslucentDarkBox(save_rect.l, save_rect.t, save_rect.r,
                            save_rect.b);
  XPLMDrawTranslucentDarkBox(close_rect.l, close_rect.t, close_rect.r,
                            close_rect.b);

  XPLMDrawString(color_white, apply_rect.l + 20, apply_rect.t - 15,
                 const_cast<char*>("Apply"), nullptr, xplmFont_Proportional);
  XPLMDrawString(color_white, save_rect.l + 24, save_rect.t - 15,
                 const_cast<char*>("Save"), nullptr, xplmFont_Proportional);
  XPLMDrawString(color_white, close_rect.l + 20, close_rect.t - 15,
                 const_cast<char*>("Close"), nullptr, xplmFont_Proportional);

  const config::Config& cfg = g_state.config_manager->getConfig();
  std::string status = g_state.enabled ? "Broadcasting: ON"
                                       : "Broadcasting: OFF";
  status += " (" + cfg.target_ip + ":" + std::to_string(cfg.target_port) + ")";
  XPLMDrawString(color_white, left + 20, bottom + 42,
                 const_cast<char*>(status.c_str()), nullptr,
                 xplmFont_Proportional);
}

int HandleSettingsMouseClick(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             XPLMMouseStatus status,
                             void* in_refcon) {
  (void)in_window_id;
  (void)in_refcon;

  if (status != xplm_MouseDown) {
    return 1;
  }

  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  XPLMGetWindowGeometry(g_state.window_id, &left, &top, &right, &bottom);

  const Rect apply_rect = GetButtonRect(0, left, bottom);
  const Rect save_rect = GetButtonRect(1, left, bottom);
  const Rect close_rect = GetButtonRect(2, left, bottom);

  if (apply_rect.Contains(x, y)) {
    if (g_state.active_field >= 0) {
      g_state.field_values[static_cast<size_t>(g_state.active_field)] =
          g_state.edit_buffer;
      g_state.active_field = -1;
      g_state.edit_buffer.clear();
    }
    if (ApplyFieldsToConfig()) {
      SyncFieldsFromConfig();
    }
    return 1;
  }
  if (save_rect.Contains(x, y)) {
    if (g_state.active_field >= 0) {
      g_state.field_values[static_cast<size_t>(g_state.active_field)] =
          g_state.edit_buffer;
      g_state.active_field = -1;
      g_state.edit_buffer.clear();
    }
    if (ApplyFieldsToConfig()) {
      SyncFieldsFromConfig();
      SaveConfig();
    }
    return 1;
  }
  if (close_rect.Contains(x, y)) {
    ShowSettingsWindow(false);
    return 1;
  }

  for (size_t i = 0; i < kFieldDefs.size(); ++i) {
    const Rect rect = GetFieldRect(static_cast<int>(i), left, top);
    if (rect.Contains(x, y)) {
      g_state.active_field = static_cast<int>(i);
      g_state.edit_buffer = g_state.field_values[i];
      XPLMTakeKeyboardFocus(g_state.window_id);
      return 1;
    }
  }

  g_state.active_field = -1;
  g_state.edit_buffer.clear();
  return 1;
}

void HandleSettingsKey(XPLMWindowID in_window_id,
                       char in_key,
                       XPLMKeyFlags in_flags,
                       char in_virtual_key,
                       void* in_refcon,
                       int losing_focus) {
  (void)in_window_id;
  (void)in_flags;
  (void)in_virtual_key;
  (void)in_refcon;

  if (losing_focus) {
    return;
  }

  if ((in_flags & xplm_DownFlag) == 0) {
    return;
  }

  if (g_state.active_field < 0) {
    return;
  }

  if (in_virtual_key == XPLM_VK_BACK) {
    if (!g_state.edit_buffer.empty()) {
      g_state.edit_buffer.pop_back();
    }
    return;
  }

  if (in_virtual_key == XPLM_VK_RETURN || in_key == '\r') {
    g_state.field_values[static_cast<size_t>(g_state.active_field)] =
        g_state.edit_buffer;
    g_state.active_field = -1;
    g_state.edit_buffer.clear();
    return;
  }

  if (in_virtual_key == XPLM_VK_TAB) {
    g_state.field_values[static_cast<size_t>(g_state.active_field)] =
        g_state.edit_buffer;
    g_state.active_field =
        (g_state.active_field + 1) % static_cast<int>(FieldId::Count);
    g_state.edit_buffer =
        g_state.field_values[static_cast<size_t>(g_state.active_field)];
    return;
  }

  if (in_virtual_key == XPLM_VK_ESCAPE) {
    g_state.active_field = -1;
    g_state.edit_buffer.clear();
    return;
  }

  if (in_key >= 32 && in_key <= 126) {
    if (g_state.edit_buffer.size() < 64) {
      g_state.edit_buffer.push_back(in_key);
    }
  }
}

XPLMCursorStatus HandleSettingsCursor(XPLMWindowID in_window_id,
                                      int x,
                                      int y,
                                      void* in_refcon) {
  (void)in_window_id;
  (void)x;
  (void)y;
  (void)in_refcon;
  return xplm_CursorDefault;
}

int HandleSettingsMouseWheel(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             int wheel,
                             int clicks,
                             void* in_refcon) {
  (void)in_window_id;
  (void)x;
  (void)y;
  (void)wheel;
  (void)clicks;
  (void)in_refcon;
  return 0;
}

int HandleSettingsRightClick(XPLMWindowID in_window_id,
                             int x,
                             int y,
                             XPLMMouseStatus status,
                             void* in_refcon) {
  (void)in_window_id;
  (void)x;
  (void)y;
  (void)status;
  (void)in_refcon;
  return 0;
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
  std::strcpy(outName, "XP2GDL90");
  std::strcpy(outSig, "com.xp2gdl90.plugin");
  std::strcpy(outDesc, "GDL90 ADS-B data broadcaster for EFB applications");

  LogMessage("Plugin starting...");

  g_state.initialized = false;
  g_state.enabled = false;
  g_state.last_heartbeat = 0.0f;
  g_state.last_position = 0.0f;

  g_state.encoder = std::make_unique<gdl90::GDL90Encoder>();
  g_state.config_manager = std::make_unique<config::ConfigManager>();

  char path[512];
  XPLMGetSystemPath(path);
  const std::string system_path = NormalizeSystemPath(path);
  const std::string config_path =
      system_path + "Resources/plugins/xp2gdl90/xp2gdl90.ini";
  g_state.config_path = config_path;

  if (!g_state.config_manager->load(config_path)) {
    LogMessage("Warning: Could not load config file, using defaults");
    g_state.config_manager->save(config_path);
    LogMessage("Default configuration saved");
  }

  const config::Config& cfg = g_state.config_manager->getConfig();

  g_state.broadcaster =
      std::make_unique<udp::UDPBroadcaster>(cfg.target_ip, cfg.target_port);
  if (!g_state.broadcaster->initialize()) {
    LogMessage("ERROR: Failed to initialize UDP broadcaster: " +
               g_state.broadcaster->getLastError());
    return 0;
  }
  LogMessage("UDP broadcaster initialized: " + cfg.target_ip + ":" +
             std::to_string(cfg.target_port));

  g_state.lat_ref = XPLMFindDataRef("sim/flightmodel/position/latitude");
  g_state.lon_ref = XPLMFindDataRef("sim/flightmodel/position/longitude");
  g_state.alt_ref = XPLMFindDataRef("sim/flightmodel/position/elevation");
  g_state.speed_ref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
  g_state.track_ref = XPLMFindDataRef("sim/flightmodel/position/true_psi");
  g_state.vs_ref = XPLMFindDataRef("sim/flightmodel/position/vh_ind_fpm");
  g_state.airborne_ref = XPLMFindDataRef("sim/flightmodel/failures/onground_any");
  g_state.sim_time_ref = XPLMFindDataRef("sim/time/total_flight_time_sec");
  g_state.tailnum_ref = XPLMFindDataRef("sim/aircraft/view/acf_tailnum");

  bool datarefs_ok = true;
  datarefs_ok &= VerifyDataRef(g_state.lat_ref,
                               "sim/flightmodel/position/latitude");
  datarefs_ok &= VerifyDataRef(g_state.lon_ref,
                               "sim/flightmodel/position/longitude");
  datarefs_ok &= VerifyDataRef(g_state.alt_ref,
                               "sim/flightmodel/position/elevation");
  datarefs_ok &= VerifyDataRef(g_state.speed_ref,
                               "sim/flightmodel/position/groundspeed");
  datarefs_ok &= VerifyDataRef(g_state.track_ref,
                               "sim/flightmodel/position/true_psi");
  datarefs_ok &= VerifyDataRef(g_state.vs_ref,
                               "sim/flightmodel/position/vh_ind_fpm");
  datarefs_ok &= VerifyDataRef(g_state.airborne_ref,
                               "sim/flightmodel/failures/onground_any");
  datarefs_ok &= VerifyDataRef(g_state.sim_time_ref,
                               "sim/time/total_flight_time_sec");

  if (!datarefs_ok) {
    LogMessage("ERROR: Failed to find required datarefs");
    return 0;
  }

  const int menu_container =
      XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XP2GDL90", nullptr, 0);
  g_state.menu_id = XPLMCreateMenu("XP2GDL90", XPLMFindPluginsMenu(),
                                  menu_container, MenuHandlerCallback,
                                  nullptr);
  g_state.menu_item_enable =
      XPLMAppendMenuItem(g_state.menu_id, "Enable Broadcasting",
                         reinterpret_cast<void*>(1), 0);
  g_state.menu_item_settings =
      XPLMAppendMenuItem(g_state.menu_id, "Settings...",
                         reinterpret_cast<void*>(2), 0);
  XPLMAppendMenuItem(g_state.menu_id, "Reload Config",
                     reinterpret_cast<void*>(3), 0);

  int screen_left = 0;
  int screen_top = 0;
  int screen_right = 0;
  int screen_bottom = 0;
  XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right,
                            &screen_bottom);
  const int window_width = 420;
  const int window_height = 340;
  const int window_left =
      screen_left + (screen_right - screen_left - window_width) / 2;
  const int window_top =
      screen_top - (screen_top - screen_bottom - window_height) / 2;
  const int window_right = window_left + window_width;
  const int window_bottom = window_top - window_height;

  XPLMCreateWindow_t window_params = {};
  window_params.structSize = sizeof(window_params);
  window_params.left = window_left;
  window_params.top = window_top;
  window_params.right = window_right;
  window_params.bottom = window_bottom;
  window_params.visible = 0;
  window_params.drawWindowFunc = DrawSettingsWindow;
  window_params.handleMouseClickFunc = HandleSettingsMouseClick;
  window_params.handleKeyFunc = HandleSettingsKey;
  window_params.handleCursorFunc = HandleSettingsCursor;
  window_params.handleMouseWheelFunc = HandleSettingsMouseWheel;
  window_params.handleRightClickFunc = HandleSettingsRightClick;
  window_params.refcon = nullptr;
  window_params.layer = xplm_WindowLayerFloatingWindows;
#if defined(XPLM301)
  window_params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
#endif
  g_state.window_id = XPLMCreateWindowEx(&window_params);
  XPLMSetWindowTitle(g_state.window_id, "XP2GDL90 Settings");
  XPLMSetWindowPositioningMode(g_state.window_id, xplm_WindowPositionFree, -1);
  XPLMSetWindowResizingLimits(g_state.window_id, window_width, window_height,
                              window_width, window_height);

  g_state.initialized = true;
  LogMessage("Plugin initialized successfully");

  return 1;
}

PLUGIN_API void XPluginStop(void) {
  LogMessage("Plugin stopping...");

  if (g_state.enabled) {
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);
  }

  g_state.broadcaster.reset();
  g_state.encoder.reset();
  g_state.config_manager.reset();
  if (g_state.window_id) {
    XPLMDestroyWindow(g_state.window_id);
    g_state.window_id = nullptr;
  }

  LogMessage("Plugin stopped");
}

PLUGIN_API int XPluginEnable(void) {
  if (!g_state.initialized) {
    return 0;
  }

  LogMessage("Enabling plugin...");

  XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, nullptr);

  g_state.enabled = true;
  XPLMCheckMenuItem(g_state.menu_id, g_state.menu_item_enable,
                    xplm_Menu_Checked);

  LogMessage("Plugin enabled - Broadcasting GDL90 data");
  return 1;
}

PLUGIN_API void XPluginDisable(void) {
  LogMessage("Disabling plugin...");

  XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);

  g_state.enabled = false;
  XPLMCheckMenuItem(g_state.menu_id, g_state.menu_item_enable,
                    xplm_Menu_Unchecked);

  LogMessage("Plugin disabled");
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg,
                                      void* inParam) {
  (void)inFrom;
  (void)inMsg;
  (void)inParam;
}

namespace {

float FlightLoopCallback(float in_elapsed_since_last_call,
                         float in_elapsed_time_since_last_flight_loop,
                         int in_counter,
                         void* in_refcon) {
  (void)in_elapsed_since_last_call;
  (void)in_elapsed_time_since_last_flight_loop;
  (void)in_counter;
  (void)in_refcon;

  if (!g_state.enabled || !g_state.initialized) {
    return -1.0f;
  }

  const float sim_time = XPLMGetDataf(g_state.sim_time_ref);
  const config::Config& cfg = g_state.config_manager->getConfig();

  if (cfg.heartbeat_rate > 0.0f &&
      sim_time - g_state.last_heartbeat >= (1.0f / cfg.heartbeat_rate)) {
    const auto heartbeat = g_state.encoder->createHeartbeat(true, true);
    g_state.broadcaster->send(heartbeat);
    g_state.last_heartbeat = sim_time;
  }

  if (cfg.position_rate > 0.0f &&
      sim_time - g_state.last_position >= (1.0f / cfg.position_rate)) {
    const gdl90::PositionData ownship = GetOwnshipData(cfg);
    const auto position_msg = g_state.encoder->createOwnshipReport(ownship);
    g_state.broadcaster->send(position_msg);
    g_state.last_position = sim_time;
  }

  return -1.0f;
}

void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref) {
  (void)in_menu_ref;

  const intptr_t item = reinterpret_cast<intptr_t>(in_item_ref);
  if (item != 1) {
    if (item == 2) {
      ShowSettingsWindow(!g_state.window_visible);
    } else if (item == 3) {
      ReloadConfigFromDisk();
    }
    return;
  }

  if (g_state.enabled) {
    XPluginDisable();
  } else {
    XPluginEnable();
  }
}

}  // namespace
