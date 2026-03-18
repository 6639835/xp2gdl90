#ifndef FOREFLIGHT_ENCODER_H
#define FOREFLIGHT_ENCODER_H

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace gdl90::foreflight {

constexpr uint8_t MSG_ID_FORE_FLIGHT = 0x65;
constexpr uint8_t SUB_ID_DEVICE_INFO = 0x00;
constexpr uint8_t SUB_ID_AHRS = 0x01;

constexpr uint64_t DEVICE_SERIAL_INVALID = 0xFFFFFFFFFFFFFFFFull;
constexpr int16_t AHRS_ATTITUDE_INVALID = 0x7FFF;
constexpr uint16_t AHRS_HEADING_INVALID = 0xFFFF;
constexpr uint16_t AHRS_AIRSPEED_INVALID = 0xFFFF;

struct DeviceInfo {
  uint64_t serial_number = DEVICE_SERIAL_INVALID;
  std::string device_name;
  std::string device_long_name;
  uint32_t capabilities_mask = 0;
};

struct AhrsData {
  double roll_deg = std::numeric_limits<double>::quiet_NaN();
  double pitch_deg = std::numeric_limits<double>::quiet_NaN();
  double heading_deg = std::numeric_limits<double>::quiet_NaN();
  bool magnetic_heading = false;
  uint16_t indicated_airspeed = AHRS_AIRSPEED_INVALID;
  uint16_t true_airspeed = AHRS_AIRSPEED_INVALID;
};

class ForeFlightEncoder {
 public:
  std::vector<uint8_t> createIdMessage(const DeviceInfo& data) const;
  std::vector<uint8_t> createAhrsMessage(const AhrsData& data) const;

 private:
  int16_t encodeAhrsAttitude(double degrees) const;
  uint16_t encodeAhrsHeading(double degrees, bool magnetic_heading) const;
};

}  // namespace gdl90::foreflight

#endif  // FOREFLIGHT_ENCODER_H
