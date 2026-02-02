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

#include <array>
#include <cfloat>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

#include "backends/imgui_impl_opengl2.h"
#include "imgui.h"

#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/udp_broadcaster.h"

PLUGIN_API int XPluginEnable(void);
PLUGIN_API void XPluginDisable(void);

namespace {

constexpr double kMetersToFeet = 3.28084;
constexpr float kMetersPerSecondToKnots = 1.94384f;

struct Settings {
  std::string target_ip = "192.168.1.100";
  uint16_t target_port = 4000;

  uint32_t icao_address = 0xABCDEF;
  std::string callsign = "N12345";
  uint8_t emitter_category = 1;

  float heartbeat_rate = 1.0f;
  float position_rate = 2.0f;

  uint8_t nic = 11;
  uint8_t nacp = 11;

  bool debug_logging = false;
  bool log_messages = false;
};

struct PluginState {
  std::unique_ptr<gdl90::GDL90Encoder> encoder;
  std::unique_ptr<udp::UDPBroadcaster> broadcaster;
  Settings settings;

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
  std::string settings_path;

  XPLMWindowID settings_window = nullptr;
  bool imgui_initialized = false;
  double imgui_last_time = 0.0;
  bool imgui_mouse_down[3] = {false, false, false};
  bool imgui_mouse_changed[3] = {false, false, false};
  float imgui_mouse_wheel = 0.0f;
  float imgui_mouse_wheel_h = 0.0f;

  bool settings_dirty = false;
  std::string settings_last_error;

  char settings_target_ip[64] = {};
  int settings_target_port = 0;
  char settings_icao_address[16] = {};
  char settings_callsign[16] = {};
  int settings_emitter_category = 0;
  float settings_heartbeat_rate = 0.0f;
  float settings_position_rate = 0.0f;
  int settings_nic = 0;
  int settings_nacp = 0;
  bool settings_debug_logging = false;
  bool settings_log_messages = false;

  uint64_t heartbeat_packets_sent = 0;
  uint64_t position_packets_sent = 0;
  uint64_t bytes_sent = 0;
  int last_heartbeat_send_bytes = 0;
  int last_position_send_bytes = 0;
  std::string last_send_error;
};

PluginState g_state;

float FlightLoopCallback(float in_elapsed_since_last_call,
                         float in_elapsed_time_since_last_flight_loop,
                         int in_counter,
                         void* in_refcon);
void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref);

bool SaveSettingsToDisk(std::string* out_error);
void ShowSettingsWindow(bool show);
void CreateSettingsWindow();
void DestroySettingsWindow();
bool ReloadSettingsFromDisk();
void SyncSettingsUiFromConfig();


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

bool VerifyDataRef(XPLMDataRef ref, const char* name) {
  if (ref) {
    return true;
  }
  LogMessage(std::string("ERROR: Missing dataref ") + name);
  return false;
}

bool ApplyConfigToRuntime(const Settings& new_cfg,
                          std::string* out_error) {
  if (!g_state.broadcaster) {
    if (out_error) {
      *out_error = "Plugin not initialized";
    }
    return false;
  }

  const Settings old_cfg = g_state.settings;
  const bool target_changed = (old_cfg.target_ip != new_cfg.target_ip) ||
                              (old_cfg.target_port != new_cfg.target_port);

  if (target_changed) {
    auto new_broadcaster =
        std::make_unique<udp::UDPBroadcaster>(new_cfg.target_ip,
                                              new_cfg.target_port);
    if (!new_broadcaster->initialize()) {
      if (out_error) {
        *out_error = "Failed to initialize UDP broadcaster: " +
                     new_broadcaster->getLastError();
      }
      return false;
    }
    g_state.broadcaster = std::move(new_broadcaster);
    LogMessage("UDP broadcaster updated: " + new_cfg.target_ip + ":" +
               std::to_string(new_cfg.target_port));
  }

  g_state.settings = new_cfg;
  return true;
}

gdl90::PositionData GetOwnshipData(const Settings& cfg) {
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

bool SaveSettingsToDisk(std::string* out_error) {
  std::ofstream file(g_state.settings_path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    if (out_error) {
      *out_error = "Failed to open settings file for writing: " + g_state.settings_path;
    }
    return false;
  }

  auto json_escape = [](const std::string& input) -> std::string {
    std::string out;
    out.reserve(input.size());
    for (char ch : input) {
      switch (ch) {
        case '\\':
          out += "\\\\";
          break;
        case '"':
          out += "\\\"";
          break;
        case '\n':
          out += "\\n";
          break;
        case '\r':
          out += "\\r";
          break;
        case '\t':
          out += "\\t";
          break;
        default:
          out += ch;
          break;
      }
    }
    return out;
  };

  // Minimal JSON (no external deps).
  const Settings& s = g_state.settings;
  file << "{\n";
  file << "  \"target_ip\": \"" << json_escape(s.target_ip) << "\",\n";
  file << "  \"target_port\": " << static_cast<unsigned int>(s.target_port) << ",\n";
  file << "  \"icao_address\": " << static_cast<unsigned int>(s.icao_address & 0xFFFFFFu) << ",\n";
  file << "  \"callsign\": \"" << json_escape(s.callsign) << "\",\n";
  file << "  \"emitter_category\": " << static_cast<unsigned int>(s.emitter_category) << ",\n";
  file << "  \"heartbeat_rate\": " << s.heartbeat_rate << ",\n";
  file << "  \"position_rate\": " << s.position_rate << ",\n";
  file << "  \"nic\": " << static_cast<unsigned int>(s.nic) << ",\n";
  file << "  \"nacp\": " << static_cast<unsigned int>(s.nacp) << ",\n";
  file << "  \"debug_logging\": " << (s.debug_logging ? "true" : "false") << ",\n";
  file << "  \"log_messages\": " << (s.log_messages ? "true" : "false") << "\n";
  file << "}\n";
  file.close();

  if (out_error) {
    out_error->clear();
  }
  return true;
}

std::string ExtractJsonString(const std::string& text, const char* key) {
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = text.find(needle);
  if (pos == std::string::npos) return "";
  const size_t colon = text.find(':', pos + needle.size());
  if (colon == std::string::npos) return "";
  const size_t first_quote = text.find('"', colon + 1);
  if (first_quote == std::string::npos) return "";
  const size_t second_quote = text.find('"', first_quote + 1);
  if (second_quote == std::string::npos) return "";
  return text.substr(first_quote + 1, second_quote - first_quote - 1);
}

bool ExtractJsonNumber(const std::string& text, const char* key, double* out_value) {
  if (!out_value) return false;
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = text.find(needle);
  if (pos == std::string::npos) return false;
  const size_t colon = text.find(':', pos + needle.size());
  if (colon == std::string::npos) return false;
  size_t start = text.find_first_not_of(" \t\r\n", colon + 1);
  if (start == std::string::npos) return false;
  size_t end = start;
  while (end < text.size()) {
    const char c = text[end];
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '+'
          || c == 'e' || c == 'E')) {
      break;
    }
    ++end;
  }
  if (end == start) return false;
  try {
    *out_value = std::stod(text.substr(start, end - start));
    return true;
  } catch (...) {
    return false;
  }
}

bool ExtractJsonBool(const std::string& text, const char* key, bool* out_value) {
  if (!out_value) return false;
  const std::string needle = std::string("\"") + key + "\"";
  const size_t pos = text.find(needle);
  if (pos == std::string::npos) return false;
  const size_t colon = text.find(':', pos + needle.size());
  if (colon == std::string::npos) return false;
  const size_t start = text.find_first_not_of(" \t\r\n", colon + 1);
  if (start == std::string::npos) return false;
  if (text.compare(start, 4, "true") == 0) {
    *out_value = true;
    return true;
  }
  if (text.compare(start, 5, "false") == 0) {
    *out_value = false;
    return true;
  }
  return false;
}

bool LoadSettingsFromDisk(Settings* out_settings, std::string* out_error) {
  if (!out_settings) return false;
  std::ifstream file(g_state.settings_path, std::ios::binary);
  if (!file.is_open()) {
    // Not an error: file may not exist yet.
    if (out_error) out_error->clear();
    return false;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  const std::string text = buffer.str();

  Settings s = *out_settings;

  const std::string ip = ExtractJsonString(text, "target_ip");
  if (!ip.empty()) s.target_ip = ip;

  const std::string callsign = ExtractJsonString(text, "callsign");
  if (!callsign.empty()) s.callsign = callsign.substr(0, 8);

  double number = 0.0;
  if (ExtractJsonNumber(text, "target_port", &number)) {
    if (number >= 1.0 && number <= 65535.0) {
      s.target_port = static_cast<uint16_t>(static_cast<unsigned int>(number));
    }
  }
  if (ExtractJsonNumber(text, "icao_address", &number)) {
    s.icao_address = static_cast<uint32_t>(static_cast<unsigned int>(number)) & 0xFFFFFFu;
  }
  if (ExtractJsonNumber(text, "emitter_category", &number)) {
    s.emitter_category = static_cast<uint8_t>(static_cast<unsigned int>(number) & 0xFFu);
  }
  if (ExtractJsonNumber(text, "heartbeat_rate", &number)) {
    if (number > 0.0) s.heartbeat_rate = static_cast<float>(number);
  }
  if (ExtractJsonNumber(text, "position_rate", &number)) {
    if (number > 0.0) s.position_rate = static_cast<float>(number);
  }
  if (ExtractJsonNumber(text, "nic", &number)) {
    s.nic = static_cast<uint8_t>(static_cast<unsigned int>(number) & 0xFFu);
  }
  if (ExtractJsonNumber(text, "nacp", &number)) {
    s.nacp = static_cast<uint8_t>(static_cast<unsigned int>(number) & 0xFFu);
  }

  bool flag = false;
  if (ExtractJsonBool(text, "debug_logging", &flag)) s.debug_logging = flag;
  if (ExtractJsonBool(text, "log_messages", &flag)) s.log_messages = flag;

  *out_settings = s;
  if (out_error) out_error->clear();
  return true;
}

bool ReloadSettingsFromDisk() {
  Settings loaded = g_state.settings;
  std::string error;
  if (!LoadSettingsFromDisk(&loaded, &error)) {
    if (!error.empty()) {
      LogMessage("ERROR: Failed to reload settings: " + error);
      return false;
    }
    LogMessage("No settings file found; keeping current settings");
    return true;
  }

  std::string apply_error;
  if (!ApplyConfigToRuntime(loaded, &apply_error)) {
    LogMessage("ERROR: Failed to apply settings: " + apply_error);
    return false;
  }

  if (g_state.settings_window &&
      XPLMGetWindowIsVisible(g_state.settings_window) &&
      !g_state.settings_dirty) {
    SyncSettingsUiFromConfig();
  }

  LogMessage("Settings reloaded from disk");
  return true;
}

void SyncSettingsUiFromConfig() {
  const Settings& cfg = g_state.settings;

  std::snprintf(g_state.settings_target_ip, sizeof(g_state.settings_target_ip),
                "%s", cfg.target_ip.c_str());
  g_state.settings_target_port = static_cast<int>(cfg.target_port);
  std::snprintf(g_state.settings_icao_address,
                sizeof(g_state.settings_icao_address), "0x%06X",
                static_cast<unsigned int>(cfg.icao_address & 0xFFFFFFu));
  std::snprintf(g_state.settings_callsign, sizeof(g_state.settings_callsign),
                "%s", cfg.callsign.c_str());

  g_state.settings_emitter_category = static_cast<int>(cfg.emitter_category);
  g_state.settings_heartbeat_rate = cfg.heartbeat_rate;
  g_state.settings_position_rate = cfg.position_rate;
  g_state.settings_nic = static_cast<int>(cfg.nic);
  g_state.settings_nacp = static_cast<int>(cfg.nacp);
  g_state.settings_debug_logging = cfg.debug_logging;
  g_state.settings_log_messages = cfg.log_messages;

  g_state.settings_dirty = false;
  g_state.settings_last_error.clear();
}

bool ParseHex24(const char* text, uint32_t* out_value) {
  if (!text || !out_value) {
    return false;
  }
  std::string s = Trim(text);
  if (s.empty()) {
    return false;
  }
  try {
    unsigned long value = 0;
    if (s.rfind("0x", 0) == 0 || s.rfind("0X", 0) == 0) {
      value = std::stoul(s, nullptr, 16);
    } else {
      value = std::stoul(s, nullptr, 16);
    }
    *out_value = static_cast<uint32_t>(value) & 0xFFFFFFu;
    return true;
  } catch (...) {
    return false;
  }
}

bool BuildConfigFromSettingsUi(Settings* out_cfg,
                               std::string* out_error) {
  if (!out_cfg) {
    return false;
  }
  Settings cfg = g_state.settings;

  const std::string ip = Trim(g_state.settings_target_ip);
  if (ip.empty()) {
    if (out_error) {
      *out_error = "Target IP cannot be empty";
    }
    return false;
  }
  cfg.target_ip = ip;

  if (g_state.settings_target_port <= 0 || g_state.settings_target_port > 65535) {
    if (out_error) {
      *out_error = "Target port must be 1-65535";
    }
    return false;
  }
  cfg.target_port = static_cast<uint16_t>(g_state.settings_target_port);

  uint32_t icao = 0;
  if (!ParseHex24(g_state.settings_icao_address, &icao)) {
    if (out_error) {
      *out_error = "ICAO address must be a hex value (e.g. 0xABCDEF)";
    }
    return false;
  }
  cfg.icao_address = icao;

  cfg.callsign = Trim(g_state.settings_callsign).substr(0, 8);

  if (g_state.settings_emitter_category < 0 ||
      g_state.settings_emitter_category > 255) {
    if (out_error) {
      *out_error = "Emitter category must be 0-255";
    }
    return false;
  }
  cfg.emitter_category = static_cast<uint8_t>(g_state.settings_emitter_category);

  if (g_state.settings_heartbeat_rate <= 0.0f) {
    if (out_error) {
      *out_error = "Heartbeat rate must be > 0";
    }
    return false;
  }
  cfg.heartbeat_rate = g_state.settings_heartbeat_rate;

  if (g_state.settings_position_rate <= 0.0f) {
    if (out_error) {
      *out_error = "Position rate must be > 0";
    }
    return false;
  }
  cfg.position_rate = g_state.settings_position_rate;

  if (g_state.settings_nic < 0 || g_state.settings_nic > 255) {
    if (out_error) {
      *out_error = "NIC must be 0-255";
    }
    return false;
  }
  cfg.nic = static_cast<uint8_t>(g_state.settings_nic);

  if (g_state.settings_nacp < 0 || g_state.settings_nacp > 255) {
    if (out_error) {
      *out_error = "NACp must be 0-255";
    }
    return false;
  }
  cfg.nacp = static_cast<uint8_t>(g_state.settings_nacp);

  cfg.debug_logging = g_state.settings_debug_logging;
  cfg.log_messages = g_state.settings_log_messages;

  *out_cfg = cfg;
  return true;
}

ImGuiKey XplmVkeyToImGuiKey(unsigned char vkey) {
  if (vkey >= XPLM_VK_A && vkey <= XPLM_VK_Z) {
    return static_cast<ImGuiKey>(ImGuiKey_A + (vkey - XPLM_VK_A));
  }
  if (vkey >= XPLM_VK_0 && vkey <= XPLM_VK_9) {
    return static_cast<ImGuiKey>(ImGuiKey_0 + (vkey - XPLM_VK_0));
  }
  if (vkey >= XPLM_VK_F1 && vkey <= XPLM_VK_F12) {
    return static_cast<ImGuiKey>(ImGuiKey_F1 + (vkey - XPLM_VK_F1));
  }
  if (vkey >= XPLM_VK_NUMPAD0 && vkey <= XPLM_VK_NUMPAD9) {
    return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (vkey - XPLM_VK_NUMPAD0));
  }

  switch (vkey) {
    case XPLM_VK_TAB:
      return ImGuiKey_Tab;
    case XPLM_VK_LEFT:
      return ImGuiKey_LeftArrow;
    case XPLM_VK_RIGHT:
      return ImGuiKey_RightArrow;
    case XPLM_VK_UP:
      return ImGuiKey_UpArrow;
    case XPLM_VK_DOWN:
      return ImGuiKey_DownArrow;
    case XPLM_VK_PRIOR:
      return ImGuiKey_PageUp;
    case XPLM_VK_NEXT:
      return ImGuiKey_PageDown;
    case XPLM_VK_HOME:
      return ImGuiKey_Home;
    case XPLM_VK_END:
      return ImGuiKey_End;
    case XPLM_VK_INSERT:
      return ImGuiKey_Insert;
    case XPLM_VK_DELETE:
      return ImGuiKey_Delete;
    case XPLM_VK_BACK:
      return ImGuiKey_Backspace;
    case XPLM_VK_SPACE:
      return ImGuiKey_Space;
    case XPLM_VK_RETURN:
      return ImGuiKey_Enter;
    case XPLM_VK_ESCAPE:
      return ImGuiKey_Escape;
    default:
      return ImGuiKey_None;
  }
}

void EnsureImGuiInitialized() {
  if (g_state.imgui_initialized) {
    return;
  }
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::StyleColorsDark();
  ImGui_ImplOpenGL2_Init();
  g_state.imgui_last_time = static_cast<double>(XPLMGetElapsedTime());
  g_state.imgui_initialized = true;
}

void ShutdownImGui() {
  if (!g_state.imgui_initialized) {
    return;
  }
  ImGui_ImplOpenGL2_Shutdown();
  ImGui::DestroyContext();
  g_state.imgui_initialized = false;
}

void DrawSettingsWindowUI() {
  const Settings& cfg = g_state.settings;

  bool enabled = g_state.enabled;
  if (ImGui::Checkbox("Broadcasting Enabled", &enabled)) {
    if (enabled && !g_state.enabled) {
      XPluginEnable();
    } else if (!enabled && g_state.enabled) {
      XPluginDisable();
    }
  }

  ImGui::SameLine();
  ImGui::Text("Target: %s:%u", cfg.target_ip.c_str(),
              static_cast<unsigned int>(cfg.target_port));

  const float sim_time =
      g_state.sim_time_ref ? XPLMGetDataf(g_state.sim_time_ref) : 0.0f;
  const float since_heartbeat =
      (g_state.last_heartbeat > 0.0f) ? (sim_time - g_state.last_heartbeat) : 0.0f;
  const float since_position =
      (g_state.last_position > 0.0f) ? (sim_time - g_state.last_position) : 0.0f;

  const std::string tail = ReadTailNumber();
  const std::string effective_callsign = !tail.empty() ? tail : cfg.callsign;

  ImGui::Separator();

  bool dirty_now = g_state.settings_dirty;

  if (ImGui::BeginTabBar("XP2GDL90Tabs")) {
    if (ImGui::BeginTabItem("Status")) {
      ImGui::Text("Effective callsign: %s", effective_callsign.c_str());
      ImGui::Text("Heartbeat rate: %.2f Hz | Position rate: %.2f Hz",
                  cfg.heartbeat_rate, cfg.position_rate);
      ImGui::Text("Last heartbeat: %.2fs ago | Last position: %.2fs ago",
                  since_heartbeat, since_position);
      ImGui::Text("Heartbeat packets: %llu (%d bytes last)",
                  static_cast<unsigned long long>(g_state.heartbeat_packets_sent),
                  g_state.last_heartbeat_send_bytes);
      ImGui::Text("Position packets: %llu (%d bytes last)",
                  static_cast<unsigned long long>(g_state.position_packets_sent),
                  g_state.last_position_send_bytes);
      ImGui::Text("Bytes sent: %llu",
                  static_cast<unsigned long long>(g_state.bytes_sent));

      if (!g_state.last_send_error.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last send error:");
        ImGui::TextWrapped("%s", g_state.last_send_error.c_str());
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Network")) {
      dirty_now |= ImGui::InputText("Target IP", g_state.settings_target_ip,
                                    sizeof(g_state.settings_target_ip));
      dirty_now |= ImGui::InputInt("Target Port", &g_state.settings_target_port);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Ownship")) {
      dirty_now |= ImGui::InputText("ICAO Address (hex)",
                                    g_state.settings_icao_address,
                                    sizeof(g_state.settings_icao_address));
      dirty_now |= ImGui::InputText("Callsign (fallback)",
                                    g_state.settings_callsign,
                                    sizeof(g_state.settings_callsign));
      dirty_now |= ImGui::InputInt("Emitter Category",
                                   &g_state.settings_emitter_category);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rates")) {
      dirty_now |= ImGui::InputFloat("Heartbeat Rate (Hz)",
                                     &g_state.settings_heartbeat_rate, 0.1f,
                                     1.0f, "%.2f");
      dirty_now |= ImGui::InputFloat("Position Rate (Hz)",
                                     &g_state.settings_position_rate, 0.1f,
                                     1.0f, "%.2f");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Accuracy")) {
      dirty_now |= ImGui::InputInt("NIC", &g_state.settings_nic);
      dirty_now |= ImGui::InputInt("NACp", &g_state.settings_nacp);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Debug")) {
      dirty_now |= ImGui::Checkbox("Debug logging", &g_state.settings_debug_logging);
      dirty_now |= ImGui::Checkbox("Log raw messages", &g_state.settings_log_messages);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  g_state.settings_dirty = dirty_now;

  ImGui::Separator();
  if (g_state.settings_dirty) {
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       "Unsaved changes (Apply and/or Save).");
  } else {
    ImGui::TextUnformatted("No pending changes.");
  }

  if (!g_state.settings_last_error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                       g_state.settings_last_error.c_str());
  }

  ImGui::BeginDisabled(!g_state.settings_dirty);
  if (ImGui::Button("Apply")) {
    Settings new_cfg;
    std::string error;
    if (!BuildConfigFromSettingsUi(&new_cfg, &error)) {
      g_state.settings_last_error = error;
    } else if (!ApplyConfigToRuntime(new_cfg, &error)) {
      g_state.settings_last_error = error;
    } else {
      g_state.settings_last_error.clear();
      SyncSettingsUiFromConfig();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Save")) {
    Settings new_cfg;
    std::string error;
    if (!BuildConfigFromSettingsUi(&new_cfg, &error)) {
      g_state.settings_last_error = error;
    } else if (!ApplyConfigToRuntime(new_cfg, &error)) {
      g_state.settings_last_error = error;
    } else if (!SaveSettingsToDisk(&error)) {
      g_state.settings_last_error = "Failed to save settings: " + error;
    } else {
      g_state.settings_last_error.clear();
      SyncSettingsUiFromConfig();
    }
  }
  ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button("Revert")) {
    SyncSettingsUiFromConfig();
  }
  ImGui::SameLine();
  if (ImGui::Button("Defaults")) {
    const Settings defaults;
    std::snprintf(g_state.settings_target_ip, sizeof(g_state.settings_target_ip),
                  "%s", defaults.target_ip.c_str());
    g_state.settings_target_port = static_cast<int>(defaults.target_port);
    std::snprintf(g_state.settings_icao_address,
                  sizeof(g_state.settings_icao_address), "0x%06X",
                  static_cast<unsigned int>(defaults.icao_address & 0xFFFFFFu));
    std::snprintf(g_state.settings_callsign, sizeof(g_state.settings_callsign),
                  "%s", defaults.callsign.c_str());
    g_state.settings_emitter_category =
        static_cast<int>(defaults.emitter_category);
    g_state.settings_heartbeat_rate = defaults.heartbeat_rate;
    g_state.settings_position_rate = defaults.position_rate;
    g_state.settings_nic = static_cast<int>(defaults.nic);
    g_state.settings_nacp = static_cast<int>(defaults.nacp);
    g_state.settings_debug_logging = defaults.debug_logging;
    g_state.settings_log_messages = defaults.log_messages;
    g_state.settings_dirty = true;
    g_state.settings_last_error.clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reload from Disk")) {
    if (!ReloadSettingsFromDisk()) {
      g_state.settings_last_error = "Failed to reload settings from disk";
    } else {
      SyncSettingsUiFromConfig();
        }
      }
}

void SettingsDrawCallback(XPLMWindowID in_window_id, void* in_refcon) {
  (void)in_refcon;

  EnsureImGuiInitialized();

  int screen_left = 0;
  int screen_top = 0;
  int screen_right = 0;
  int screen_bottom = 0;
  XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right,
                            &screen_bottom);
  const float screen_width = static_cast<float>(screen_right - screen_left);
  const float screen_height = static_cast<float>(screen_top - screen_bottom);

  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  XPLMGetWindowGeometry(in_window_id, &left, &top, &right, &bottom);
  const float width = static_cast<float>(right - left);
  const float height = static_cast<float>(top - bottom);

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(screen_width, screen_height);

  const double now = static_cast<double>(XPLMGetElapsedTime());
  const double dt = now - g_state.imgui_last_time;
  io.DeltaTime = (dt > 0.0) ? static_cast<float>(dt) : (1.0f / 60.0f);
  g_state.imgui_last_time = now;

  int mouse_x = 0;
  int mouse_y = 0;
  XPLMGetMouseLocationGlobal(&mouse_x, &mouse_y);
  if (mouse_x >= left && mouse_x <= right && mouse_y >= bottom &&
      mouse_y <= top) {
    const float imgui_x = static_cast<float>(mouse_x - screen_left);
    const float imgui_y = static_cast<float>(screen_top - mouse_y);
    io.AddMousePosEvent(imgui_x, imgui_y);
  } else {
    io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
  }

  for (int i = 0; i < 3; ++i) {
    if (g_state.imgui_mouse_changed[i]) {
      io.AddMouseButtonEvent(i, g_state.imgui_mouse_down[i]);
      g_state.imgui_mouse_changed[i] = false;
    }
  }

  if (g_state.imgui_mouse_wheel != 0.0f || g_state.imgui_mouse_wheel_h != 0.0f) {
    io.AddMouseWheelEvent(g_state.imgui_mouse_wheel_h, g_state.imgui_mouse_wheel);
    g_state.imgui_mouse_wheel = 0.0f;
    g_state.imgui_mouse_wheel_h = 0.0f;
  }

  ImGui_ImplOpenGL2_NewFrame();
  ImGui::NewFrame();

  const float window_x = static_cast<float>(left - screen_left);
  const float window_y = static_cast<float>(screen_top - top);
  ImGui::SetNextWindowPos(ImVec2(window_x, window_y));
  ImGui::SetNextWindowSize(ImVec2(width, height));
  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoBringToFrontOnFocus;
  ImGui::Begin("XP2GDL90 Settings Host", nullptr, flags);
  DrawSettingsWindowUI();
  ImGui::End();

  ImGui::Render();
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
}

int SettingsMouseClickCallback(XPLMWindowID in_window_id,
                               int x,
                               int y,
                               XPLMMouseStatus in_mouse,
                               void* in_refcon) {
  (void)in_refcon;

  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  XPLMGetWindowGeometry(in_window_id, &left, &top, &right, &bottom);

  const bool inside =
      (x >= left && x <= right && y >= bottom && y <= top);
  if (!inside) {
    return 0;
  }

  XPLMTakeKeyboardFocus(in_window_id);

  if (in_mouse == xplm_MouseDown) {
    g_state.imgui_mouse_down[0] = true;
    g_state.imgui_mouse_changed[0] = true;
  } else if (in_mouse == xplm_MouseUp) {
    g_state.imgui_mouse_down[0] = false;
    g_state.imgui_mouse_changed[0] = true;
  }
  return 1;
}

int SettingsRightClickCallback(XPLMWindowID in_window_id,
                               int x,
                               int y,
                               XPLMMouseStatus in_mouse,
                               void* in_refcon) {
  (void)in_refcon;

  int left = 0;
  int top = 0;
  int right = 0;
  int bottom = 0;
  XPLMGetWindowGeometry(in_window_id, &left, &top, &right, &bottom);

  const bool inside =
      (x >= left && x <= right && y >= bottom && y <= top);
  if (!inside) {
    return 0;
  }

  XPLMTakeKeyboardFocus(in_window_id);

  if (in_mouse == xplm_MouseDown) {
    g_state.imgui_mouse_down[1] = true;
    g_state.imgui_mouse_changed[1] = true;
  } else if (in_mouse == xplm_MouseUp) {
    g_state.imgui_mouse_down[1] = false;
    g_state.imgui_mouse_changed[1] = true;
  }
  return 1;
}

void SettingsKeyCallback(XPLMWindowID in_window_id,
                         char in_key,
                         XPLMKeyFlags in_flags,
                         char in_virtual_key,
                         void* in_refcon,
                         int losing_focus) {
  (void)in_window_id;
  (void)in_refcon;

  if (!g_state.imgui_initialized) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  if (losing_focus) {
    io.AddFocusEvent(false);
    return;
  }
  io.AddFocusEvent(true);

  io.AddKeyEvent(ImGuiMod_Shift, (in_flags & xplm_ShiftFlag) != 0);
  io.AddKeyEvent(ImGuiMod_Alt, (in_flags & xplm_OptionAltFlag) != 0);
  io.AddKeyEvent(ImGuiMod_Ctrl, (in_flags & xplm_ControlFlag) != 0);

  const unsigned char vkey = static_cast<unsigned char>(in_virtual_key);
  const ImGuiKey key = XplmVkeyToImGuiKey(vkey);
  const bool down = (in_flags & xplm_DownFlag) != 0;
  const bool up = (in_flags & xplm_UpFlag) != 0;

  if (key != ImGuiKey_None) {
    if (down) {
      io.AddKeyEvent(key, true);
    }
    if (up) {
      io.AddKeyEvent(key, false);
    }
  }

  if (down) {
    const unsigned char ch = static_cast<unsigned char>(in_key);
    if (ch >= 32 && ch != 127) {
      io.AddInputCharacter(static_cast<unsigned int>(ch));
    }
  }
}

XPLMCursorStatus SettingsCursorCallback(XPLMWindowID in_window_id,
                                       int x,
                                       int y,
                                       void* in_refcon) {
  (void)in_window_id;
  (void)x;
  (void)y;
  (void)in_refcon;
  return xplm_CursorArrow;
}

int SettingsMouseWheelCallback(XPLMWindowID in_window_id,
                               int x,
                               int y,
                               int wheel,
                               int clicks,
                               void* in_refcon) {
  (void)in_window_id;
  (void)x;
  (void)y;
  (void)in_refcon;

  if (wheel == 0) {
    g_state.imgui_mouse_wheel += static_cast<float>(clicks);
  } else {
    g_state.imgui_mouse_wheel_h += static_cast<float>(clicks);
  }
  return 1;
}

void CreateSettingsWindow() {
  if (g_state.settings_window) {
    return;
  }

  int screen_left = 0;
  int screen_top = 0;
  int screen_right = 0;
  int screen_bottom = 0;
  XPLMGetScreenBoundsGlobal(&screen_left, &screen_top, &screen_right,
                            &screen_bottom);

  const int window_width = 660;
  const int window_height = 540;
  const int window_left =
      screen_left + (screen_right - screen_left - window_width) / 2;
  const int window_top =
      screen_top - (screen_top - screen_bottom - window_height) / 2;

  XPLMCreateWindow_t params = {};
  params.structSize = sizeof(params);
  params.left = window_left;
  params.top = window_top;
  params.right = window_left + window_width;
  params.bottom = window_top - window_height;
  params.visible = 0;
  params.drawWindowFunc = SettingsDrawCallback;
  params.handleMouseClickFunc = SettingsMouseClickCallback;
  params.handleKeyFunc = SettingsKeyCallback;
  params.handleCursorFunc = SettingsCursorCallback;
  params.handleMouseWheelFunc = SettingsMouseWheelCallback;
  params.refcon = nullptr;
  params.decorateAsFloatingWindow = xplm_WindowDecorationRoundRectangle;
  params.layer = xplm_WindowLayerFloatingWindows;
  params.handleRightClickFunc = SettingsRightClickCallback;

  g_state.settings_window = XPLMCreateWindowEx(&params);
  if (!g_state.settings_window) {
    LogMessage("ERROR: Failed to create Settings window");
    return;
  }

  XPLMSetWindowPositioningMode(g_state.settings_window, xplm_WindowPositionFree,
                               -1);
  XPLMSetWindowResizingLimits(g_state.settings_window, 520, 420, 1200, 900);
  XPLMSetWindowTitle(g_state.settings_window, "XP2GDL90 Settings");
}

void DestroySettingsWindow() {
  if (!g_state.settings_window) {
    return;
  }
  XPLMDestroyWindow(g_state.settings_window);
  g_state.settings_window = nullptr;
  ShutdownImGui();
}

void ShowSettingsWindow(bool show) {
  if (!g_state.settings_window) {
    CreateSettingsWindow();
  }
  if (!g_state.settings_window) {
    return;
  }

  if (show) {
    SyncSettingsUiFromConfig();
    XPLMSetWindowIsVisible(g_state.settings_window, 1);
    XPLMBringWindowToFront(g_state.settings_window);
  } else {
    XPLMSetWindowIsVisible(g_state.settings_window, 0);
  }
}

}  // namespace

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
  std::strcpy(outName, "XP2GDL90");
  std::strcpy(outSig, "com.xp2gdl90.plugin");
  std::strcpy(outDesc, "GDL90 ADS-B data broadcaster for EFB applications");

  LogMessage("Plugin starting...");

  XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

  g_state.initialized = false;
  g_state.enabled = false;
  g_state.last_heartbeat = 0.0f;
  g_state.last_position = 0.0f;

  g_state.encoder = std::make_unique<gdl90::GDL90Encoder>();

  char prefs_path[512] = {};
  XPLMGetPrefsPath(prefs_path);
  XPLMExtractFileAndPath(prefs_path);  // leaves directory in prefs_path
  g_state.settings_path =
      std::string(prefs_path) + XPLMGetDirectorySeparator() + "xp2gdl90.json";

  Settings loaded = g_state.settings;
  std::string load_error;
  if (LoadSettingsFromDisk(&loaded, &load_error)) {
    g_state.settings = loaded;
    LogMessage("Settings loaded: " + g_state.settings_path);
  } else if (!load_error.empty()) {
    LogMessage("Warning: Failed to load settings: " + load_error);
  } else {
    LogMessage("No settings file found; using GUI defaults");
  }

  const Settings& cfg = g_state.settings;

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
  XPLMAppendMenuItem(g_state.menu_id, "Reload Settings",
                     reinterpret_cast<void*>(3), 0);

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
  const Settings& cfg = g_state.settings;

  if (cfg.heartbeat_rate > 0.0f &&
      sim_time - g_state.last_heartbeat >= (1.0f / cfg.heartbeat_rate)) {
    const auto heartbeat = g_state.encoder->createHeartbeat(true, true);
    const int sent = g_state.broadcaster->send(heartbeat);
    g_state.last_heartbeat_send_bytes = sent;
    if (sent >= 0) {
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.heartbeat_packets_sent++;
      g_state.last_send_error.clear();
    } else {
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
    g_state.last_heartbeat = sim_time;
  }

  if (cfg.position_rate > 0.0f &&
      sim_time - g_state.last_position >= (1.0f / cfg.position_rate)) {
    const gdl90::PositionData ownship = GetOwnshipData(cfg);
    const auto position_msg = g_state.encoder->createOwnshipReport(ownship);
    const int sent = g_state.broadcaster->send(position_msg);
    g_state.last_position_send_bytes = sent;
    if (sent >= 0) {
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.position_packets_sent++;
      g_state.last_send_error.clear();
    } else {
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
    g_state.last_position = sim_time;
  }

  return -1.0f;
}

void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref) {
  (void)in_menu_ref;

  const intptr_t item = reinterpret_cast<intptr_t>(in_item_ref);
  if (item != 1) {
    if (item == 2) {
      const int visible =
          g_state.settings_window ? XPLMGetWindowIsVisible(g_state.settings_window) : 0;
      ShowSettingsWindow(visible == 0);
    } else if (item == 3) {
      ReloadSettingsFromDisk();
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
