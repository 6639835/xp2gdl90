#include "xp2gdl90/protocol_utils.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string_view>

namespace xp2gdl90::protocol {

std::string SanitizeCallsign(std::string_view input) {
  std::string out;
  out.reserve(8);

  for (const char ch : input) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isalnum(uch) != 0) {
      out.push_back(static_cast<char>(std::toupper(uch)));
    } else if (ch == ' ' || ch == '-' || ch == '_') {
      out.push_back(' ');
    }
    if (out.size() >= 8) {
      break;
    }
  }

  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
  return out;
}

bool IsValidIpv4Address(std::string_view input) {
  if (input.empty()) {
    return false;
  }

  int octet_count = 0;
  int octet_value = 0;
  bool saw_digit = false;

  for (const char ch : input) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (std::isdigit(uch) != 0) {
      saw_digit = true;
      octet_value = octet_value * 10 + (ch - '0');
      if (octet_value > 255) {
        return false;
      }
      continue;
    }

    if (ch != '.' || !saw_digit || octet_count >= 3) {
      return false;
    }

    ++octet_count;
    octet_value = 0;
    saw_digit = false;
  }

  return saw_digit && octet_count == 3;
}

bool IsValidNic(uint8_t value) {
  return value <= 11;
}

bool IsValidNacp(uint8_t value) {
  return value <= 11;
}

bool IsValidEmitterCategory(uint8_t value) {
  return value <= 39;
}

bool HasValidOwnshipPosition(double latitude, double longitude) {
  return std::isfinite(latitude) && std::isfinite(longitude) &&
         latitude >= -90.0 && latitude <= 90.0 &&
         longitude >= -180.0 && longitude <= 180.0;
}

}  // namespace xp2gdl90::protocol
