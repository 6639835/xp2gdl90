#include "xp2gdl90/gdl90_encoder.h"

#include <algorithm>
#include <cmath>

#if defined(_WIN32)
#include <time.h>
#endif

namespace gdl90 {

const uint16_t GDL90Encoder::crc16_table_[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0};

GDL90Encoder::GDL90Encoder() = default;

uint16_t GDL90Encoder::calculateCRC(const std::vector<uint8_t>& data) const {
  uint16_t crc = 0;
  for (const uint8_t byte : data) {
    crc = crc16_table_[crc >> 8] ^ static_cast<uint16_t>(crc << 8) ^ byte;
  }
  return crc;
}

std::vector<uint8_t> GDL90Encoder::escapeMessage(
    const std::vector<uint8_t>& data) const {
  std::vector<uint8_t> escaped;
  escaped.reserve(data.size() + 10);

  for (const uint8_t byte : data) {
    if (byte == 0x7D || byte == 0x7E) {
      escaped.push_back(0x7D);
      escaped.push_back(static_cast<uint8_t>(byte ^ 0x20));
    } else {
      escaped.push_back(byte);
    }
  }

  return escaped;
}

std::vector<uint8_t> GDL90Encoder::prepareMessage(
    const std::vector<uint8_t>& payload) const {
  const uint16_t crc = calculateCRC(payload);

  std::vector<uint8_t> message = payload;
  message.push_back(static_cast<uint8_t>(crc & 0xFF));
  message.push_back(static_cast<uint8_t>((crc >> 8) & 0xFF));

  std::vector<uint8_t> escaped = escapeMessage(message);

  std::vector<uint8_t> final_message;
  final_message.reserve(escaped.size() + 2);
  final_message.push_back(0x7E);
  final_message.insert(final_message.end(), escaped.begin(), escaped.end());
  final_message.push_back(0x7E);

  return final_message;
}

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
  int32_t encoded = (altitude + 1000) / 25;

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

void GDL90Encoder::pack24bit(std::vector<uint8_t>& buffer,
                             uint32_t value) const {
  buffer.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
  buffer.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
  buffer.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint32_t GDL90Encoder::getUTCTime() const {
  std::time_t now = std::time(nullptr);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  return static_cast<uint32_t>(utc.tm_hour * 3600 + utc.tm_min * 60 +
                               utc.tm_sec);
}

std::vector<uint8_t> GDL90Encoder::createHeartbeat(bool gps_valid,
                                                   bool utc_ok) {
  std::vector<uint8_t> payload;
  payload.reserve(7);

  payload.push_back(MSG_ID_HEARTBEAT);

  uint8_t status1 = 0x01;
  if (gps_valid) {
    status1 |= 0x80;
  }
  payload.push_back(status1);

  const uint32_t timestamp = getUTCTime();
  uint8_t status2 = 0x00;
  if (utc_ok) {
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

  return prepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createPositionReport(
    uint8_t msg_id, const PositionData& data) {
  std::vector<uint8_t> payload;
  payload.reserve(28);

  payload.push_back(msg_id);

  const uint8_t st =
      static_cast<uint8_t>(((data.alert_status & 0x0F) << 4) |
                           (static_cast<uint8_t>(data.address_type) & 0x0F));
  payload.push_back(st);

  pack24bit(payload, data.icao_address);
  pack24bit(payload, encodeLatitude(data.latitude));
  pack24bit(payload, encodeLongitude(data.longitude));

  const uint16_t altitude = encodeAltitude(data.altitude);
  const uint8_t misc =
      static_cast<uint8_t>((static_cast<uint8_t>(data.airborne) << 3) |
                           (0 << 2) |
                           (static_cast<uint8_t>(data.track_type) & 0x03));

  payload.push_back(static_cast<uint8_t>((altitude >> 4) & 0xFF));
  payload.push_back(static_cast<uint8_t>(((altitude & 0x0F) << 4) | misc));

  payload.push_back(
      static_cast<uint8_t>(((data.nic & 0x0F) << 4) | (data.nacp & 0x0F)));

  const uint16_t h_vel = std::min(data.h_velocity, static_cast<uint16_t>(0xFFE));
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

  return prepareMessage(payload);
}

std::vector<uint8_t> GDL90Encoder::createOwnshipReport(
    const PositionData& data) {
  return createPositionReport(MSG_ID_OWNSHIP_REPORT, data);
}

std::vector<uint8_t> GDL90Encoder::createTrafficReport(
    const PositionData& data) {
  return createPositionReport(MSG_ID_TRAFFIC_REPORT, data);
}

}  // namespace gdl90
