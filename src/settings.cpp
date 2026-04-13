#include "xp2gdl90/settings.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>

#include "xp2gdl90/protocol_utils.h"
#include "xp2gdl90/simple_json.h"

namespace xp2gdl90 {
namespace {

bool ReadUnsignedPort(const json::Value* value, uint16_t* out_port) {
  if (!value || !value->IsNumber() || !std::isfinite(value->number_value) ||
      value->number_value < 1.0 || value->number_value > 65535.0) {
    return false;
  }
  *out_port = static_cast<uint16_t>(static_cast<unsigned int>(value->number_value));
  return true;
}

bool ReadPositiveRate(const json::Value* value, float* out_rate) {
  if (!value || !value->IsNumber() || !std::isfinite(value->number_value) ||
      value->number_value <= 0.0 ||
      value->number_value > static_cast<double>(std::numeric_limits<float>::max())) {
    return false;
  }
  *out_rate = static_cast<float>(value->number_value);
  return true;
}

bool ReadUInt8(const json::Value* value, uint8_t* out_value) {
  if (!value || !value->IsNumber() || !std::isfinite(value->number_value) ||
      value->number_value < 0.0 || value->number_value > 255.0) {
    return false;
  }
  *out_value = static_cast<uint8_t>(static_cast<unsigned int>(value->number_value));
  return true;
}

}  // namespace

bool LoadSettingsFromJsonFile(const std::string& path,
                              Settings* out_settings,
                              std::string* out_error) {
  if (!out_settings) {
    if (out_error) {
      *out_error = "Output settings object is required";
    }
    return false;
  }

  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    if (out_error) {
      out_error->clear();
    }
    return false;
  }

  std::stringstream buffer;
  buffer << file.rdbuf();

  json::Value root;
  std::string parse_error;
  if (!json::Parse(buffer.str(), &root, &parse_error)) {
    if (out_error) {
      *out_error = "Invalid settings JSON: " + parse_error;
    }
    return false;
  }
  if (!root.IsObject()) {
    if (out_error) {
      *out_error = "Invalid settings JSON: expected top-level object";
    }
    return false;
  }

  Settings settings = *out_settings;

  if (const json::Value* value = root.Find("target_ip");
      value && value->IsString() &&
      protocol::IsValidIpv4Address(value->string_value)) {
    settings.target_ip = value->string_value;
  }

  uint16_t port = 0;
  if (ReadUnsignedPort(root.Find("target_port"), &port)) {
    settings.target_port = port;
  }
  if (ReadUnsignedPort(root.Find("foreflight_broadcast_port"), &port)) {
    settings.foreflight_broadcast_port = port;
  }

  if (const json::Value* value = root.Find("foreflight_auto_discovery");
      value && value->IsBool()) {
    settings.foreflight_auto_discovery = value->bool_value;
  }

  if (const json::Value* value = root.Find("icao_address");
      value && value->IsNumber() && std::isfinite(value->number_value) &&
      value->number_value >= 0.0) {
    settings.icao_address =
        static_cast<uint32_t>(static_cast<unsigned int>(value->number_value)) &
        0xFFFFFFu;
  }

  if (const json::Value* value = root.Find("callsign");
      value && value->IsString() && !value->string_value.empty()) {
    settings.callsign = value->string_value.substr(0, 8);
  }
  if (const json::Value* value = root.Find("device_name");
      value && value->IsString() && !value->string_value.empty()) {
    settings.device_name = value->string_value.substr(0, 8);
  }
  if (const json::Value* value = root.Find("device_long_name");
      value && value->IsString() && !value->string_value.empty()) {
    settings.device_long_name = value->string_value.substr(0, 16);
  }

  uint8_t byte_value = 0;
  if (ReadUInt8(root.Find("emitter_category"), &byte_value) &&
      protocol::IsValidEmitterCategory(byte_value)) {
    settings.emitter_category = byte_value;
  }
  if (ReadUInt8(root.Find("internet_policy"), &byte_value) && byte_value <= 2u) {
    settings.internet_policy = byte_value;
  }
  if (ReadUInt8(root.Find("nic"), &byte_value) && protocol::IsValidNic(byte_value)) {
    settings.nic = byte_value;
  }
  if (ReadUInt8(root.Find("nacp"), &byte_value) &&
      protocol::IsValidNacp(byte_value)) {
    settings.nacp = byte_value;
  }

  float rate = 0.0f;
  if (ReadPositiveRate(root.Find("heartbeat_rate"), &rate)) {
    settings.heartbeat_rate = rate;
  }
  if (ReadPositiveRate(root.Find("position_rate"), &rate)) {
    settings.position_rate = rate;
  }

  if (const json::Value* value = root.Find("ahrs_use_magnetic_heading");
      value && value->IsBool()) {
    settings.ahrs_use_magnetic_heading = value->bool_value;
  }
  if (const json::Value* value = root.Find("debug_logging");
      value && value->IsBool()) {
    settings.debug_logging = value->bool_value;
  }
  if (const json::Value* value = root.Find("log_messages");
      value && value->IsBool()) {
    settings.log_messages = value->bool_value;
  }

  *out_settings = settings;
  if (out_error) {
    out_error->clear();
  }
  return true;
}

bool SaveSettingsToJsonFile(const std::string& path,
                            const Settings& settings,
                            std::string* out_error) {
  if (!protocol::IsValidIpv4Address(settings.target_ip)) {
    if (out_error) {
      *out_error = "Target IP must be a valid IPv4 address";
    }
    return false;
  }

  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  if (!file.is_open()) {
    if (out_error) {
      *out_error = "Failed to open settings file for writing: " + path;
    }
    return false;
  }

  file << "{\n";
  file << "  \"target_ip\": \""
       << json::EscapeString(settings.target_ip) << "\",\n";
  file << "  \"target_port\": "
       << static_cast<unsigned int>(settings.target_port) << ",\n";
  file << "  \"foreflight_auto_discovery\": "
       << (settings.foreflight_auto_discovery ? "true" : "false") << ",\n";
  file << "  \"foreflight_broadcast_port\": "
       << static_cast<unsigned int>(settings.foreflight_broadcast_port) << ",\n";
  file << "  \"icao_address\": "
       << static_cast<unsigned int>(settings.icao_address & 0xFFFFFFu) << ",\n";
  file << "  \"callsign\": \"" << json::EscapeString(settings.callsign)
       << "\",\n";
  file << "  \"emitter_category\": "
       << static_cast<unsigned int>(settings.emitter_category) << ",\n";
  file << "  \"device_name\": \""
       << json::EscapeString(settings.device_name) << "\",\n";
  file << "  \"device_long_name\": \""
       << json::EscapeString(settings.device_long_name) << "\",\n";
  file << "  \"internet_policy\": "
       << static_cast<unsigned int>(settings.internet_policy) << ",\n";
  file << "  \"ahrs_use_magnetic_heading\": "
       << (settings.ahrs_use_magnetic_heading ? "true" : "false") << ",\n";
  file << "  \"heartbeat_rate\": " << settings.heartbeat_rate << ",\n";
  file << "  \"position_rate\": " << settings.position_rate << ",\n";
  file << "  \"nic\": " << static_cast<unsigned int>(settings.nic) << ",\n";
  file << "  \"nacp\": " << static_cast<unsigned int>(settings.nacp) << ",\n";
  file << "  \"debug_logging\": "
       << (settings.debug_logging ? "true" : "false") << ",\n";
  file << "  \"log_messages\": "
       << (settings.log_messages ? "true" : "false") << "\n";
  file << "}\n";

  if (!file.good()) {
    if (out_error) {
      *out_error = "Failed to write settings file: " + path;
    }
    return false;
  }

  if (out_error) {
    out_error->clear();
  }
  return true;
}

}  // namespace xp2gdl90
