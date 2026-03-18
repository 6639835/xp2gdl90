#include "xp2gdl90/gdl90_encoder.h"

#include "encoder_support.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <utility>

#if defined(_WIN32)
#include <time.h>
#endif

namespace gdl90 {

GDL90Encoder::GDL90Encoder() = default;

GDL90Encoder::GDL90Encoder(UtcTimeProvider utc_time_provider)
    : utc_time_provider_(
          [provider = std::move(utc_time_provider)](uint32_t* out_time) {
            if (!out_time || !provider) {
              return false;
            }
            *out_time = provider();
            return true;
          }) {}

GDL90Encoder::GDL90Encoder(CheckedUtcTimeProvider utc_time_provider)
    : utc_time_provider_(std::move(utc_time_provider)) {}

uint32_t GDL90Encoder::encodeLatitude(double latitude) const {
  latitude = std::max(-90.0, std::min(90.0, latitude));

  int32_t value =
      static_cast<int32_t>(latitude * (0x800000 / 180.0));
  if (value < 0) {
    value = (0x1000000 + value) & 0xFFFFFF;
  }

  return static_cast<uint32_t>(value);
}

uint32_t GDL90Encoder::encodeLongitude(double longitude) const {
  longitude = std::max(-180.0, std::min(180.0, longitude));

  int32_t value =
      static_cast<int32_t>(longitude * (0x800000 / 180.0));
  if (value < 0) {
    value = (0x1000000 + value) & 0xFFFFFF;
  }

  return static_cast<uint32_t>(value);
}

uint16_t GDL90Encoder::encodeAltitude(int32_t altitude) const {
  if (altitude == std::numeric_limits<int32_t>::min()) {
    return ALTITUDE_INVALID;
  }

  int64_t encoded = (static_cast<int64_t>(altitude) + 1000) / 25;

  if (encoded < 0) {
    encoded = 0;
  }
  if (encoded > 0xFFE) {
    encoded = 0xFFE;
  }

  return static_cast<uint16_t>(encoded);
}

uint16_t GDL90Encoder::encodeVerticalVelocity(int16_t vv_fpm) const {
  if (vv_fpm == INT16_MIN) {
    return VVELOCITY_INVALID;
  }

  if (vv_fpm > 32576) {
    return 0x1FE;
  }
  if (vv_fpm < -32576) {
    return 0xE02;
  }

  int16_t value = static_cast<int16_t>(vv_fpm / 64);
  if (value < 0) {
    value = (0x1000 + value) & 0xFFF;
  }

  return static_cast<uint16_t>(value);
}

uint8_t GDL90Encoder::encodeTrack(uint16_t track) const {
  return static_cast<uint8_t>((track % 360) * 256 / 360);
}

int16_t GDL90Encoder::encodeGeoAltitude(int32_t altitude_feet) const {
  int64_t value = std::llround(static_cast<double>(altitude_feet) / 5.0);
  if (value < std::numeric_limits<int16_t>::min()) {
    value = std::numeric_limits<int16_t>::min();
  }
  if (value > std::numeric_limits<int16_t>::max()) {
    value = std::numeric_limits<int16_t>::max();
  }
  return static_cast<int16_t>(value);
}

uint16_t GDL90Encoder::encodeGeoVerticalMetrics(bool vertical_warning,
                                                uint16_t vfom_meters) const {
  uint16_t encoded_vfom = vfom_meters;
  if (encoded_vfom != GEO_ALTITUDE_VFOM_INVALID &&
      encoded_vfom != GEO_ALTITUDE_VFOM_EXCESSIVE &&
      encoded_vfom >= GEO_ALTITUDE_VFOM_INVALID) {
    encoded_vfom = GEO_ALTITUDE_VFOM_EXCESSIVE;
  }

  encoded_vfom &= 0x7FFFu;
  if (vertical_warning) {
    encoded_vfom |= 0x8000u;
  }
  return encoded_vfom;
}

bool GDL90Encoder::getUTCTime(uint32_t* out_time) const {
  if (!out_time) {
    return false;
  }

  if (utc_time_provider_) {
    return utc_time_provider_(out_time);
  }

  std::time_t now = std::time(nullptr);
  if (now == static_cast<std::time_t>(-1)) {
    *out_time = 0;
    return false;
  }

  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  *out_time = static_cast<uint32_t>(utc.tm_hour * 3600 + utc.tm_min * 60 +
                                    utc.tm_sec);
  return true;
}

std::vector<uint8_t> GDL90Encoder::createHeartbeat(bool gps_valid,
                                                   bool utc_ok) const {
  std::vector<uint8_t> payload;
  payload.reserve(7);

  payload.push_back(MSG_ID_HEARTBEAT);

  uint8_t status1 = 0x01;
  if (gps_valid) {
    status1 |= 0x80;
  }
  payload.push_back(status1);

  uint32_t timestamp = 0;
  const bool have_utc_time = getUTCTime(&timestamp);
  uint8_t status2 = 0x00;
  if (utc_ok && have_utc_time) {
    status2 |= 0x01;
  }
  if (timestamp & 0x10000) {
    status2 |= 0x80;
  }
  payload.push_back(status2);

  payload.push_back(static_cast<uint8_t>(timestamp & 0xFF));
  payload.push_back(static_cast<uint8_t>((timestamp >> 8) & 0xFF));

  payload.push_back(0x00);
  payload.push_back(0x00);

  return internal::PrepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createPositionReport(
    uint8_t msg_id, const PositionData& data) const {
  std::vector<uint8_t> payload;
  payload.reserve(28);

  payload.push_back(msg_id);

  const uint8_t st =
      static_cast<uint8_t>(((data.alert_status & 0x0F) << 4) |
                           (static_cast<uint8_t>(data.address_type) & 0x0F));
  payload.push_back(st);

  internal::Pack24Bit(payload, data.icao_address);
  internal::Pack24Bit(payload, encodeLatitude(data.latitude));
  internal::Pack24Bit(payload, encodeLongitude(data.longitude));

  const uint16_t altitude = encodeAltitude(data.altitude);
  const uint8_t misc =
      static_cast<uint8_t>((static_cast<uint8_t>(data.airborne) << 3) |
                           (0 << 2) |
                           (static_cast<uint8_t>(data.track_type) & 0x03));

  payload.push_back(static_cast<uint8_t>((altitude >> 4) & 0xFF));
  payload.push_back(static_cast<uint8_t>(((altitude & 0x0F) << 4) | misc));

  payload.push_back(
      static_cast<uint8_t>(((data.nic & 0x0F) << 4) | (data.nacp & 0x0F)));

  const uint16_t h_vel =
      (data.h_velocity == VELOCITY_INVALID)
          ? VELOCITY_INVALID
          : std::min(data.h_velocity, static_cast<uint16_t>(0xFFE));
  const uint16_t v_vel = encodeVerticalVelocity(data.v_velocity);

  payload.push_back(static_cast<uint8_t>((h_vel >> 4) & 0xFF));
  payload.push_back(static_cast<uint8_t>(((h_vel & 0x0F) << 4) |
                                         ((v_vel >> 8) & 0x0F)));
  payload.push_back(static_cast<uint8_t>(v_vel & 0xFF));

  payload.push_back(encodeTrack(data.track));
  payload.push_back(static_cast<uint8_t>(data.emitter_category));

  std::string callsign = data.callsign.substr(0, 8);
  callsign.resize(8, ' ');
  for (const char ch : callsign) {
    payload.push_back(static_cast<uint8_t>(ch));
  }

  payload.push_back(static_cast<uint8_t>((data.emergency_code & 0x0F) << 4));

  return internal::PrepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createOwnshipReport(
    const PositionData& data) const {
  return createPositionReport(MSG_ID_OWNSHIP_REPORT, data);
}

std::vector<uint8_t> GDL90Encoder::createOwnshipGeometricAltitude(
    const GeoAltitudeData& data) const {
  std::vector<uint8_t> payload;
  payload.reserve(5);

  payload.push_back(MSG_ID_OWNSHIP_GEO_ALTITUDE);
  internal::AppendBigEndian16(
      payload, static_cast<uint16_t>(encodeGeoAltitude(data.altitude_feet)));
  internal::AppendBigEndian16(
      payload, encodeGeoVerticalMetrics(data.vertical_warning, data.vfom_meters));

  return internal::PrepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createTrafficReport(
    const PositionData& data) const {
  return createPositionReport(MSG_ID_TRAFFIC_REPORT, data);
}

}  // namespace gdl90
