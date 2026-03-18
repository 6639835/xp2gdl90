#include "xp2gdl90/foreflight_encoder.h"

#include "encoder_support.h"

#include <cmath>

namespace gdl90::foreflight {

int16_t ForeFlightEncoder::encodeAhrsAttitude(double degrees) const {
  if (!std::isfinite(degrees)) {
    return AHRS_ATTITUDE_INVALID;
  }

  const long value = std::lround(degrees * 10.0);
  if (value < -1800 || value > 1800) {
    return AHRS_ATTITUDE_INVALID;
  }

  return static_cast<int16_t>(value);
}

uint16_t ForeFlightEncoder::encodeAhrsHeading(double degrees,
                                              bool magnetic_heading) const {
  if (!std::isfinite(degrees)) {
    return AHRS_HEADING_INVALID;
  }

  double normalized = std::fmod(degrees, 360.0);
  if (normalized < 0.0) {
    normalized += 360.0;
  }

  long value = std::lround(normalized * 10.0);
  if (value == 3600) {
    value = 0;
  }
  if (value < 0 || value > 3600) {
    return AHRS_HEADING_INVALID;
  }

  uint16_t encoded = static_cast<uint16_t>(value) & 0x7FFF;
  if (magnetic_heading) {
    encoded |= 0x8000;
  }
  return encoded;
}

std::vector<uint8_t> ForeFlightEncoder::createIdMessage(
    const DeviceInfo& data) const {
  std::vector<uint8_t> payload;
  payload.reserve(39);

  payload.push_back(MSG_ID_FORE_FLIGHT);
  payload.push_back(SUB_ID_DEVICE_INFO);
  payload.push_back(0x01);
  internal::AppendBigEndian64(payload, data.serial_number);
  internal::AppendFixedText(payload, data.device_name, 8);
  internal::AppendFixedText(payload, data.device_long_name, 16);
  internal::AppendBigEndian32(payload, data.capabilities_mask);

  return internal::PrepareMessage(payload);
}

std::vector<uint8_t> ForeFlightEncoder::createAhrsMessage(
    const AhrsData& data) const {
  std::vector<uint8_t> payload;
  payload.reserve(12);

  payload.push_back(MSG_ID_FORE_FLIGHT);
  payload.push_back(SUB_ID_AHRS);
  internal::AppendBigEndian16(
      payload, static_cast<uint16_t>(encodeAhrsAttitude(data.roll_deg)));
  internal::AppendBigEndian16(
      payload, static_cast<uint16_t>(encodeAhrsAttitude(data.pitch_deg)));
  internal::AppendBigEndian16(
      payload, encodeAhrsHeading(data.heading_deg, data.magnetic_heading));
  internal::AppendBigEndian16(payload, data.indicated_airspeed);
  internal::AppendBigEndian16(payload, data.true_airspeed);

  return internal::PrepareMessage(payload);
}

}  // namespace gdl90::foreflight
