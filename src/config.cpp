#include "xp2gdl90/config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace config {

Config::Config()
    : target_ip("192.168.1.100"),
      target_port(4000),
      icao_address(0xABCDEF),
      callsign("N12345"),
      emitter_category(1),
      heartbeat_rate(1.0f),
      position_rate(2.0f),
      nic(11),
      nacp(11),
      debug_logging(false),
      log_messages(false) {}

ConfigManager::ConfigManager() : config_(), last_error_() {}

std::string ConfigManager::trim(const std::string& str) {
  const size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, last - first + 1);
}

bool ConfigManager::parseBool(const std::string& value) {
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return (lower == "true" || lower == "yes" || lower == "1" ||
          lower == "on");
}

void ConfigManager::parseLine(const std::string& line) {
  if (line.empty() || line[0] == '#' || line[0] == ';') {
    return;
  }

  const size_t eq_pos = line.find('=');
  if (eq_pos == std::string::npos) {
    return;
  }

  const std::string key = trim(line.substr(0, eq_pos));
  std::string value = trim(line.substr(eq_pos + 1));

  size_t comment_pos = value.find('#');
  if (comment_pos != std::string::npos) {
    value = trim(value.substr(0, comment_pos));
  }
  comment_pos = value.find(';');
  if (comment_pos != std::string::npos) {
    value = trim(value.substr(0, comment_pos));
  }

  if (value.empty()) {
    return;
  }

  if (key == "target_ip") {
    config_.target_ip = value;
  } else if (key == "target_port") {
    const unsigned long port = std::stoul(value);
    if (port == 0 || port > 65535) {
      throw std::out_of_range("target_port must be 1-65535");
    }
    config_.target_port = static_cast<uint16_t>(port);
  } else if (key == "icao_address") {
    if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
      config_.icao_address = std::stoul(value, nullptr, 16);
    } else {
      config_.icao_address = std::stoul(value);
    }
    config_.icao_address &= 0xFFFFFF;
  } else if (key == "callsign") {
    config_.callsign = value.substr(0, 8);
  } else if (key == "emitter_category") {
    config_.emitter_category = static_cast<uint8_t>(std::stoi(value));
  } else if (key == "heartbeat_rate") {
    const float rate = std::stof(value);
    if (rate <= 0.0f) {
      throw std::out_of_range("heartbeat_rate must be > 0");
    }
    config_.heartbeat_rate = rate;
  } else if (key == "position_rate") {
    const float rate = std::stof(value);
    if (rate <= 0.0f) {
      throw std::out_of_range("position_rate must be > 0");
    }
    config_.position_rate = rate;
  } else if (key == "nic") {
    config_.nic = static_cast<uint8_t>(std::stoi(value));
  } else if (key == "nacp") {
    config_.nacp = static_cast<uint8_t>(std::stoi(value));
  } else if (key == "debug_logging") {
    config_.debug_logging = parseBool(value);
  } else if (key == "log_messages") {
    config_.log_messages = parseBool(value);
  }
}

bool ConfigManager::load(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    last_error_ = "Failed to open config file: " + filename;
    return false;
  }

  const Config original = config_;
  bool had_error = false;
  std::string line;
  while (std::getline(file, line)) {
    try {
      parseLine(line);
    } catch (const std::exception& e) {
      last_error_ =
          "Error parsing line: " + line + " (" + e.what() + ")";
      had_error = true;
    }
  }

  if (had_error) {
    config_ = original;
    return false;
  }

  last_error_.clear();
  return true;
}

bool ConfigManager::save(const std::string& filename) {
  std::ofstream file(filename);
  if (!file.is_open()) {
    last_error_ = "Failed to open config file for writing: " + filename;
    return false;
  }

  file << "# XP2GDL90 Configuration File\n";
  file << "# Generated configuration\n\n";

  file << "[Network]\n";
  file << "target_ip = " << config_.target_ip << "\n";
  file << "target_port = " << config_.target_port << "\n\n";

  file << "[Ownship]\n";
  file << "icao_address = 0x" << std::hex << config_.icao_address << std::dec
       << "\n";
  file << "callsign = " << config_.callsign << "\n";
  file << "emitter_category = "
       << static_cast<int>(config_.emitter_category) << "\n\n";

  file << "[Update Rates]\n";
  file << "heartbeat_rate = " << config_.heartbeat_rate << "\n";
  file << "position_rate = " << config_.position_rate << "\n\n";

  file << "[Accuracy]\n";
  file << "nic = " << static_cast<int>(config_.nic) << "\n";
  file << "nacp = " << static_cast<int>(config_.nacp) << "\n\n";

  file << "[Debug]\n";
  file << "debug_logging = "
       << (config_.debug_logging ? "true" : "false") << "\n";
  file << "log_messages = "
       << (config_.log_messages ? "true" : "false") << "\n";

  last_error_.clear();
  return true;
}

}  // namespace config
