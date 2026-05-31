#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d3d11.h>
#include <dxgi.h>
#include <tchar.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include "xp2gdl90/foreflight_encoder.h"
#include "xp2gdl90/foreflight_protocol.h"
#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/msfs_bridge.h"
#include "xp2gdl90/protocol_utils.h"
#include "xp2gdl90/settings.h"
#include "xp2gdl90/settings_ui.h"
#include "xp2gdl90/simconnect_compat.h"
#include "xp2gdl90/udp_broadcaster.h"
#include "xp2gdl90/udp_receiver.h"

// Forward declaration required by imgui_impl_win32.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM,
                                                             LPARAM);

namespace {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

constexpr double kTrafficReportRate = 1.0;
constexpr double kForeFlightDeviceRate = 1.0;
constexpr double kForeFlightAhrsRate = 5.0;
constexpr double kGeoAltitudeRate = 1.0;
constexpr double kForeFlightDiscoveryTimeout = 15.0;
constexpr DWORD kTrafficRadiusMeters = 20000;
constexpr int kLogMaxLines = 500;
constexpr float kWindowWidth = 960.0f;
constexpr float kWindowHeight = 680.0f;

// ---------------------------------------------------------------------------
// SimConnect data structures (must match AddSimVar order exactly)
// ---------------------------------------------------------------------------

enum DataDefinitionId { kDefinitionOwnship = 1, kDefinitionTraffic = 2 };
enum DataRequestId { kRequestOwnship = 1, kRequestTraffic = 2 };

#pragma pack(push, 1)
struct OwnshipSimData {
  double latitude_deg = 0.0;         // PLANE LATITUDE, degrees, FLOAT64
  double longitude_deg = 0.0;        // PLANE LONGITUDE, degrees, FLOAT64
  double altitude_ft = 0.0;          // PLANE ALTITUDE, feet, FLOAT64
  double pressure_altitude_ft = 0.0; // PRESSURE ALTITUDE, feet, FLOAT64
  double ground_velocity_kt = 0.0;   // GROUND VELOCITY, knots, FLOAT64
  double vertical_speed_fps = 0.0;   // VERTICAL SPEED, feet per second, FLOAT64
  double true_heading_deg = 0.0; // PLANE HEADING DEGREES TRUE, degrees, FLOAT64
  double magnetic_heading_deg =
      0.0;                // PLANE HEADING DEGREES MAGNETIC, degrees, FLOAT64
  double pitch_deg = 0.0; // PLANE PITCH DEGREES, degrees, FLOAT64
  double bank_deg = 0.0;  // PLANE BANK DEGREES, degrees, FLOAT64
  double indicated_airspeed_kt = 0.0; // AIRSPEED INDICATED, knots, FLOAT64
  double true_airspeed_kt = 0.0;      // AIRSPEED TRUE, knots, FLOAT64
  INT32 sim_on_ground = 0;            // SIM ON GROUND, Bool, INT32
  char atc_id[32] = {};               // ATC ID, NULL, STRING32
};

struct TrafficSimData {
  double latitude_deg = 0.0;       // PLANE LATITUDE, degrees, FLOAT64
  double longitude_deg = 0.0;      // PLANE LONGITUDE, degrees, FLOAT64
  double altitude_ft = 0.0;        // PLANE ALTITUDE, feet, FLOAT64
  double ground_velocity_kt = 0.0; // GROUND VELOCITY, knots, FLOAT64
  double velocity_world_x_fps =
      0.0; // VELOCITY WORLD X, feet per second, FLOAT64
  double velocity_world_y_fps =
      0.0; // VELOCITY WORLD Y, feet per second, FLOAT64
  double velocity_world_z_fps =
      0.0;                       // VELOCITY WORLD Z, feet per second, FLOAT64
  double true_heading_deg = 0.0; // PLANE HEADING DEGREES TRUE, degrees, FLOAT64
  INT32 sim_on_ground = 0;       // SIM ON GROUND, Bool, INT32
  char atc_id[32] = {};          // ATC ID, NULL, STRING32
};
#pragma pack(pop)

struct TrafficEntry {
  DWORD object_id = 0;
  TrafficSimData data;
};

// ---------------------------------------------------------------------------
// Log buffer
// ---------------------------------------------------------------------------

struct LogEntry {
  std::string text;
  bool is_error = false;
};

struct LogBuffer {
  std::mutex mutex;
  std::deque<LogEntry> entries;
  bool scroll_to_bottom = false;

  void Add(bool is_error, std::string text) {
    std::lock_guard<std::mutex> lock(mutex);
    entries.push_back({std::move(text), is_error});
    if (static_cast<int>(entries.size()) > kLogMaxLines) {
      entries.pop_front();
    }
    scroll_to_bottom = true;
  }

  void Info(std::string text) { Add(false, std::move(text)); }
  void Error(std::string text) { Add(true, std::move(text)); }
};

LogBuffer g_log;

// ---------------------------------------------------------------------------
// Bridge state
// ---------------------------------------------------------------------------

struct BridgeState {
  xp2gdl90::Settings settings;
  xp2gdl90::SettingsUiState ui_state;
  std::string settings_path;
  std::string override_target_ip;
  uint16_t override_target_port = 0;
  bool settings_dirty = false;
  std::string settings_last_error;
  bool debug_flag = false; // --debug CLI override

  HANDLE simconnect = nullptr;
  bool simconnect_ready = false;
  bool ownship_valid = false;
  OwnshipSimData ownship;
  std::vector<TrafficEntry> traffic;

  std::unique_ptr<gdl90::GDL90Encoder> encoder;
  std::unique_ptr<gdl90::foreflight::ForeFlightEncoder> foreflight_encoder;
  std::unique_ptr<udp::UDPBroadcaster> broadcaster;
  std::unique_ptr<udp::UDPReceiver> foreflight_receiver;

  std::string discovered_target_ip;
  uint16_t discovered_target_port = 0;
  double last_foreflight_discovery = -1.0;
  bool using_discovered_target = false;

  double last_heartbeat = 0.0;
  double last_position = 0.0;
  double last_traffic = 0.0;
  double last_device_info = 0.0;
  double last_ahrs = 0.0;
  double last_geo_altitude = 0.0;
  double last_traffic_request = 0.0;
  double start_time = 0.0;

  uint64_t packets_sent = 0;
  int last_traffic_count = 0;
};

// ---------------------------------------------------------------------------
// DX11 objects
// ---------------------------------------------------------------------------

static ID3D11Device *g_device = nullptr;
static ID3D11DeviceContext *g_device_context = nullptr;
static IDXGISwapChain *g_swap_chain = nullptr;
static ID3D11RenderTargetView *g_rtv = nullptr;

bool CreateDeviceAndSwapChain(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferDesc.RefreshRate.Numerator = 60;
  sd.BufferDesc.RefreshRate.Denominator = 1;
  sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0,
                                      D3D_FEATURE_LEVEL_10_0};
  D3D_FEATURE_LEVEL level_out;
  return SUCCEEDED(D3D11CreateDeviceAndSwapChain(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
      D3D11_SDK_VERSION, &sd, &g_swap_chain, &g_device, &level_out,
      &g_device_context));
}

void CreateRenderTarget() {
  ID3D11Texture2D *back_buffer = nullptr;
  g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
  if (back_buffer) {
    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_rtv);
    back_buffer->Release();
  }
}

void CleanupRenderTarget() {
  if (g_rtv) {
    g_rtv->Release();
    g_rtv = nullptr;
  }
}

void CleanupDeviceAndSwapChain() {
  CleanupRenderTarget();
  if (g_swap_chain) {
    g_swap_chain->Release();
    g_swap_chain = nullptr;
  }
  if (g_device_context) {
    g_device_context->Release();
    g_device_context = nullptr;
  }
  if (g_device) {
    g_device->Release();
    g_device = nullptr;
  }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

double NowSeconds() {
  using Clock = std::chrono::steady_clock;
  static const auto kStart = Clock::now();
  return std::chrono::duration<double>(Clock::now() - kStart).count();
}

std::string HexHresult(HRESULT r) {
  std::ostringstream s;
  s << "0x" << std::hex << std::uppercase << static_cast<unsigned long>(r);
  return s.str();
}

std::string SafeString(const char *value, size_t size) {
  if (!value || size == 0)
    return "";
  const size_t len = strnlen_s(value, size);
  return msfs_bridge::TrimString(std::string(value, len));
}

std::string FormatUptime(double seconds) {
  const int s = static_cast<int>(seconds);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", s / 3600, (s % 3600) / 60,
                s % 60);
  return buf;
}

msfs_bridge::OwnshipData ToOwnshipData(const OwnshipSimData &sim) {
  msfs_bridge::OwnshipData out;
  out.latitude_deg = sim.latitude_deg;
  out.longitude_deg = sim.longitude_deg;
  out.altitude_ft = sim.altitude_ft;
  out.pressure_altitude_ft = sim.pressure_altitude_ft;
  out.ground_velocity_kt = sim.ground_velocity_kt;
  out.vertical_speed_fps = sim.vertical_speed_fps;
  out.true_heading_deg = sim.true_heading_deg;
  out.magnetic_heading_deg = sim.magnetic_heading_deg;
  out.pitch_deg = sim.pitch_deg;
  out.bank_deg = sim.bank_deg;
  out.indicated_airspeed_kt = sim.indicated_airspeed_kt;
  out.true_airspeed_kt = sim.true_airspeed_kt;
  out.sim_on_ground = (sim.sim_on_ground != 0);
  out.callsign = SafeString(sim.atc_id, sizeof(sim.atc_id));
  return out;
}

msfs_bridge::TrafficData ToTrafficData(DWORD object_id,
                                       const TrafficSimData &sim) {
  msfs_bridge::TrafficData out;
  out.object_id = static_cast<uint32_t>(object_id);
  out.latitude_deg = sim.latitude_deg;
  out.longitude_deg = sim.longitude_deg;
  out.altitude_ft = sim.altitude_ft;
  out.ground_velocity_kt = sim.ground_velocity_kt;
  out.velocity_world_x_fps = sim.velocity_world_x_fps;
  out.velocity_world_y_fps = sim.velocity_world_y_fps;
  out.velocity_world_z_fps = sim.velocity_world_z_fps;
  out.true_heading_deg = sim.true_heading_deg;
  out.sim_on_ground = (sim.sim_on_ground != 0);
  out.callsign = SafeString(sim.atc_id, sizeof(sim.atc_id));
  return out;
}

// ---------------------------------------------------------------------------
// SimConnect
// ---------------------------------------------------------------------------

bool AddSimVar(HANDLE handle, DataDefinitionId def, const char *name,
               const char *unit,
               SIMCONNECT_DATATYPE type = SIMCONNECT_DATATYPE_FLOAT64) {
  const HRESULT r =
      SimConnect_AddToDataDefinition(handle, def, name, unit, type);
  if (FAILED(r)) {
    g_log.Error(std::string("Failed to add SimVar '") + name +
                "': " + HexHresult(r));
    return false;
  }
  return true;
}

bool ConfigureSimConnectDefinitions(HANDLE handle) {
  bool ok = true;
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE LATITUDE", "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE LONGITUDE", "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE ALTITUDE", "feet");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PRESSURE ALTITUDE", "feet");
  ok &= AddSimVar(handle, kDefinitionOwnship, "GROUND VELOCITY", "knots");
  ok &= AddSimVar(handle, kDefinitionOwnship, "VERTICAL SPEED",
                  "feet per second");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE HEADING DEGREES TRUE",
                  "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE HEADING DEGREES MAGNETIC",
                  "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE PITCH DEGREES", "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "PLANE BANK DEGREES", "degrees");
  ok &= AddSimVar(handle, kDefinitionOwnship, "AIRSPEED INDICATED", "knots");
  ok &= AddSimVar(handle, kDefinitionOwnship, "AIRSPEED TRUE", "knots");
  ok &= AddSimVar(handle, kDefinitionOwnship, "SIM ON GROUND", "Bool",
                  SIMCONNECT_DATATYPE_INT32);
  ok &= AddSimVar(handle, kDefinitionOwnship, "ATC ID", "NULL",
                  SIMCONNECT_DATATYPE_STRING32);

  ok &= AddSimVar(handle, kDefinitionTraffic, "PLANE LATITUDE", "degrees");
  ok &= AddSimVar(handle, kDefinitionTraffic, "PLANE LONGITUDE", "degrees");
  ok &= AddSimVar(handle, kDefinitionTraffic, "PLANE ALTITUDE", "feet");
  ok &= AddSimVar(handle, kDefinitionTraffic, "GROUND VELOCITY", "knots");
  ok &= AddSimVar(handle, kDefinitionTraffic, "VELOCITY WORLD X",
                  "feet per second");
  ok &= AddSimVar(handle, kDefinitionTraffic, "VELOCITY WORLD Y",
                  "feet per second");
  ok &= AddSimVar(handle, kDefinitionTraffic, "VELOCITY WORLD Z",
                  "feet per second");
  ok &= AddSimVar(handle, kDefinitionTraffic, "PLANE HEADING DEGREES TRUE",
                  "degrees");
  ok &= AddSimVar(handle, kDefinitionTraffic, "SIM ON GROUND", "Bool",
                  SIMCONNECT_DATATYPE_INT32);
  ok &= AddSimVar(handle, kDefinitionTraffic, "ATC ID", "NULL",
                  SIMCONNECT_DATATYPE_STRING32);
  if (!ok)
    return false;

  const HRESULT r = SimConnect_RequestDataOnSimObject(
      handle, kRequestOwnship, kDefinitionOwnship, SIMCONNECT_OBJECT_ID_USER,
      SIMCONNECT_PERIOD_SIM_FRAME);
  if (FAILED(r)) {
    g_log.Error("Failed to request ownship data: " + HexHresult(r));
    return false;
  }
  return true;
}

bool ConnectSimConnect(BridgeState *state) {
  if (state->simconnect)
    return true;
  const HRESULT r =
      SimConnect_Open(&state->simconnect, "MSFS2GDL90", nullptr, 0, nullptr, 0);
  if (FAILED(r)) {
    state->simconnect = nullptr;
    return false;
  }
  if (!ConfigureSimConnectDefinitions(state->simconnect)) {
    SimConnect_Close(state->simconnect);
    state->simconnect = nullptr;
    return false;
  }
  state->simconnect_ready = true;
  g_log.Info("Connected to Microsoft Flight Simulator via SimConnect.");
  return true;
}

void DisconnectSimConnect(BridgeState *state) {
  if (!state->simconnect)
    return;
  SimConnect_Close(state->simconnect);
  state->simconnect = nullptr;
  state->simconnect_ready = false;
  state->ownship_valid = false;
  state->traffic.clear();
  g_log.Info("Disconnected from SimConnect.");
}

void DispatchSimConnectMessage(BridgeState *state, SIMCONNECT_RECV *msg,
                               DWORD) {
  switch (msg->dwID) {
  case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
  case SIMCONNECT_RECV_ID_SIMOBJECT_DATA_BYTYPE: {
    const auto *data =
        reinterpret_cast<const SIMCONNECT_RECV_SIMOBJECT_DATA *>(msg);
    if (data->dwRequestID == kRequestOwnship) {
      state->ownship = *reinterpret_cast<const OwnshipSimData *>(&data->dwData);
      state->ownship_valid = true;
    } else if (data->dwRequestID == kRequestTraffic &&
               data->dwObjectID != SIMCONNECT_OBJECT_ID_USER) {
      TrafficEntry entry;
      entry.object_id = data->dwObjectID;
      entry.data = *reinterpret_cast<const TrafficSimData *>(&data->dwData);
      state->traffic.push_back(entry);
    }
    break;
  }
  case SIMCONNECT_RECV_ID_EXCEPTION: {
    const auto *ex = reinterpret_cast<const SIMCONNECT_RECV_EXCEPTION *>(msg);
    std::ostringstream s;
    s << "SimConnect exception " << ex->dwException
      << " send_id=" << ex->dwSendID << " index=" << ex->dwIndex;
    g_log.Error(s.str());
    break;
  }
  case SIMCONNECT_RECV_ID_QUIT:
    DisconnectSimConnect(state);
    break;
  default:
    break;
  }
}

void PollSimConnect(BridgeState *state) {
  while (state->simconnect) {
    SIMCONNECT_RECV *msg = nullptr;
    DWORD size = 0;
    const HRESULT r =
        SimConnect_GetNextDispatch(state->simconnect, &msg, &size);
    if (r == E_FAIL)
      break;
    if (FAILED(r)) {
      g_log.Error("SimConnect dispatch failed: " + HexHresult(r));
      DisconnectSimConnect(state);
      break;
    }
    DispatchSimConnectMessage(state, msg, size);
  }
}

// ---------------------------------------------------------------------------
// ForeFlight discovery
// ---------------------------------------------------------------------------

void PollForeFlightDiscovery(BridgeState *state, double now) {
  if (!state->settings.foreflight_auto_discovery || !state->foreflight_receiver)
    return;
  while (true) {
    std::vector<uint8_t> packet;
    std::string source_ip;
    uint16_t source_port = 0;
    const int r =
        state->foreflight_receiver->receive(&packet, &source_ip, &source_port);
    if (r == 0)
      break;
    if (r < 0) {
      g_log.Error("ForeFlight discovery error: " +
                  state->foreflight_receiver->getLastError());
      break;
    }
    uint16_t port = 0;
    if (!xp2gdl90::foreflight::ParseDiscoveryBroadcast(packet, &port))
      continue;
    if (source_ip != state->discovered_target_ip ||
        port != state->discovered_target_port) {
      g_log.Info("ForeFlight discovered at " + source_ip + ":" +
                 std::to_string(port));
    }
    state->discovered_target_ip = source_ip;
    state->discovered_target_port = port;
    state->last_foreflight_discovery = now;
  }
}

void RefreshBroadcastTarget(BridgeState *state, double now) {
  const xp2gdl90::Settings &cfg = state->settings;
  const bool discovery_valid =
      cfg.foreflight_auto_discovery && !state->discovered_target_ip.empty() &&
      state->discovered_target_port > 0 &&
      state->last_foreflight_discovery >= 0.0 &&
      (now - state->last_foreflight_discovery) <= kForeFlightDiscoveryTimeout;

  const std::string &ip =
      discovery_valid ? state->discovered_target_ip : cfg.target_ip;
  const uint16_t port =
      discovery_valid ? state->discovered_target_port : cfg.target_port;

  if (state->broadcaster->getTargetIp() != ip ||
      state->broadcaster->getTargetPort() != port) {
    state->broadcaster->setTarget(ip, port);
    g_log.Info("Broadcast target: " + ip + ":" + std::to_string(port) +
               (discovery_valid ? " (ForeFlight discovery)" : " (manual)"));
  }
  state->using_discovered_target = discovery_valid;
}

// ---------------------------------------------------------------------------
// Packet sending
// ---------------------------------------------------------------------------

void SendPacket(BridgeState *state, const std::vector<uint8_t> &packet) {
  const int sent = state->broadcaster->send(packet);
  if (sent < 0) {
    g_log.Error("UDP send failed: " + state->broadcaster->getLastError());
    return;
  }
  ++state->packets_sent;
  if (state->settings.log_messages) {
    g_log.Info("Sent " + std::to_string(packet.size()) + " bytes (total " +
               std::to_string(state->packets_sent) + ")");
  }
}

void RequestTrafficIfDue(BridgeState *state, double now) {
  if (!state->simconnect || now - state->last_traffic_request < 1.0)
    return;
  state->last_traffic_request = now;
  state->traffic.clear();
  const HRESULT r = SimConnect_RequestDataOnSimObjectType(
      state->simconnect, kRequestTraffic, kDefinitionTraffic,
      kTrafficRadiusMeters, SIMCONNECT_SIMOBJECT_TYPE_AIRCRAFT);
  if (FAILED(r))
    g_log.Error("Failed to request traffic: " + HexHresult(r));
}

void SendScheduledPackets(BridgeState *state, double now) {
  const xp2gdl90::Settings &cfg = state->settings;
  const bool gps_valid =
      state->ownship_valid &&
      xp2gdl90::protocol::HasValidOwnshipPosition(state->ownship.latitude_deg,
                                                  state->ownship.longitude_deg);

  if (cfg.heartbeat_rate > 0.0f &&
      now - state->last_heartbeat >= 1.0 / cfg.heartbeat_rate) {
    SendPacket(state, state->encoder->createHeartbeat(gps_valid, true));
    state->last_heartbeat = now;
  }

  if (!state->ownship_valid)
    return;

  const msfs_bridge::OwnshipData own = ToOwnshipData(state->ownship);

  if (cfg.debug_logging &&
      now - state->last_position >= 1.0 / cfg.position_rate) {
    g_log.Info("[debug] ownship lat=" + std::to_string(own.latitude_deg) +
               " lon=" + std::to_string(own.longitude_deg) +
               " palt=" + std::to_string(own.pressure_altitude_ft) + "ft" +
               " gs=" + std::to_string(own.ground_velocity_kt) + "kt" +
               " hdg=" + std::to_string(own.true_heading_deg) + " gnd=" +
               (own.sim_on_ground ? "Y" : "N") + " cs=" + own.callsign);
  }

  if (cfg.position_rate > 0.0f &&
      now - state->last_position >= 1.0 / cfg.position_rate) {
    SendPacket(state, state->encoder->createOwnshipReport(
                          msfs_bridge::BuildOwnshipPosition(own, cfg)));
    state->last_position = now;
  }

  if (now - state->last_geo_altitude >= 1.0 / kGeoAltitudeRate) {
    SendPacket(state, state->encoder->createOwnshipGeometricAltitude(
                          msfs_bridge::BuildGeoAltitude(own)));
    state->last_geo_altitude = now;
  }

  if (now - state->last_device_info >= 1.0 / kForeFlightDeviceRate) {
    SendPacket(state, state->foreflight_encoder->createIdMessage(
                          msfs_bridge::BuildDeviceInfo(cfg)));
    state->last_device_info = now;
  }

  if (now - state->last_ahrs >= 1.0 / kForeFlightAhrsRate) {
    SendPacket(state, state->foreflight_encoder->createAhrsMessage(
                          msfs_bridge::BuildAhrs(own, cfg)));
    state->last_ahrs = now;
  }

  if (now - state->last_traffic >= 1.0 / kTrafficReportRate) {
    state->last_traffic_count = static_cast<int>(state->traffic.size());
    if (cfg.debug_logging) {
      g_log.Info("[debug] sending " +
                 std::to_string(state->last_traffic_count) +
                 " traffic report(s)");
    }
    for (const TrafficEntry &entry : state->traffic) {
      gdl90::PositionData pos;
      if (msfs_bridge::BuildTrafficPosition(
              ToTrafficData(entry.object_id, entry.data), cfg, &pos)) {
        SendPacket(state, state->encoder->createTrafficReport(pos));
      }
    }
    state->last_traffic = now;
  }
}

// ---------------------------------------------------------------------------
// Networking init
// ---------------------------------------------------------------------------

bool InitializeNetworking(BridgeState *state) {
  state->broadcaster = std::make_unique<udp::UDPBroadcaster>(
      state->settings.target_ip, state->settings.target_port);
  if (!state->broadcaster->initialize()) {
    g_log.Error("Failed to initialize UDP broadcaster: " +
                state->broadcaster->getLastError());
    return false;
  }
  if (state->settings.foreflight_auto_discovery) {
    state->foreflight_receiver = std::make_unique<udp::UDPReceiver>(
        state->settings.foreflight_broadcast_port);
    if (!state->foreflight_receiver->initialize()) {
      g_log.Error("Failed to initialize ForeFlight discovery listener: " +
                  state->foreflight_receiver->getLastError());
      return false;
    }
  }
  g_log.Info("Broadcast target: " + state->settings.target_ip + ":" +
             std::to_string(state->settings.target_port));
  return true;
}

// ---------------------------------------------------------------------------
// Settings apply (from UI)
// ---------------------------------------------------------------------------

void ApplySettings(BridgeState *state) {
  xp2gdl90::Settings new_cfg;
  std::string error;
  if (!xp2gdl90::BuildConfigFromSettingsUi(state->ui_state, state->settings,
                                           &new_cfg, &error)) {
    state->settings_last_error = error;
    return;
  }
  if (!xp2gdl90::protocol::IsValidIpv4Address(new_cfg.target_ip)) {
    state->settings_last_error = "Target IP must be a valid IPv4 address.";
    return;
  }
  new_cfg.heartbeat_rate = (std::max)(0.0f, new_cfg.heartbeat_rate);
  new_cfg.position_rate = (std::max)(0.0f, new_cfg.position_rate);
  new_cfg.icao_address &= 0x00FFFFFFu;

  if (state->broadcaster) {
    state->broadcaster->setTarget(new_cfg.target_ip, new_cfg.target_port);
  }
  if (state->settings.foreflight_auto_discovery !=
          new_cfg.foreflight_auto_discovery ||
      state->settings.foreflight_broadcast_port !=
          new_cfg.foreflight_broadcast_port) {
    state->foreflight_receiver.reset();
    if (new_cfg.foreflight_auto_discovery) {
      state->foreflight_receiver =
          std::make_unique<udp::UDPReceiver>(new_cfg.foreflight_broadcast_port);
      state->foreflight_receiver->initialize();
    }
  }

  state->settings = new_cfg;
  state->settings_dirty = false;
  state->settings_last_error.clear();
  g_log.Info("Settings applied.");

  std::string save_error;
  if (!xp2gdl90::SaveSettingsToJsonFile(state->settings_path, state->settings,
                                        &save_error)) {
    g_log.Error("Could not save settings: " + save_error);
  }
}

// ---------------------------------------------------------------------------
// UI rendering
// ---------------------------------------------------------------------------

void RenderUi(BridgeState *state, double now) {
  const ImGuiIO &io = ImGui::GetIO();

  // Full-screen root window
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("##root", nullptr,
               ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                   ImGuiWindowFlags_NoScrollWithMouse);

  // ---- Status bar --------------------------------------------------------
  const bool connected = state->simconnect_ready;
  if (connected) {
    ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.2f, 1.0f), "● Connected");
  } else {
    ImGui::TextColored(ImVec4(0.9f, 0.4f, 0.4f, 1.0f), "○ Waiting for MSFS...");
  }
  ImGui::SameLine(180.0f);

  const std::string &target_ip = state->using_discovered_target
                                     ? state->discovered_target_ip
                                     : state->settings.target_ip;
  const uint16_t target_port = state->using_discovered_target
                                   ? state->discovered_target_port
                                   : state->settings.target_port;
  ImGui::Text("Target: %s:%d%s", target_ip.c_str(), target_port,
              state->using_discovered_target ? " (FF)" : "");

  ImGui::SameLine(440.0f);
  ImGui::Text("Traffic: %d", state->last_traffic_count);

  ImGui::SameLine(560.0f);
  ImGui::Text("Pkts: %" PRIu64, state->packets_sent);

  ImGui::SameLine(680.0f);
  ImGui::Text("Up: %s", FormatUptime(now - state->start_time).c_str());

  ImGui::Separator();

  // ---- Two-column body ---------------------------------------------------
  const float body_height = io.DisplaySize.y - ImGui::GetCursorPosY() -
                            ImGui::GetFrameHeightWithSpacing() * 0.5f;
  const float left_width = io.DisplaySize.x * 0.55f;
  const float right_width =
      io.DisplaySize.x - left_width - ImGui::GetStyle().ItemSpacing.x;

  // Left: Settings
  ImGui::BeginChild("##settings", ImVec2(left_width, body_height), true);
  ImGui::TextUnformatted("Settings");
  ImGui::Separator();

  bool dirty_now = false;
  if (ImGui::BeginTabBar("##tabs")) {
    if (ImGui::BeginTabItem("Network")) {
      dirty_now |= ImGui::InputText("Target IP", state->ui_state.target_ip,
                                    sizeof(state->ui_state.target_ip));
      dirty_now |= ImGui::InputInt("Target Port", &state->ui_state.target_port);
      dirty_now |= ImGui::Checkbox("ForeFlight auto discovery",
                                   &state->ui_state.foreflight_auto_discovery);
      dirty_now |= ImGui::InputInt("ForeFlight broadcast port",
                                   &state->ui_state.foreflight_broadcast_port);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Ownship")) {
      dirty_now |=
          ImGui::InputText("ICAO Address (hex)", state->ui_state.icao_address,
                           sizeof(state->ui_state.icao_address));
      dirty_now |=
          ImGui::InputText("Callsign (fallback)", state->ui_state.callsign,
                           sizeof(state->ui_state.callsign));
      dirty_now |= ImGui::InputInt("Emitter Category",
                                   &state->ui_state.emitter_category);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Device")) {
      dirty_now |= ImGui::InputText("Device Name", state->ui_state.device_name,
                                    sizeof(state->ui_state.device_name));
      dirty_now |=
          ImGui::InputText("Device Long Name", state->ui_state.device_long_name,
                           sizeof(state->ui_state.device_long_name));
      dirty_now |=
          ImGui::InputInt("Internet Policy", &state->ui_state.internet_policy);
      dirty_now |= ImGui::Checkbox("AHRS magnetic heading",
                                   &state->ui_state.ahrs_use_magnetic_heading);
      ImGui::TextDisabled("0=Unrestricted  1=Expensive  2=Disallowed");
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Rates")) {
      dirty_now |= ImGui::InputFloat("Heartbeat Rate (Hz)",
                                     &state->ui_state.heartbeat_rate, 0.1f,
                                     1.0f, "%.2f");
      dirty_now |=
          ImGui::InputFloat("Position Rate (Hz)",
                            &state->ui_state.position_rate, 0.1f, 1.0f, "%.2f");
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Accuracy")) {
      dirty_now |= ImGui::InputInt("NIC", &state->ui_state.nic);
      dirty_now |= ImGui::InputInt("NACp", &state->ui_state.nacp);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Debug")) {
      dirty_now |=
          ImGui::Checkbox("Debug logging", &state->ui_state.debug_logging);
      dirty_now |=
          ImGui::Checkbox("Log raw messages", &state->ui_state.log_messages);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }

  if (dirty_now)
    state->settings_dirty = true;

  ImGui::Separator();
  if (state->settings_dirty) {
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.1f, 1.0f), "Unsaved changes.");
  } else {
    ImGui::TextDisabled("No pending changes.");
  }
  if (!state->settings_last_error.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                       state->settings_last_error.c_str());
  }

  ImGui::BeginDisabled(!state->settings_dirty);
  if (ImGui::Button("Apply & Save")) {
    ApplySettings(state);
    xp2gdl90::SyncSettingsUiFromConfig(&state->ui_state, state->settings);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!state->settings_dirty);
  if (ImGui::Button("Revert")) {
    xp2gdl90::SyncSettingsUiFromConfig(&state->ui_state, state->settings);
    state->settings_dirty = false;
    state->settings_last_error.clear();
  }
  ImGui::EndDisabled();

  ImGui::EndChild(); // settings

  // Right: Log
  ImGui::SameLine();
  ImGui::BeginChild("##log", ImVec2(right_width, body_height), true);
  ImGui::TextUnformatted("Log");
  ImGui::Separator();

  ImGui::BeginChild("##log_scroll", ImVec2(0, 0), false,
                    ImGuiWindowFlags_HorizontalScrollbar);
  {
    std::lock_guard<std::mutex> lock(g_log.mutex);
    for (const LogEntry &entry : g_log.entries) {
      if (entry.is_error) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                           entry.text.c_str());
      } else {
        ImGui::TextUnformatted(entry.text.c_str());
      }
    }
    if (g_log.scroll_to_bottom) {
      ImGui::SetScrollHereY(1.0f);
      g_log.scroll_to_bottom = false;
    }
  }
  ImGui::EndChild(); // log_scroll
  ImGui::EndChild(); // log

  ImGui::End(); // root
}

// ---------------------------------------------------------------------------
// Win32 window proc
// ---------------------------------------------------------------------------

static HWND g_hwnd = nullptr;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam))
    return true;
  switch (msg) {
  case WM_SIZE:
    if (g_device && wparam != SIZE_MINIMIZED) {
      CleanupRenderTarget();
      g_swap_chain->ResizeBuffers(0, LOWORD(lparam), HIWORD(lparam),
                                  DXGI_FORMAT_UNKNOWN, 0);
      CreateRenderTarget();
    }
    return 0;
  case WM_SYSCOMMAND:
    if ((wparam & 0xFFF0) == SC_KEYMENU)
      return 0;
    break;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  default:
    break;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

std::string DefaultSettingsPath() {
  char *appdata = nullptr;
  size_t sz = 0;
  if (_dupenv_s(&appdata, &sz, "APPDATA") != 0 || !appdata)
    return "msfs2gdl90.json";
  std::filesystem::path p(appdata);
  free(appdata);
  return (p / "xp2gdl90" / "msfs2gdl90.json").string();
}

void EnsureSettingsDirectory(const std::string &path) {
  std::error_code ec;
  const auto parent = std::filesystem::path(path).parent_path();
  if (!parent.empty())
    std::filesystem::create_directories(parent, ec);
}

void PrintUsage() {
  MessageBoxA(nullptr,
              "msfs2gdl90.exe [--config PATH] [--target-ip IPv4] "
              "[--target-port PORT] [--debug]\n\n"
              "MSFS 2020/2024 SimConnect to GDL90 bridge.\n\n"
              "  --config PATH     Path to JSON settings file\n"
              "  --target-ip IPv4  Override broadcast target IP\n"
              "  --target-port N   Override broadcast target port\n"
              "  --debug           Enable verbose debug logging",
              "msfs2gdl90", MB_OK | MB_ICONINFORMATION);
}

bool ParseArgs(int argc, char **argv, BridgeState *state) {
  state->settings_path = DefaultSettingsPath();
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return false;
    }
    if ((arg == "--config" || arg == "-c") && i + 1 < argc) {
      state->settings_path = argv[++i];
      continue;
    }
    if (arg == "--target-ip" && i + 1 < argc) {
      state->override_target_ip = argv[++i];
      continue;
    }
    if (arg == "--target-port" && i + 1 < argc) {
      const int port = std::atoi(argv[++i]);
      if (port <= 0 || port > 65535) {
        MessageBoxA(nullptr, "Invalid target port.", "msfs2gdl90",
                    MB_OK | MB_ICONERROR);
        return false;
      }
      state->override_target_port = static_cast<uint16_t>(port);
      continue;
    }
    if (arg == "--debug") {
      state->debug_flag = true;
      continue;
    }
    MessageBoxA(nullptr, ("Unknown argument: " + arg).c_str(), "msfs2gdl90",
                MB_OK | MB_ICONERROR);
    return false;
  }
  return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Entry point  (linked with /ENTRY:mainCRTStartup so we keep int main)
// ---------------------------------------------------------------------------

int main(int argc, char **argv) {
  BridgeState state;
  if (!ParseArgs(argc, argv, &state))
    return 1;

  // Load settings
  const bool debug_flag = state.debug_flag;
  std::string load_error;
  xp2gdl90::Settings loaded = state.settings;
  if (xp2gdl90::LoadSettingsFromJsonFile(state.settings_path, &loaded,
                                         &load_error)) {
    state.settings = loaded;
    g_log.Info("Settings loaded: " + state.settings_path);
  } else {
    if (!load_error.empty())
      g_log.Error("Settings load error: " + load_error);
    EnsureSettingsDirectory(state.settings_path);
    std::string save_error;
    if (xp2gdl90::SaveSettingsToJsonFile(state.settings_path, state.settings,
                                         &save_error)) {
      g_log.Info("Created default settings: " + state.settings_path);
    } else {
      g_log.Error("Could not create default settings: " + save_error);
    }
  }

  if (debug_flag)
    state.settings.debug_logging = true;
  if (!state.override_target_ip.empty())
    state.settings.target_ip = state.override_target_ip;
  if (state.override_target_port > 0)
    state.settings.target_port = state.override_target_port;
  state.settings.heartbeat_rate =
      (std::max)(0.0f, state.settings.heartbeat_rate);
  state.settings.position_rate = (std::max)(0.0f, state.settings.position_rate);
  state.settings.icao_address &= 0x00FFFFFFu;

  if (!xp2gdl90::protocol::IsValidIpv4Address(state.settings.target_ip)) {
    MessageBoxA(nullptr,
                ("Invalid target IP: " + state.settings.target_ip).c_str(),
                "msfs2gdl90", MB_OK | MB_ICONERROR);
    return 1;
  }

  xp2gdl90::SyncSettingsUiFromConfig(&state.ui_state, state.settings);

  state.encoder = std::make_unique<gdl90::GDL90Encoder>();
  state.foreflight_encoder =
      std::make_unique<gdl90::foreflight::ForeFlightEncoder>();

  if (!InitializeNetworking(&state)) {
    MessageBoxA(nullptr, "Failed to initialize networking.", "msfs2gdl90",
                MB_OK | MB_ICONERROR);
    return 1;
  }

  // Create window
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"MSFS2GDL90";
  wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
  RegisterClassExW(&wc);

  g_hwnd = CreateWindowExW(
      0, L"MSFS2GDL90", L"MSFS → GDL90", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
      CW_USEDEFAULT, static_cast<int>(kWindowWidth),
      static_cast<int>(kWindowHeight), nullptr, nullptr, wc.hInstance, nullptr);

  if (!CreateDeviceAndSwapChain(g_hwnd)) {
    MessageBoxA(nullptr, "Failed to create Direct3D 11 device.", "msfs2gdl90",
                MB_OK | MB_ICONERROR);
    return 1;
  }
  CreateRenderTarget();
  ShowWindow(g_hwnd, SW_SHOWDEFAULT);
  UpdateWindow(g_hwnd);

  // ImGui setup
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.IniFilename = nullptr; // don't write imgui.ini
  ImGui::StyleColorsDark();
  ImGui_ImplWin32_Init(g_hwnd);
  ImGui_ImplDX11_Init(g_device, g_device_context);

  g_log.Info("Waiting for MSFS 2020/2024...");
  state.start_time = NowSeconds();

  double last_connect_attempt = -10.0;
  bool running = true;

  while (running) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT)
        running = false;
    }
    if (!running)
      break;

    const double now = NowSeconds();

    // SimConnect work
    if (!state.simconnect && now - last_connect_attempt >= 2.0) {
      last_connect_attempt = now;
      ConnectSimConnect(&state);
    }
    PollSimConnect(&state);
    RequestTrafficIfDue(&state, now);
    PollForeFlightDiscovery(&state, now);
    RefreshBroadcastTarget(&state, now);
    SendScheduledPackets(&state, now);

    // Render
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    RenderUi(&state, now);

    ImGui::Render();
    constexpr float kClear[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    g_device_context->OMSetRenderTargets(1, &g_rtv, nullptr);
    g_device_context->ClearRenderTargetView(g_rtv, kClear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swap_chain->Present(1, 0); // vsync
  }

  DisconnectSimConnect(&state);

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  CleanupDeviceAndSwapChain();
  DestroyWindow(g_hwnd);
  UnregisterClassW(L"MSFS2GDL90", wc.hInstance);

  return 0;
}
