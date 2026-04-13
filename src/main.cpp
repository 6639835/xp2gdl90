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
#include <XPLMScenery.h>
#include <XPLMUtilities.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "backends/imgui_impl_opengl2.h"
#include "imgui.h"

#include "xp2gdl90/foreflight_encoder.h"
#include "xp2gdl90/foreflight_protocol.h"
#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/protocol_utils.h"
#include "xp2gdl90/settings.h"
#include "xp2gdl90/settings_ui.h"
#include "xp2gdl90/udp_receiver.h"
#include "xp2gdl90/udp_broadcaster.h"

#ifdef _WIN32
// X-Plane SDK headers may include Windows headers that define min/max macros.
// Undefine them so std::min/std::max and std::numeric_limits<>::min/max work.
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

PLUGIN_API int XPluginEnable(void);
PLUGIN_API void XPluginDisable(void);

namespace {

constexpr double kMetersToFeet = 3.28084;
constexpr float kMetersPerSecondToKnots = 1.94384f;
constexpr float kMetersPerSecondToFeetPerMinute = 196.8504f;
constexpr float kTrafficReportRate = 1.0f;
constexpr float kForeFlightDeviceInfoRate = 1.0f;
constexpr float kForeFlightAhrsRate = 5.0f;
constexpr float kOwnshipGeoAltitudeRate = 1.0f;
constexpr float kForeFlightDiscoveryTimeout = 15.0f;
constexpr int kTrafficFlightIdSize = 8;
constexpr int kTrafficTailnumSize = 10;
constexpr float kMinTrackSpeedMps = 0.5f;
constexpr double kRadiansToDegrees = 57.29577951308232;

using xp2gdl90::Settings;
using xp2gdl90::SettingsUiState;

enum class TcasSsrMode : int {
  Off = 0,
  Standby = 1,
  ModeA = 2,
  ModeC = 3,
  Test = 4,
  Ground = 5,
  TaOnly = 6,
  TaRa = 7,
};

struct TrafficTcasRefs {
  XPLMDataRef mode_s_ref = nullptr;
  XPLMDataRef mode_c_code_ref = nullptr;
  XPLMDataRef ssr_mode_ref = nullptr;
  XPLMDataRef flight_id_ref = nullptr;
  XPLMDataRef x_ref = nullptr;
  XPLMDataRef y_ref = nullptr;
  XPLMDataRef z_ref = nullptr;
  XPLMDataRef vx_ref = nullptr;
  XPLMDataRef vy_ref = nullptr;
  XPLMDataRef vz_ref = nullptr;
  XPLMDataRef vertical_speed_ref = nullptr;
  XPLMDataRef heading_ref = nullptr;
  XPLMDataRef weight_on_wheels_ref = nullptr;
  XPLMDataRef wake_cat_ref = nullptr;
  size_t slot_count = 0;

  bool IsUsable() const {
    return mode_s_ref && x_ref && y_ref && z_ref && slot_count > 0;
  }
};

struct LegacyTrafficRefs {
  XPLMDataRef x_ref = nullptr;
  XPLMDataRef y_ref = nullptr;
  XPLMDataRef z_ref = nullptr;
  XPLMDataRef vx_ref = nullptr;
  XPLMDataRef vy_ref = nullptr;
  XPLMDataRef vz_ref = nullptr;
  XPLMDataRef heading_ref = nullptr;

  bool IsUsable() const {
    return x_ref && y_ref && z_ref && vx_ref && vy_ref && vz_ref &&
           heading_ref;
  }
};

struct TrafficTextRefs {
  XPLMDataRef tailnum_ref = nullptr;
};

struct PluginState {
  std::unique_ptr<gdl90::GDL90Encoder> encoder;
  std::unique_ptr<gdl90::foreflight::ForeFlightEncoder> foreflight_encoder;
  std::unique_ptr<udp::UDPBroadcaster> broadcaster;
  std::unique_ptr<udp::UDPReceiver> foreflight_receiver;
  Settings settings;

  XPLMDataRef lat_ref = nullptr;
  XPLMDataRef lon_ref = nullptr;
  XPLMDataRef alt_ref = nullptr;
  XPLMDataRef pressure_alt_ref = nullptr;
  XPLMDataRef speed_ref = nullptr;
  XPLMDataRef track_ref = nullptr;
  XPLMDataRef pitch_ref = nullptr;
  XPLMDataRef roll_ref = nullptr;
  XPLMDataRef heading_ref = nullptr;
  XPLMDataRef indicated_airspeed_ref = nullptr;
  XPLMDataRef true_airspeed_ref = nullptr;
  XPLMDataRef vs_ref = nullptr;
  XPLMDataRef airborne_ref = nullptr;
  XPLMDataRef sim_time_ref = nullptr;
  XPLMDataRef tailnum_ref = nullptr;
  TrafficTcasRefs traffic_tcas_refs;
  std::vector<LegacyTrafficRefs> legacy_traffic_refs;
  std::vector<TrafficTextRefs> traffic_text_refs;

  float last_heartbeat = 0.0f;
  float last_position = 0.0f;
  float last_traffic = 0.0f;
  float last_device_info = 0.0f;
  float last_ahrs = 0.0f;
  float last_geo_altitude = 0.0f;
  float last_foreflight_discovery = -1.0f;

  std::string discovered_target_ip;
  uint16_t discovered_target_port = 0;
  bool using_discovered_target = false;

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
  SettingsUiState settings_ui;

  uint64_t heartbeat_packets_sent = 0;
  uint64_t position_packets_sent = 0;
  uint64_t traffic_packets_sent = 0;
  uint64_t device_info_packets_sent = 0;
  uint64_t ahrs_packets_sent = 0;
  uint64_t geo_altitude_packets_sent = 0;
  uint64_t bytes_sent = 0;
  int last_heartbeat_send_bytes = 0;
  int last_position_send_bytes = 0;
  int last_traffic_send_bytes = 0;
  int last_device_info_send_bytes = 0;
  int last_ahrs_send_bytes = 0;
  int last_geo_altitude_send_bytes = 0;
  int last_traffic_target_count = 0;
  std::string last_send_error;
  std::string last_receiver_error;
};

PluginState g_state;

float FlightLoopCallback(float in_elapsed_since_last_call,
                         float in_elapsed_time_since_last_flight_loop,
                         int in_counter,
                         void* in_refcon);
void MenuHandlerCallback(void* in_menu_ref, void* in_item_ref);

bool SaveSettingsToDisk(std::string* out_error);
bool LoadSettingsFromDisk(Settings* out_settings, std::string* out_error);
void ShowSettingsWindow(bool show);
void CreateSettingsWindow();
void DestroySettingsWindow();
bool ReloadSettingsFromDisk();
void SyncSettingsUiFromConfig();
void InitializeTrafficDataRefs();
size_t CollectTrafficData(const Settings& cfg,
                          std::vector<gdl90::PositionData>* out_reports);
void SendTrafficReports(float sim_time, const Settings& cfg);
bool ReconfigureRuntimeReceivers(const Settings& cfg, std::string* out_error);
void RefreshBroadcastTarget(float sim_time, const Settings& cfg);
void PollForeFlightDiscovery(float sim_time, const Settings& cfg);


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

template <typename Int, typename Float>
Int ClampFloatToInt(Float value) {
  static_assert(std::numeric_limits<Int>::is_integer, "Int must be integral");

  if (!std::isfinite(static_cast<double>(value))) {
    return Int{0};
  }

  const double v = static_cast<double>(value);
  const double lo = static_cast<double>(std::numeric_limits<Int>::min());
  const double hi = static_cast<double>(std::numeric_limits<Int>::max());

  if (v <= lo) return std::numeric_limits<Int>::min();
  if (v >= hi) return std::numeric_limits<Int>::max();
  return static_cast<Int>(value);
}

int16_t ClampFpmToInt16OrInvalid(float fpm) {
  if (!std::isfinite(static_cast<double>(fpm))) {
    return std::numeric_limits<int16_t>::min();  // INT16_MIN sentinel => invalid.
  }

  // INT16_MIN is reserved as invalid sentinel in the encoder; avoid producing it.
  constexpr float kMin = static_cast<float>(std::numeric_limits<int16_t>::min() + 1);
  constexpr float kMax = static_cast<float>(std::numeric_limits<int16_t>::max());
  const float clamped = (std::max)(kMin, (std::min)(kMax, fpm));
  return static_cast<int16_t>(clamped);
}

uint16_t NormalizeDegreesToUint16(float degrees) {
  if (!std::isfinite(static_cast<double>(degrees))) {
    return 0;
  }
  double d = std::fmod(static_cast<double>(degrees), 360.0);
  if (d < 0.0) d += 360.0;
  return ClampFloatToInt<uint16_t>(d);
}

double NormalizeDegrees360(double degrees) {
  if (!std::isfinite(degrees)) {
    return std::numeric_limits<double>::quiet_NaN();
  }

  double normalized = std::fmod(degrees, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }
  return normalized;
}

uint32_t NormalizeTcasAddress(int raw_address) {
  return static_cast<uint32_t>(raw_address) & 0xFFFFFFu;
}

bool TcasAddressUsesRealIcao(int raw_address) {
  return (static_cast<uint32_t>(raw_address) & 0x80000000u) != 0u;
}

gdl90::AddressType ResolveTcasAddressType(int raw_address) {
  return TcasAddressUsesRealIcao(raw_address)
             ? gdl90::AddressType::ADSB_ICAO
             : gdl90::AddressType::ADSB_SELF_ASSIGNED;
}

bool TcasSsrModeHasAltitude(int ssr_mode) {
  switch (static_cast<TcasSsrMode>(ssr_mode)) {
    case TcasSsrMode::ModeC:
    case TcasSsrMode::Ground:
    case TcasSsrMode::TaOnly:
    case TcasSsrMode::TaRa:
      return true;
    default:
      return false;
  }
}

bool TcasSsrModeIndicatesGround(int ssr_mode) {
  return static_cast<TcasSsrMode>(ssr_mode) == TcasSsrMode::Ground;
}

uint8_t ResolveTrafficEmergencyCodeFromSquawk(int squawk) {
  switch (squawk) {
    case 7500:
      return 5;
    case 7600:
      return 4;
    case 7700:
      return 1;
    default:
      return 0;
  }
}

bool TryResolveTrackFromVelocity(float vx, float vz, uint16_t* out_track) {
  if (!out_track || !std::isfinite(static_cast<double>(vx)) ||
      !std::isfinite(static_cast<double>(vz))) {
    return false;
  }

  const double speed = std::hypot(static_cast<double>(vx), static_cast<double>(vz));
  if (speed < kMinTrackSpeedMps) {
    return false;
  }

  const double degrees =
      std::atan2(static_cast<double>(vx), -static_cast<double>(vz)) *
      kRadiansToDegrees;
  *out_track = ClampFloatToInt<uint16_t>(NormalizeDegrees360(degrees));
  return true;
}

void ResolveTrafficTrack(float vx,
                         float vz,
                         float heading,
                         uint16_t* out_track,
                         gdl90::TrackType* out_track_type) {
  if (!out_track || !out_track_type) {
    return;
  }

  uint16_t velocity_track = 0;
  if (TryResolveTrackFromVelocity(vx, vz, &velocity_track)) {
    *out_track = velocity_track;
    *out_track_type = gdl90::TrackType::TRUE_TRACK;
    return;
  }

  if (std::isfinite(static_cast<double>(heading))) {
    *out_track = NormalizeDegreesToUint16(heading);
    *out_track_type = gdl90::TrackType::TRUE_HEADING;
    return;
  }

  *out_track = 0;
  *out_track_type = gdl90::TrackType::INVALID;
}

uint16_t ClampKnotsToUint16OrInvalid(float knots) {
  if (!std::isfinite(static_cast<double>(knots)) || knots < 0.0f) {
    return gdl90::foreflight::AHRS_AIRSPEED_INVALID;
  }
  const float clamped =
      (std::min)(knots,
                 static_cast<float>(gdl90::foreflight::AHRS_AIRSPEED_INVALID - 1u));
  return static_cast<uint16_t>(clamped);
}

bool ParseForeFlightBroadcastPacket(const std::vector<uint8_t>& packet,
                                    uint16_t* out_port) {
  return xp2gdl90::foreflight::ParseDiscoveryBroadcast(packet, out_port);
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

std::string ReadDataRefText(XPLMDataRef ref, int max_bytes) {
  if (!ref || max_bytes <= 0) {
    return "";
  }

  std::string buffer(static_cast<size_t>(max_bytes), '\0');
  const int bytes_read = XPLMGetDatab(ref, buffer.data(), 0, max_bytes);
  if (bytes_read <= 0) {
    return "";
  }

  buffer.resize(static_cast<size_t>(bytes_read));
  const size_t nul = buffer.find('\0');
  if (nul != std::string::npos) {
    buffer.resize(nul);
  }
  return Trim(buffer);
}

float ReadFloatArrayValue(XPLMDataRef ref, int index, float fallback) {
  if (!ref || index < 0) {
    return fallback;
  }

  float value = fallback;
  const int values_read = XPLMGetDatavf(ref, &value, index, 1);
  return values_read == 1 ? value : fallback;
}

int ReadIntArrayValue(XPLMDataRef ref, int index, int fallback) {
  if (!ref || index < 0) {
    return fallback;
  }

  int value = fallback;
  const int values_read = XPLMGetDatavi(ref, &value, index, 1);
  return values_read == 1 ? value : fallback;
}

bool LocalPositionToWorld(double x,
                          double y,
                          double z,
                          double* out_latitude,
                          double* out_longitude,
                          int32_t* out_altitude_feet) {
  if (!out_latitude || !out_longitude || !out_altitude_feet ||
      !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
    return false;
  }

  double latitude = 0.0;
  double longitude = 0.0;
  double altitude_meters = 0.0;
  XPLMLocalToWorld(x, y, z, &latitude, &longitude, &altitude_meters);

  if (!std::isfinite(latitude) || !std::isfinite(longitude) ||
      !std::isfinite(altitude_meters)) {
    return false;
  }

  *out_latitude = latitude;
  *out_longitude = longitude;
  *out_altitude_feet =
      ClampFloatToInt<int32_t>(altitude_meters * kMetersToFeet);
  return true;
}

gdl90::EmitterCategory WakeCategoryToEmitterCategory(int wake_category) {
  switch (wake_category) {
    case 0:
      return gdl90::EmitterCategory::LIGHT;
    case 1:
      return gdl90::EmitterCategory::LARGE;
    case 2:
      return gdl90::EmitterCategory::HEAVY;
    case 3:
      return gdl90::EmitterCategory::HIGH_VORTEX_LARGE;
    default:
      return gdl90::EmitterCategory::NO_INFO;
  }
}

std::string FormatTrafficFallbackCallsign(uint32_t address) {
  char buffer[9] = {};
  std::snprintf(buffer, sizeof(buffer), "V%06X",
                static_cast<unsigned int>(address & 0xFFFFFFu));
  return buffer;
}

std::string ReadTrafficFlightId(size_t slot) {
  if (!g_state.traffic_tcas_refs.flight_id_ref) {
    return "";
  }

  char buffer[kTrafficFlightIdSize + 1] = {};
  const int offset =
      ClampFloatToInt<int>(slot * static_cast<size_t>(kTrafficFlightIdSize));
  const int bytes_read = XPLMGetDatab(g_state.traffic_tcas_refs.flight_id_ref,
                                      buffer, offset, kTrafficFlightIdSize);
  if (bytes_read <= 0) {
    return "";
  }
  buffer[(std::min)(bytes_read, kTrafficFlightIdSize)] = '\0';
  return Trim(std::string(buffer));
}

std::string ReadTrafficCallsign(size_t slot, uint32_t address) {
  std::string callsign =
      xp2gdl90::protocol::SanitizeCallsign(ReadTrafficFlightId(slot));
  if (callsign.empty() && slot >= 1 && slot <= g_state.traffic_text_refs.size()) {
    callsign = xp2gdl90::protocol::SanitizeCallsign(ReadDataRefText(
        g_state.traffic_text_refs[slot - 1].tailnum_ref, kTrafficTailnumSize));
  }
  if (!callsign.empty()) {
    return callsign;
  }
  return FormatTrafficFallbackCallsign(address);
}

uint16_t CalculateHorizontalSpeedKnots(float vx, float vz) {
  if (!std::isfinite(static_cast<double>(vx)) ||
      !std::isfinite(static_cast<double>(vz))) {
    return gdl90::VELOCITY_INVALID;
  }
  const float speed = std::sqrt(vx * vx + vz * vz);
  return ClampFloatToInt<uint16_t>(speed * kMetersPerSecondToKnots);
}

int16_t CalculateVerticalSpeedFpm(float vy_mps) {
  if (!std::isfinite(static_cast<double>(vy_mps))) {
    return std::numeric_limits<int16_t>::min();
  }
  return ClampFpmToInt16OrInvalid(vy_mps * kMetersPerSecondToFeetPerMinute);
}

bool BuildTrafficReportFromTcasSlot(const Settings& cfg,
                                    size_t slot,
                                    gdl90::PositionData* out_report) {
  if (!out_report || !g_state.traffic_tcas_refs.IsUsable()) {
    return false;
  }

  const int slot_index = ClampFloatToInt<int>(slot);
  const int raw_address =
      ReadIntArrayValue(g_state.traffic_tcas_refs.mode_s_ref, slot_index, 0);
  const uint32_t address = NormalizeTcasAddress(raw_address);
  if (address == 0u) {
    return false;
  }

  const float local_x =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.x_ref, slot_index, NAN);
  const float local_y =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.y_ref, slot_index, NAN);
  const float local_z =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.z_ref, slot_index, NAN);

  gdl90::PositionData report{};
  if (!LocalPositionToWorld(local_x, local_y, local_z, &report.latitude,
                            &report.longitude, &report.altitude)) {
    return false;
  }

  const float vx =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.vx_ref, slot_index, NAN);
  const float vy =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.vy_ref, slot_index, NAN);
  const float vz =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.vz_ref, slot_index, NAN);
  const float vertical_speed =
      ReadFloatArrayValue(g_state.traffic_tcas_refs.vertical_speed_ref,
                          slot_index, NAN);
  const float heading = ReadFloatArrayValue(g_state.traffic_tcas_refs.heading_ref,
                                            slot_index, NAN);
  const int weight_on_wheels = ReadIntArrayValue(
      g_state.traffic_tcas_refs.weight_on_wheels_ref, slot_index, -1);
  const int ssr_mode =
      ReadIntArrayValue(g_state.traffic_tcas_refs.ssr_mode_ref, slot_index, -1);
  const int squawk =
      ReadIntArrayValue(g_state.traffic_tcas_refs.mode_c_code_ref, slot_index, 0);
  const int wake_category =
      ReadIntArrayValue(g_state.traffic_tcas_refs.wake_cat_ref, slot_index, -1);

  const bool airborne = (weight_on_wheels >= 0)
                            ? (weight_on_wheels == 0)
                            : !TcasSsrModeIndicatesGround(ssr_mode);
  if (g_state.traffic_tcas_refs.ssr_mode_ref &&
      !TcasSsrModeHasAltitude(ssr_mode)) {
    report.altitude = std::numeric_limits<int32_t>::min();
  }

  report.h_velocity = CalculateHorizontalSpeedKnots(vx, vz);
  report.v_velocity = std::isfinite(static_cast<double>(vertical_speed))
                          ? ClampFpmToInt16OrInvalid(vertical_speed)
                          : CalculateVerticalSpeedFpm(vy);
  ResolveTrafficTrack(vx, vz, heading, &report.track, &report.track_type);
  report.airborne = airborne;
  report.nic = cfg.nic;
  report.nacp = cfg.nacp;
  report.icao_address = address;
  report.callsign = ReadTrafficCallsign(slot, report.icao_address);
  report.emitter_category = WakeCategoryToEmitterCategory(wake_category);
  report.address_type = ResolveTcasAddressType(raw_address);
  report.alert_status = 0;
  report.emergency_code = ResolveTrafficEmergencyCodeFromSquawk(squawk);

  *out_report = report;
  return true;
}

bool BuildTrafficReportFromLegacySlot(const Settings& cfg,
                                      size_t slot,
                                      gdl90::PositionData* out_report) {
  if (!out_report || slot < 1 || slot > g_state.legacy_traffic_refs.size()) {
    return false;
  }

  const LegacyTrafficRefs& refs = g_state.legacy_traffic_refs[slot - 1];
  if (!refs.IsUsable()) {
    return false;
  }

  const float local_x = XPLMGetDataf(refs.x_ref);
  const float local_y = XPLMGetDataf(refs.y_ref);
  const float local_z = XPLMGetDataf(refs.z_ref);
  const float vx = XPLMGetDataf(refs.vx_ref);
  const float vy = XPLMGetDataf(refs.vy_ref);
  const float vz = XPLMGetDataf(refs.vz_ref);
  const float heading = XPLMGetDataf(refs.heading_ref);

  const uint32_t synthetic_address =
      static_cast<uint32_t>(0xF00000u | (static_cast<uint32_t>(slot) & 0xFFFFu));
  const std::string callsign = ReadTrafficCallsign(slot, synthetic_address);

  if (!std::isfinite(static_cast<double>(local_x)) ||
      !std::isfinite(static_cast<double>(local_y)) ||
      !std::isfinite(static_cast<double>(local_z))) {
    return false;
  }
  if (local_x == 0.0f && local_y == 0.0f && local_z == 0.0f &&
      callsign == FormatTrafficFallbackCallsign(synthetic_address)) {
    return false;
  }

  gdl90::PositionData report{};
  if (!LocalPositionToWorld(local_x, local_y, local_z, &report.latitude,
                            &report.longitude, &report.altitude)) {
    return false;
  }

  report.h_velocity = CalculateHorizontalSpeedKnots(vx, vz);
  report.v_velocity = CalculateVerticalSpeedFpm(vy);
  report.airborne =
      report.h_velocity >= 35 || std::abs(static_cast<double>(vy)) >= 0.5;
  ResolveTrafficTrack(vx, vz, heading, &report.track, &report.track_type);
  report.nic = cfg.nic;
  report.nacp = cfg.nacp;
  report.icao_address = synthetic_address;
  report.callsign = callsign;
  report.emitter_category = gdl90::EmitterCategory::NO_INFO;
  report.address_type = gdl90::AddressType::ADSB_SELF_ASSIGNED;
  report.alert_status = 0;
  report.emergency_code = 0;

  *out_report = report;
  return true;
}

const char* GetTrafficSourceName() {
  if (g_state.traffic_tcas_refs.IsUsable()) {
    return "TCAS targets";
  }
  if (!g_state.legacy_traffic_refs.empty()) {
    return "Legacy multiplayer";
  }
  return "Unavailable";
}

size_t GetTrafficSourceSlotCount() {
  if (g_state.traffic_tcas_refs.IsUsable()) {
    return g_state.traffic_tcas_refs.slot_count;
  }
  return g_state.legacy_traffic_refs.size();
}

bool VerifyDataRef(XPLMDataRef ref, const char* name) {
  if (ref) {
    return true;
  }
  LogMessage(std::string("ERROR: Missing dataref ") + name);
  return false;
}

bool ReconfigureRuntimeReceivers(const Settings& cfg, std::string* out_error) {
  if (cfg.foreflight_auto_discovery) {
    if (!g_state.foreflight_receiver ||
        g_state.foreflight_receiver->getListenPort() !=
            cfg.foreflight_broadcast_port) {
      auto receiver =
          std::make_unique<udp::UDPReceiver>(cfg.foreflight_broadcast_port);
      if (!receiver->initialize()) {
        if (out_error) {
          *out_error = "Failed to initialize ForeFlight discovery listener: " +
                       receiver->getLastError();
        }
        return false;
      }
      g_state.foreflight_receiver = std::move(receiver);
      LogMessage("ForeFlight discovery listener active on UDP port " +
                 std::to_string(cfg.foreflight_broadcast_port));
    }
  } else {
    g_state.foreflight_receiver.reset();
    g_state.discovered_target_ip.clear();
    g_state.discovered_target_port = 0;
    g_state.last_foreflight_discovery = -1.0f;
    g_state.using_discovered_target = false;
  }

  if (out_error) {
    out_error->clear();
  }
  return true;
}

void RefreshBroadcastTarget(float sim_time, const Settings& cfg) {
  if (!g_state.broadcaster) {
    return;
  }

  const bool discovery_valid =
      cfg.foreflight_auto_discovery &&
      !g_state.discovered_target_ip.empty() &&
      g_state.discovered_target_port > 0 &&
      g_state.last_foreflight_discovery >= 0.0f &&
      (sim_time - g_state.last_foreflight_discovery) <=
          kForeFlightDiscoveryTimeout;

  const std::string resolved_ip =
      discovery_valid ? g_state.discovered_target_ip : cfg.target_ip;
  const uint16_t resolved_port =
      discovery_valid ? g_state.discovered_target_port : cfg.target_port;

  if (g_state.broadcaster->getTargetIp() != resolved_ip ||
      g_state.broadcaster->getTargetPort() != resolved_port) {
    g_state.broadcaster->setTarget(resolved_ip, resolved_port);
    LogMessage(std::string("Broadcast target updated: ") + resolved_ip + ":" +
               std::to_string(resolved_port) +
               (discovery_valid ? " (ForeFlight discovery)" : " (manual)"));
  }

  g_state.using_discovered_target = discovery_valid;
}

bool ApplyConfigToRuntime(const Settings& new_cfg,
                          std::string* out_error) {
  if (!g_state.broadcaster) {
    if (out_error) {
      *out_error = "Plugin not initialized";
    }
    return false;
  }
  if (!xp2gdl90::protocol::IsValidIpv4Address(new_cfg.target_ip)) {
    if (out_error) {
      *out_error = "Target IP must be a valid IPv4 address";
    }
    return false;
  }

  if (!ReconfigureRuntimeReceivers(new_cfg, out_error)) {
    return false;
  }

  g_state.settings = new_cfg;
  const float sim_time =
      g_state.sim_time_ref ? XPLMGetDataf(g_state.sim_time_ref) : 0.0f;
  RefreshBroadcastTarget(sim_time, g_state.settings);
  return true;
}

gdl90::PositionData GetOwnshipData(const Settings& cfg) {
  gdl90::PositionData data;

  const double latitude = XPLMGetDatad(g_state.lat_ref);
  const double longitude = XPLMGetDatad(g_state.lon_ref);
  const bool gps_valid =
      xp2gdl90::protocol::HasValidOwnshipPosition(latitude, longitude);
  data.latitude = gps_valid ? latitude : 0.0;
  data.longitude = gps_valid ? longitude : 0.0;

  if (g_state.pressure_alt_ref) {
    data.altitude = ClampFloatToInt<int32_t>(XPLMGetDataf(g_state.pressure_alt_ref));
  } else {
    data.altitude = std::numeric_limits<int32_t>::min();
  }

  const float speed_ms = XPLMGetDataf(g_state.speed_ref);
  data.h_velocity = ClampFloatToInt<uint16_t>(speed_ms * kMetersPerSecondToKnots);

  data.v_velocity = ClampFpmToInt16OrInvalid(XPLMGetDataf(g_state.vs_ref));

  data.track = NormalizeDegreesToUint16(XPLMGetDataf(g_state.track_ref));
  data.track_type = gdl90::TrackType::TRUE_TRACK;

  const int on_ground = XPLMGetDatai(g_state.airborne_ref);
  data.airborne = (on_ground == 0);

  data.icao_address = cfg.icao_address;

  const std::string tail_number =
      xp2gdl90::protocol::SanitizeCallsign(ReadTailNumber());
  const std::string fallback_callsign =
      xp2gdl90::protocol::SanitizeCallsign(cfg.callsign);
  data.callsign = tail_number.empty() ? fallback_callsign : tail_number;

  data.emitter_category =
      static_cast<gdl90::EmitterCategory>(cfg.emitter_category);
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.nic = gps_valid ? cfg.nic : 0;
  data.nacp = cfg.nacp;
  data.alert_status = 0;
  data.emergency_code = 0;

  return data;
}

gdl90::foreflight::DeviceInfo GetForeFlightDeviceInfo() {
  gdl90::foreflight::DeviceInfo data;
  data.serial_number = gdl90::foreflight::DEVICE_SERIAL_INVALID;
  data.device_name = g_state.settings.device_name;
  data.device_long_name = g_state.settings.device_long_name;
  data.capabilities_mask =
      0x01u | (static_cast<uint32_t>(g_state.settings.internet_policy & 0x03u) << 1);
  return data;
}

gdl90::foreflight::AhrsData GetOwnshipAhrsData() {
  gdl90::foreflight::AhrsData data;
  data.roll_deg = g_state.roll_ref ? XPLMGetDataf(g_state.roll_ref) : NAN;
  data.pitch_deg = g_state.pitch_ref ? XPLMGetDataf(g_state.pitch_ref) : NAN;
  if (g_state.heading_ref) {
    double heading_deg =
        NormalizeDegrees360(static_cast<double>(XPLMGetDataf(g_state.heading_ref)));
    if (g_state.settings.ahrs_use_magnetic_heading && std::isfinite(heading_deg)) {
      heading_deg = NormalizeDegrees360(
          static_cast<double>(XPLMDegTrueToDegMagnetic(static_cast<float>(heading_deg))));
    }
    data.heading_deg = heading_deg;
  } else {
    data.heading_deg = std::numeric_limits<double>::quiet_NaN();
  }
  data.magnetic_heading = g_state.settings.ahrs_use_magnetic_heading;
  data.indicated_airspeed =
      g_state.indicated_airspeed_ref
          ? ClampKnotsToUint16OrInvalid(XPLMGetDataf(g_state.indicated_airspeed_ref))
          : gdl90::foreflight::AHRS_AIRSPEED_INVALID;
  data.true_airspeed =
      g_state.true_airspeed_ref
          ? ClampKnotsToUint16OrInvalid(XPLMGetDataf(g_state.true_airspeed_ref))
          : gdl90::foreflight::AHRS_AIRSPEED_INVALID;
  return data;
}

gdl90::GeoAltitudeData GetOwnshipGeoAltitudeData() {
  gdl90::GeoAltitudeData data;
  const double altitude_meters = XPLMGetDatad(g_state.alt_ref);
  data.altitude_feet = ClampFloatToInt<int32_t>(altitude_meters * kMetersToFeet);
  data.vertical_warning = false;
  data.vfom_meters = gdl90::GEO_ALTITUDE_VFOM_INVALID;
  return data;
}

void InitializeTrafficDataRefs() {
  g_state.traffic_tcas_refs = {};
  g_state.legacy_traffic_refs.clear();
  g_state.traffic_text_refs.clear();

  g_state.traffic_tcas_refs.mode_s_ref =
      XPLMFindDataRef("sim/cockpit2/tcas/targets/modeS_id");
  if (g_state.traffic_tcas_refs.mode_s_ref) {
    const int array_size =
        XPLMGetDatavi(g_state.traffic_tcas_refs.mode_s_ref, nullptr, 0, 0);
    if (array_size > 1) {
      g_state.traffic_tcas_refs.slot_count =
          static_cast<size_t>(array_size - 1);
    }
  }

  if (g_state.traffic_tcas_refs.slot_count > 0) {
    g_state.traffic_tcas_refs.mode_c_code_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/modeC_code");
    g_state.traffic_tcas_refs.ssr_mode_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/ssr_mode");
    g_state.traffic_tcas_refs.flight_id_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/flight_id");
    g_state.traffic_tcas_refs.x_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/x");
    g_state.traffic_tcas_refs.y_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/y");
    g_state.traffic_tcas_refs.z_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/z");
    g_state.traffic_tcas_refs.vx_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vx");
    g_state.traffic_tcas_refs.vy_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vy");
    g_state.traffic_tcas_refs.vz_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vz");
    g_state.traffic_tcas_refs.vertical_speed_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/vertical_speed");
    g_state.traffic_tcas_refs.heading_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/psi");
    g_state.traffic_tcas_refs.weight_on_wheels_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/position/weight_on_wheels");
    g_state.traffic_tcas_refs.wake_cat_ref =
        XPLMFindDataRef("sim/cockpit2/tcas/targets/wake/wake_cat");
  }

  for (size_t slot = 1;; ++slot) {
    char buffer[128] = {};
    LegacyTrafficRefs refs;

    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_x", slot);
    refs.x_ref = XPLMFindDataRef(buffer);
    if (!refs.x_ref) {
      break;
    }

    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_y", slot);
    refs.y_ref = XPLMFindDataRef(buffer);
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_z", slot);
    refs.z_ref = XPLMFindDataRef(buffer);
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_v_x", slot);
    refs.vx_ref = XPLMFindDataRef(buffer);
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_v_y", slot);
    refs.vy_ref = XPLMFindDataRef(buffer);
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_v_z", slot);
    refs.vz_ref = XPLMFindDataRef(buffer);
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_psi", slot);
    refs.heading_ref = XPLMFindDataRef(buffer);

    if (!refs.IsUsable()) {
      break;
    }
    g_state.legacy_traffic_refs.push_back(refs);
  }

  const size_t text_slots = (std::max)(g_state.traffic_tcas_refs.slot_count,
                                       g_state.legacy_traffic_refs.size());
  g_state.traffic_text_refs.resize(text_slots);
  for (size_t slot = 1; slot <= text_slots; ++slot) {
    char buffer[128] = {};
    std::snprintf(buffer, sizeof(buffer),
                  "sim/multiplayer/position/plane%zu_tailnum", slot);
    g_state.traffic_text_refs[slot - 1].tailnum_ref = XPLMFindDataRef(buffer);
  }

  std::ostringstream message;
  message << "Traffic input initialized: source=" << GetTrafficSourceName()
          << ", slots=" << GetTrafficSourceSlotCount()
          << ", text_slots=" << g_state.traffic_text_refs.size();
  LogMessage(message.str());
}

size_t CollectTrafficData(const Settings& cfg,
                          std::vector<gdl90::PositionData>* out_reports) {
  if (!out_reports) {
    return 0;
  }

  out_reports->clear();

  if (g_state.traffic_tcas_refs.IsUsable()) {
    out_reports->reserve(g_state.traffic_tcas_refs.slot_count);
    for (size_t slot = 1; slot <= g_state.traffic_tcas_refs.slot_count; ++slot) {
      gdl90::PositionData report{};
      if (BuildTrafficReportFromTcasSlot(cfg, slot, &report)) {
        out_reports->push_back(report);
      }
    }
  }

  if (!out_reports->empty() || g_state.legacy_traffic_refs.empty()) {
    return out_reports->size();
  }

  out_reports->reserve(g_state.legacy_traffic_refs.size());
  for (size_t slot = 1; slot <= g_state.legacy_traffic_refs.size(); ++slot) {
    gdl90::PositionData report{};
    if (BuildTrafficReportFromLegacySlot(cfg, slot, &report)) {
      out_reports->push_back(report);
    }
  }

  return out_reports->size();
}

void SendTrafficReports(float sim_time, const Settings& cfg) {
  if (sim_time - g_state.last_traffic < (1.0f / kTrafficReportRate)) {
    return;
  }

  std::vector<gdl90::PositionData> reports;
  const size_t report_count = CollectTrafficData(cfg, &reports);

  int total_bytes = 0;
  bool saw_error = false;
  for (const gdl90::PositionData& report : reports) {
    const auto message = g_state.encoder->createTrafficReport(report);
    const int sent = g_state.broadcaster->send(message);
    if (sent >= 0) {
      total_bytes += sent;
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.traffic_packets_sent++;
    } else {
      saw_error = true;
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
  }

  g_state.last_traffic_send_bytes = total_bytes;
  g_state.last_traffic_target_count = static_cast<int>(report_count);
  if (!saw_error) {
    g_state.last_send_error.clear();
  }
  g_state.last_traffic = sim_time;
}

void PollForeFlightDiscovery(float sim_time, const Settings& cfg) {
  if (!cfg.foreflight_auto_discovery || !g_state.foreflight_receiver) {
    return;
  }

  while (true) {
    std::vector<uint8_t> packet;
    std::string source_ip;
    uint16_t source_port = 0;
    const int received = g_state.foreflight_receiver->receive(
        &packet, &source_ip, &source_port);
    if (received == 0) {
      break;
    }
    if (received < 0) {
      g_state.last_receiver_error = g_state.foreflight_receiver->getLastError();
      break;
    }

    uint16_t discovered_port = 0;
    if (!ParseForeFlightBroadcastPacket(packet, &discovered_port)) {
      continue;
    }

    g_state.discovered_target_ip = source_ip;
    g_state.discovered_target_port = discovered_port;
    g_state.last_foreflight_discovery = sim_time;
    RefreshBroadcastTarget(sim_time, cfg);
  }
}

bool SaveSettingsToDisk(std::string* out_error) {
  return xp2gdl90::SaveSettingsToJsonFile(g_state.settings_path, g_state.settings,
                                          out_error);
}

bool LoadSettingsFromDisk(Settings* out_settings, std::string* out_error) {
  return xp2gdl90::LoadSettingsFromJsonFile(g_state.settings_path, out_settings,
                                            out_error);
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
  xp2gdl90::SyncSettingsUiFromConfig(&g_state.settings_ui, g_state.settings);
  g_state.settings_dirty = false;
  g_state.settings_last_error.clear();
}

bool BuildConfigFromSettingsUi(Settings* out_cfg,
                               std::string* out_error) {
  return xp2gdl90::BuildConfigFromSettingsUi(g_state.settings_ui,
                                             g_state.settings, out_cfg,
                                             out_error);
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
  ImGui::Text("Target: %s:%u", g_state.broadcaster->getTargetIp().c_str(),
              static_cast<unsigned int>(g_state.broadcaster->getTargetPort()));

  const float sim_time =
      g_state.sim_time_ref ? XPLMGetDataf(g_state.sim_time_ref) : 0.0f;
  const float since_heartbeat =
      (g_state.last_heartbeat > 0.0f) ? (sim_time - g_state.last_heartbeat) : 0.0f;
  const float since_position =
      (g_state.last_position > 0.0f) ? (sim_time - g_state.last_position) : 0.0f;
  const float since_traffic =
      (g_state.last_traffic > 0.0f) ? (sim_time - g_state.last_traffic) : 0.0f;
  const float since_device_info =
      (g_state.last_device_info > 0.0f) ? (sim_time - g_state.last_device_info) : 0.0f;
  const float since_ahrs =
      (g_state.last_ahrs > 0.0f) ? (sim_time - g_state.last_ahrs) : 0.0f;
  const float since_geo_altitude =
      (g_state.last_geo_altitude > 0.0f) ? (sim_time - g_state.last_geo_altitude) : 0.0f;
  const float since_discovery =
      (g_state.last_foreflight_discovery >= 0.0f)
          ? (sim_time - g_state.last_foreflight_discovery)
          : -1.0f;

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
      ImGui::Text("Traffic source: %s (%zu slots)",
                  GetTrafficSourceName(), GetTrafficSourceSlotCount());
      ImGui::Text("Traffic reports: %llu (%d targets, %d bytes last, %.2fs ago)",
                  static_cast<unsigned long long>(g_state.traffic_packets_sent),
                  g_state.last_traffic_target_count,
                  g_state.last_traffic_send_bytes, since_traffic);
      ImGui::Text("ForeFlight ID: %llu (%d bytes last, %.2fs ago)",
                  static_cast<unsigned long long>(g_state.device_info_packets_sent),
                  g_state.last_device_info_send_bytes, since_device_info);
      ImGui::Text("AHRS packets: %llu (%d bytes last, %.2fs ago)",
                  static_cast<unsigned long long>(g_state.ahrs_packets_sent),
                  g_state.last_ahrs_send_bytes, since_ahrs);
      ImGui::Text("Ownship Geo Alt: %llu (%d bytes last, %.2fs ago)",
                  static_cast<unsigned long long>(g_state.geo_altitude_packets_sent),
                  g_state.last_geo_altitude_send_bytes, since_geo_altitude);
      ImGui::Text("Target mode: %s",
                  g_state.using_discovered_target ? "ForeFlight discovery" : "Manual");
      if (since_discovery >= 0.0f) {
        ImGui::Text("Last ForeFlight discovery: %s:%u (%.2fs ago)",
                    g_state.discovered_target_ip.c_str(),
                    static_cast<unsigned int>(g_state.discovered_target_port),
                    since_discovery);
      } else {
        ImGui::TextUnformatted("Last ForeFlight discovery: none");
      }
      ImGui::Text("AHRS heading mode: %s",
                  cfg.ahrs_use_magnetic_heading ? "Magnetic" : "True");
      ImGui::TextUnformatted("AHRS source: theta / phi / psi, indicated_airspeed, true_airspeed");
      ImGui::Text("Bytes sent: %llu",
                  static_cast<unsigned long long>(g_state.bytes_sent));

      if (!g_state.last_send_error.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last send error:");
        ImGui::TextWrapped("%s", g_state.last_send_error.c_str());
      }
      if (!g_state.last_receiver_error.empty()) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Last receiver error:");
        ImGui::TextWrapped("%s", g_state.last_receiver_error.c_str());
      }
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Network")) {
      dirty_now |= ImGui::InputText("Target IP", g_state.settings_ui.target_ip,
                                    sizeof(g_state.settings_ui.target_ip));
      dirty_now |= ImGui::InputInt("Target Port", &g_state.settings_ui.target_port);
      dirty_now |= ImGui::Checkbox("ForeFlight auto discovery",
                                   &g_state.settings_ui.foreflight_auto_discovery);
      dirty_now |= ImGui::InputInt("ForeFlight broadcast port",
                                   &g_state.settings_ui.foreflight_broadcast_port);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Ownship")) {
      dirty_now |= ImGui::InputText("ICAO Address (hex)",
                                    g_state.settings_ui.icao_address,
                                    sizeof(g_state.settings_ui.icao_address));
      dirty_now |= ImGui::InputText("Callsign (fallback)",
                                    g_state.settings_ui.callsign,
                                    sizeof(g_state.settings_ui.callsign));
      dirty_now |= ImGui::InputInt("Emitter Category",
                                   &g_state.settings_ui.emitter_category);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Device")) {
      dirty_now |= ImGui::InputText("Device Name", g_state.settings_ui.device_name,
                                    sizeof(g_state.settings_ui.device_name));
      dirty_now |= ImGui::InputText("Device Long Name",
                                    g_state.settings_ui.device_long_name,
                                    sizeof(g_state.settings_ui.device_long_name));
      dirty_now |= ImGui::InputInt("Internet Policy",
                                   &g_state.settings_ui.internet_policy);
      dirty_now |= ImGui::Checkbox("AHRS magnetic heading",
                                   &g_state.settings_ui.ahrs_use_magnetic_heading);
      ImGui::TextUnformatted("0=Unrestricted 1=Expensive 2=Disallowed");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rates")) {
      dirty_now |= ImGui::InputFloat("Heartbeat Rate (Hz)",
                                     &g_state.settings_ui.heartbeat_rate, 0.1f,
                                     1.0f, "%.2f");
      dirty_now |= ImGui::InputFloat("Position Rate (Hz)",
                                     &g_state.settings_ui.position_rate, 0.1f,
                                     1.0f, "%.2f");
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Accuracy")) {
      dirty_now |= ImGui::InputInt("NIC", &g_state.settings_ui.nic);
      dirty_now |= ImGui::InputInt("NACp", &g_state.settings_ui.nacp);
      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Debug")) {
      dirty_now |= ImGui::Checkbox("Debug logging", &g_state.settings_ui.debug_logging);
      dirty_now |= ImGui::Checkbox("Log raw messages", &g_state.settings_ui.log_messages);
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
    xp2gdl90::LoadDefaultSettingsUiState(&g_state.settings_ui);
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
  g_state.last_traffic = 0.0f;
  g_state.last_device_info = 0.0f;
  g_state.last_ahrs = 0.0f;
  g_state.last_geo_altitude = 0.0f;
  g_state.last_foreflight_discovery = -1.0f;
  g_state.discovered_target_ip.clear();
  g_state.discovered_target_port = 0;
  g_state.using_discovered_target = false;
  g_state.last_receiver_error.clear();

  g_state.encoder = std::make_unique<gdl90::GDL90Encoder>();
  g_state.foreflight_encoder =
      std::make_unique<gdl90::foreflight::ForeFlightEncoder>();

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
  g_state.pressure_alt_ref =
      XPLMFindDataRef("sim/cockpit2/gauges/indicators/altitude_ft_pilot");
  g_state.speed_ref = XPLMFindDataRef("sim/flightmodel/position/groundspeed");
  g_state.track_ref = XPLMFindDataRef("sim/flightmodel/position/true_psi");
  g_state.pitch_ref = XPLMFindDataRef("sim/flightmodel/position/theta");
  g_state.roll_ref = XPLMFindDataRef("sim/flightmodel/position/phi");
  g_state.heading_ref = XPLMFindDataRef("sim/flightmodel/position/psi");
  g_state.indicated_airspeed_ref =
      XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed");
  g_state.true_airspeed_ref =
      XPLMFindDataRef("sim/flightmodel/position/true_airspeed");
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
  datarefs_ok &= VerifyDataRef(g_state.pitch_ref,
                               "sim/flightmodel/position/theta");
  datarefs_ok &= VerifyDataRef(g_state.roll_ref,
                               "sim/flightmodel/position/phi");
  datarefs_ok &= VerifyDataRef(g_state.heading_ref,
                               "sim/flightmodel/position/psi");
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

  if (g_state.pressure_alt_ref) {
    LogMessage("Using pressure altitude dataref for Ownship Report altitude");
  } else {
    LogMessage("Pressure altitude dataref unavailable; Ownship Report altitude will be sent invalid");
  }

  LogMessage(std::string("AHRS heading mode: ") +
             (cfg.ahrs_use_magnetic_heading ? "magnetic" : "true"));

  InitializeTrafficDataRefs();

  std::string receiver_error;
  if (!ReconfigureRuntimeReceivers(cfg, &receiver_error)) {
    LogMessage("ERROR: " + receiver_error);
    return 0;
  }
  RefreshBroadcastTarget(0.0f, cfg);

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
  g_state.foreflight_receiver.reset();
  g_state.foreflight_encoder.reset();
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

  PollForeFlightDiscovery(sim_time, cfg);
  RefreshBroadcastTarget(sim_time, cfg);

  if (cfg.heartbeat_rate > 0.0f &&
      sim_time - g_state.last_heartbeat >= (1.0f / cfg.heartbeat_rate)) {
    const bool gps_valid = xp2gdl90::protocol::HasValidOwnshipPosition(
        XPLMGetDatad(g_state.lat_ref), XPLMGetDatad(g_state.lon_ref));
    const auto heartbeat = g_state.encoder->createHeartbeat(gps_valid, true);
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

  if (sim_time - g_state.last_geo_altitude >=
      (1.0f / kOwnshipGeoAltitudeRate)) {
    const auto geo_altitude = g_state.encoder->createOwnshipGeometricAltitude(
        GetOwnshipGeoAltitudeData());
    const int sent = g_state.broadcaster->send(geo_altitude);
    g_state.last_geo_altitude_send_bytes = sent;
    if (sent >= 0) {
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.geo_altitude_packets_sent++;
      g_state.last_send_error.clear();
    } else {
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
    g_state.last_geo_altitude = sim_time;
  }

  if (sim_time - g_state.last_device_info >= (1.0f / kForeFlightDeviceInfoRate)) {
    const auto device_info = g_state.foreflight_encoder->createIdMessage(
        GetForeFlightDeviceInfo());
    const int sent = g_state.broadcaster->send(device_info);
    g_state.last_device_info_send_bytes = sent;
    if (sent >= 0) {
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.device_info_packets_sent++;
      g_state.last_send_error.clear();
    } else {
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
    g_state.last_device_info = sim_time;
  }

  if (sim_time - g_state.last_ahrs >= (1.0f / kForeFlightAhrsRate)) {
    const auto ahrs_msg = g_state.foreflight_encoder->createAhrsMessage(
        GetOwnshipAhrsData());
    const int sent = g_state.broadcaster->send(ahrs_msg);
    g_state.last_ahrs_send_bytes = sent;
    if (sent >= 0) {
      g_state.bytes_sent += static_cast<uint64_t>(sent);
      g_state.ahrs_packets_sent++;
      g_state.last_send_error.clear();
    } else {
      g_state.last_send_error = g_state.broadcaster->getLastError();
    }
    g_state.last_ahrs = sim_time;
  }

  SendTrafficReports(sim_time, cfg);

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
