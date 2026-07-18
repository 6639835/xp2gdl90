#include "xp2gdl90/traffic_support.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace xp2gdl90::traffic {
namespace {

constexpr uint32_t kAddressMask = 0xFFFFFFu;
constexpr uint32_t kSyntheticPrefix = 0xF00000u;
constexpr double kPresenceEpsilon = 1e-6;
// X-Plane fills unused TCAS float-array slots with -FLT_MAX. Local coordinates
// are metres in the simulator's local frame, so this bound is deliberately far
// beyond any usable target while still rejecting that finite sentinel.
constexpr double kMaxLocalCoordinateMagnitude = 1.0e9;
constexpr double kMaxVelocityMagnitude = 1.0e6;

bool IsUsableMagnitude(double value, double maximum) {
  return std::isfinite(value) && std::abs(value) < maximum;
}

bool HasMeaningfulMotion(double value) {
  return IsUsableMagnitude(value, kMaxVelocityMagnitude) &&
         std::abs(value) > kPresenceEpsilon;
}

} // namespace

bool IsPopulatedTcasTarget(const TcasPresenceSample &sample) {
  if (sample.slot == 0 ||
      !IsUsableMagnitude(sample.local_x, kMaxLocalCoordinateMagnitude) ||
      !IsUsableMagnitude(sample.local_y, kMaxLocalCoordinateMagnitude) ||
      !IsUsableMagnitude(sample.local_z, kMaxLocalCoordinateMagnitude)) {
    return false;
  }

  const uint32_t address =
      static_cast<uint32_t>(sample.raw_address) & kAddressMask;
  return address != 0u || !sample.callsign.empty() || sample.ssr_mode > 0 ||
         std::abs(sample.local_x) > kPresenceEpsilon ||
         std::abs(sample.local_y) > kPresenceEpsilon ||
         std::abs(sample.local_z) > kPresenceEpsilon ||
         HasMeaningfulMotion(sample.velocity_x) ||
         HasMeaningfulMotion(sample.velocity_y) ||
         HasMeaningfulMotion(sample.velocity_z);
}

uint32_t SyntheticTrafficAddress(size_t slot, const std::string &callsign,
                                 uint32_t ownship_address) {
  uint32_t hash = 2166136261u;
  const auto mix = [&hash](uint8_t value) {
    hash ^= value;
    hash *= 16777619u;
  };

  // Prefer identity over array position so a target keeps its synthetic
  // address when X-Plane or a traffic injector reorders TCAS slots.
  if (!callsign.empty()) {
    for (unsigned char value : callsign) {
      mix(value);
    }
  } else {
    for (size_t shift = 0; shift < sizeof(slot); ++shift) {
      mix(static_cast<uint8_t>((slot >> (shift * 8u)) & 0xFFu));
    }
  }

  uint32_t address = kSyntheticPrefix | (hash & 0x0FFFFFu);
  if (address == (ownship_address & kAddressMask)) {
    address = kSyntheticPrefix | ((address + 1u) & 0x0FFFFFu);
  }
  return address;
}

int32_t CorrectGeometricToPressureAltitude(int32_t geometric_altitude_feet,
                                           double ownship_geometric_feet,
                                           double ownship_pressure_feet) {
  if (geometric_altitude_feet == std::numeric_limits<int32_t>::min() ||
      !std::isfinite(ownship_geometric_feet) ||
      !std::isfinite(ownship_pressure_feet)) {
    return geometric_altitude_feet;
  }

  const double corrected = static_cast<double>(geometric_altitude_feet) +
                           ownship_pressure_feet - ownship_geometric_feet;
  const double low =
      static_cast<double>(std::numeric_limits<int32_t>::min() + 1);
  const double high = static_cast<double>(std::numeric_limits<int32_t>::max());
  return static_cast<int32_t>((std::max)(low, (std::min)(high, corrected)));
}

} // namespace xp2gdl90::traffic
