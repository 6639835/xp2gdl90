#include "xp2gdl90/foreflight_protocol.h"

#include <cctype>
#include <string>
#include <string_view>

namespace xp2gdl90::foreflight {
namespace {

std::string ExtractJsonStringValue(std::string_view json, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return "";
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string_view::npos) {
    return "";
  }
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '"') {
    return "";
  }
  ++pos;

  std::string value;
  while (pos < json.size()) {
    const char ch = json[pos++];
    if (ch == '"') {
      return value;
    }
    if (ch == '\\' && pos < json.size()) {
      value.push_back(json[pos++]);
      continue;
    }
    value.push_back(ch);
  }

  return "";
}

bool ExtractJsonUnsignedValue(std::string_view json,
                              std::string_view key,
                              unsigned int* out_value) {
  if (!out_value) {
    return false;
  }

  const std::string needle = "\"" + std::string(key) + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string_view::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
    ++pos;
  }

  const size_t start = pos;
  while (pos < json.size() &&
         std::isdigit(static_cast<unsigned char>(json[pos])) != 0) {
    ++pos;
  }
  if (start == pos) {
    return false;
  }

  try {
    *out_value = static_cast<unsigned int>(
        std::stoul(std::string(json.substr(start, pos - start)), nullptr, 10));
    return true;
  } catch (...) {
    return false;
  }
}

bool FindJsonObject(std::string_view json,
                    std::string_view key,
                    std::string_view* out_object) {
  if (!out_object) {
    return false;
  }

  const std::string needle = "\"" + std::string(key) + "\"";
  size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  pos = json.find(':', pos + needle.size());
  if (pos == std::string_view::npos) {
    return false;
  }
  ++pos;
  while (pos < json.size() &&
         std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
    ++pos;
  }
  if (pos >= json.size() || json[pos] != '{') {
    return false;
  }

  const size_t object_start = pos;
  size_t depth = 0;
  bool in_string = false;
  bool escaped = false;
  for (; pos < json.size(); ++pos) {
    const char ch = json[pos];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (ch == '\\') {
        escaped = true;
      } else if (ch == '"') {
        in_string = false;
      }
      continue;
    }

    if (ch == '"') {
      in_string = true;
      continue;
    }
    if (ch == '{') {
      ++depth;
      continue;
    }
    if (ch == '}') {
      if (depth == 0) {
        return false;
      }
      --depth;
      if (depth == 0) {
        *out_object = json.substr(object_start, pos - object_start + 1);
        return true;
      }
    }
  }

  return false;
}

}  // namespace

bool ParseDiscoveryBroadcast(const std::vector<uint8_t>& packet,
                             uint16_t* out_port) {
  if (!out_port || packet.empty()) {
    return false;
  }

  const std::string json(packet.begin(), packet.end());
  if (ExtractJsonStringValue(json, "App") != "ForeFlight") {
    return false;
  }

  std::string_view gdl90_object;
  if (!FindJsonObject(json, "GDL90", &gdl90_object)) {
    return false;
  }

  unsigned int port = 0;
  if (!ExtractJsonUnsignedValue(gdl90_object, "port", &port) ||
      port == 0 || port > 65535u) {
    return false;
  }

  *out_port = static_cast<uint16_t>(port);
  return true;
}

}  // namespace xp2gdl90::foreflight
