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
#include <XPLMMenus.h>
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>
#include <XPStandardWidgets.h>
#include <XPWidgets.h>

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

  XPWidgetID settings_widget = nullptr;
  XPWidgetID settings_panel = nullptr;
  XPWidgetID status_caption = nullptr;
  XPWidgetID apply_button = nullptr;
  XPWidgetID save_button = nullptr;
  XPWidgetID close_button = nullptr;
  std::array<XPWidgetID, static_cast<size_t>(FieldId::Count)> field_widgets = {};
  std::array<std::string, static_cast<size_t>(FieldId::Count)> field_values;
  std::string config_path;
};

PluginState g_state;

float FlightLoopCallback(float in_elapsed_since_last_call,
                         float in_elapsed_time_since_last_flight_loop,
                         int in_counter,
                         void* in_refcon);
void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref);
int SettingsWidgetCallback(XPWidgetMessage in_message,
                           XPWidgetID in_widget,
                           intptr_t in_param1,
                           intptr_t in_param2);

void SyncFieldsFromConfig();
bool ApplyFieldsToConfig();
bool SaveConfig();
void ShowSettingsWindow(bool show);
void CreateSettingsWindow();
void DestroySettingsWindow();
void UpdateStatusCaption();
void ReadFieldsFromWidgets();
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

bool IsCheckboxField(FieldId field_id) {
  return field_id == FieldId::DebugLogging || field_id == FieldId::LogMessages;
}

void UpdateStatusCaption() {
  if (!g_state.status_caption) {
    return;
  }
  const config::Config& cfg = g_state.config_manager->getConfig();
  std::string status = g_state.enabled ? "Broadcasting: ON"
                                       : "Broadcasting: OFF";
  status += " (" + cfg.target_ip + ":" + std::to_string(cfg.target_port) + ")";
  XPSetWidgetDescriptor(g_state.status_caption, status.c_str());
}

void ReadFieldsFromWidgets() {
  if (!g_state.settings_widget) {
    return;
  }

  char buffer[128] = {};
  for (size_t i = 0; i < kFieldDefs.size(); ++i) {
    const FieldId field_id = kFieldDefs[i].id;
    XPWidgetID widget = g_state.field_widgets[i];
    if (!widget) {
      continue;
    }

    if (IsCheckboxField(field_id)) {
      const int state =
          static_cast<int>(XPGetWidgetProperty(widget, xpProperty_ButtonState,
                                               nullptr));
      g_state.field_values[i] = state ? "true" : "false";
      continue;
    }

    const int length =
        XPGetWidgetDescriptor(widget, buffer, static_cast<int>(sizeof(buffer)));
    if (length > 0 && length < static_cast<int>(sizeof(buffer))) {
      buffer[length] = '\0';
    } else if (length <= 0) {
      buffer[0] = '\0';
    } else {
      buffer[sizeof(buffer) - 1] = '\0';
    }
    g_state.field_values[i] = buffer;
  }
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

  if (!g_state.settings_widget) {
    return;
  }

  for (size_t i = 0; i < kFieldDefs.size(); ++i) {
    const FieldId field_id = kFieldDefs[i].id;
    XPWidgetID widget = g_state.field_widgets[i];
    if (!widget) {
      continue;
    }

    if (IsCheckboxField(field_id)) {
      const bool enabled = (g_state.field_values[i] == "true");
      XPSetWidgetProperty(widget, xpProperty_ButtonState, enabled ? 1 : 0);
    } else {
      XPSetWidgetDescriptor(widget, g_state.field_values[i].c_str());
    }
  }

  UpdateStatusCaption();
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

void ShowSettingsWindow(bool show) {
  if (!g_state.settings_widget) {
    CreateSettingsWindow();
  }
  if (!g_state.settings_widget) {
    return;
  }
  if (show) {
    SyncFieldsFromConfig();
    XPShowWidget(g_state.settings_widget);
    XPLMWindowID window = XPGetWidgetUnderlyingWindow(g_state.settings_widget);
    if (window) {
      XPLMBringWindowToFront(window);
    }
  } else {
    XPHideWidget(g_state.settings_widget);
  }
}

void CreateSettingsWindow() {
  if (g_state.settings_widget) {
    return;
  }

  XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);

  int screen_left = 0;
  int screen_top = 0;
  int screen_right = 0;
  int screen_bottom = 0;
  XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right,
                            &screen_bottom);

  const int window_width = 460;
  const int window_height = 360;
  const int window_left =
      screen_left + (screen_right - screen_left - window_width) / 2;
  const int window_top =
      screen_top - (screen_top - screen_bottom - window_height) / 2;
  const int window_right = window_left + window_width;
  const int window_bottom = window_top - window_height;

  g_state.settings_widget = XPCreateWidget(window_left, window_top,
                                           window_right, window_bottom, 0,
                                           "XP2GDL90 Settings", 1, nullptr,
                                           xpWidgetClass_MainWindow);
  if (!g_state.settings_widget) {
    return;
  }

  XPSetWidgetProperty(g_state.settings_widget, xpProperty_MainWindowType,
                      xpMainWindowStyle_Translucent);
  XPSetWidgetProperty(g_state.settings_widget, xpProperty_MainWindowHasCloseBoxes,
                      1);
  XPAddWidgetCallback(g_state.settings_widget, SettingsWidgetCallback);

  const int panel_left = window_left + 10;
  const int panel_top = window_top - 30;
  const int panel_right = window_right - 10;
  const int panel_bottom = window_bottom + 60;
  g_state.settings_panel = XPCreateWidget(panel_left, panel_top, panel_right,
                                          panel_bottom, 1, "", 0,
                                          g_state.settings_widget,
                                          xpWidgetClass_SubWindow);
  XPSetWidgetProperty(g_state.settings_panel, xpProperty_SubWindowType,
                      xpSubWindowStyle_SubWindow);

  const int label_left = panel_left + 10;
  const int label_right = label_left + 150;
  const int field_left = label_right + 10;
  const int field_right = field_left + 210;
  const int row_height = 22;
  const int first_row_top = panel_top - 20;

  for (size_t i = 0; i < kFieldDefs.size(); ++i) {
    const int row_top = first_row_top - static_cast<int>(i) * row_height;
    const int row_bottom = row_top - 18;

    XPCreateWidget(label_left, row_top, label_right, row_bottom, 1,
                   kFieldDefs[i].label, 0, g_state.settings_panel,
                   xpWidgetClass_Caption);

    if (IsCheckboxField(kFieldDefs[i].id)) {
      XPWidgetID checkbox = XPCreateWidget(field_left, row_top, field_left + 20,
                                           row_bottom, 1, "", 0,
                                           g_state.settings_panel,
                                           xpWidgetClass_Button);
      XPSetWidgetProperty(checkbox, xpProperty_ButtonType, xpRadioButton);
      XPSetWidgetProperty(checkbox, xpProperty_ButtonBehavior,
                          xpButtonBehaviorCheckBox);
      g_state.field_widgets[i] = checkbox;
    } else {
      XPWidgetID field = XPCreateWidget(field_left, row_top, field_right,
                                        row_bottom, 1, "", 0,
                                        g_state.settings_panel,
                                        xpWidgetClass_TextField);
      XPSetWidgetProperty(field, xpProperty_TextFieldType, xpTextEntryField);
      XPSetWidgetProperty(field, xpProperty_MaxCharacters, 64);
      g_state.field_widgets[i] = field;
    }
  }

  const int button_top = panel_bottom - 10;
  const int button_bottom = button_top - 20;
  const int button_width = 90;
  const int button_gap = 10;
  const int button_left = panel_left + 10;
  g_state.apply_button = XPCreateWidget(button_left, button_top,
                                        button_left + button_width,
                                        button_bottom, 1, "Apply", 0,
                                        g_state.settings_panel,
                                        xpWidgetClass_Button);
  XPSetWidgetProperty(g_state.apply_button, xpProperty_ButtonType, xpPushButton);
  XPSetWidgetProperty(g_state.apply_button, xpProperty_ButtonBehavior,
                      xpButtonBehaviorPushButton);

  const int save_left = button_left + button_width + button_gap;
  g_state.save_button = XPCreateWidget(save_left, button_top,
                                       save_left + button_width, button_bottom,
                                       1, "Save", 0,
                                       g_state.settings_panel,
                                       xpWidgetClass_Button);
  XPSetWidgetProperty(g_state.save_button, xpProperty_ButtonType, xpPushButton);
  XPSetWidgetProperty(g_state.save_button, xpProperty_ButtonBehavior,
                      xpButtonBehaviorPushButton);

  const int close_left = save_left + button_width + button_gap;
  g_state.close_button = XPCreateWidget(close_left, button_top,
                                        close_left + button_width,
                                        button_bottom, 1, "Close", 0,
                                        g_state.settings_panel,
                                        xpWidgetClass_Button);
  XPSetWidgetProperty(g_state.close_button, xpProperty_ButtonType, xpPushButton);
  XPSetWidgetProperty(g_state.close_button, xpProperty_ButtonBehavior,
                      xpButtonBehaviorPushButton);

  const int status_top = panel_bottom + 25;
  const int status_bottom = status_top - 18;
  g_state.status_caption =
      XPCreateWidget(panel_left + 10, status_top, panel_right, status_bottom, 1,
                     "", 0, g_state.settings_panel, xpWidgetClass_Caption);

  XPLMWindowID window = XPGetWidgetUnderlyingWindow(g_state.settings_widget);
  if (window) {
    XPLMSetWindowPositioningMode(window, xplm_WindowPositionFree, -1);
  }

  SyncFieldsFromConfig();
}

void DestroySettingsWindow() {
  if (!g_state.settings_widget) {
    return;
  }
  XPDestroyWidget(g_state.settings_widget, 1);
  g_state.settings_widget = nullptr;
  g_state.settings_panel = nullptr;
  g_state.status_caption = nullptr;
  g_state.apply_button = nullptr;
  g_state.save_button = nullptr;
  g_state.close_button = nullptr;
  g_state.field_widgets.fill(nullptr);
}

int SettingsWidgetCallback(XPWidgetMessage in_message,
                           XPWidgetID in_widget,
                           intptr_t in_param1,
                           intptr_t in_param2) {
  (void)in_widget;
  (void)in_param2;

  if (in_message == xpMessage_CloseButtonPushed) {
    ShowSettingsWindow(false);
    return 1;
  }

  if (in_message == xpMsg_PushButtonPressed) {
    XPWidgetID pressed = reinterpret_cast<XPWidgetID>(in_param1);
    if (pressed == g_state.apply_button) {
      ReadFieldsFromWidgets();
      if (ApplyFieldsToConfig()) {
        SyncFieldsFromConfig();
      }
      return 1;
    }
    if (pressed == g_state.save_button) {
      ReadFieldsFromWidgets();
      if (ApplyFieldsToConfig()) {
        SyncFieldsFromConfig();
        SaveConfig();
      }
      return 1;
    }
    if (pressed == g_state.close_button) {
      ShowSettingsWindow(false);
      return 1;
    }
  }

  if (in_message == xpMsg_ButtonStateChanged) {
    XPWidgetID changed = reinterpret_cast<XPWidgetID>(in_param1);
    if (changed == g_state.field_widgets[static_cast<size_t>(FieldId::DebugLogging)] ||
        changed == g_state.field_widgets[static_cast<size_t>(FieldId::LogMessages)]) {
      ReadFieldsFromWidgets();
      return 1;
    }
  }

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

  CreateSettingsWindow();

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
  DestroySettingsWindow();

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
  UpdateStatusCaption();

  LogMessage("Plugin enabled - Broadcasting GDL90 data");
  return 1;
}

PLUGIN_API void XPluginDisable(void) {
  LogMessage("Disabling plugin...");

  XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);

  g_state.enabled = false;
  XPLMCheckMenuItem(g_state.menu_id, g_state.menu_item_enable,
                    xplm_Menu_Unchecked);
  UpdateStatusCaption();

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
      const bool visible = g_state.settings_widget &&
                           XPIsWidgetVisible(g_state.settings_widget);
      ShowSettingsWindow(!visible);
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
