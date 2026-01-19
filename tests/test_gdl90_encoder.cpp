#include "test_harness.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "xp2gdl90/gdl90_encoder.h"

namespace {

const uint16_t kCrcTable[256] = {
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

uint16_t ComputeCrc(const std::vector<uint8_t>& data) {
  uint16_t crc = 0;
  for (uint8_t byte : data) {
    crc = kCrcTable[crc >> 8] ^ static_cast<uint16_t>(crc << 8) ^ byte;
  }
  return crc;
}

std::vector<uint8_t> UnescapeFrame(const std::vector<uint8_t>& message) {
  ASSERT_TRUE(message.size() >= 3);
  ASSERT_EQ(static_cast<uint8_t>(0x7E), message.front());
  ASSERT_EQ(static_cast<uint8_t>(0x7E), message.back());

  std::vector<uint8_t> unescaped;
  for (size_t i = 1; i + 1 < message.size(); ++i) {
    const uint8_t byte = message[i];
    if (byte == 0x7D) {
      ASSERT_TRUE(i + 1 < message.size() - 1);
      const uint8_t next = message[++i];
      unescaped.push_back(static_cast<uint8_t>(next ^ 0x20));
    } else {
      unescaped.push_back(byte);
    }
  }
  return unescaped;
}

std::vector<uint8_t> ExtractPayload(const std::vector<uint8_t>& message) {
  const std::vector<uint8_t> unescaped = UnescapeFrame(message);
  ASSERT_TRUE(unescaped.size() >= 3);
  const size_t payload_len = unescaped.size() - 2;
  std::vector<uint8_t> payload(unescaped.begin(), unescaped.begin() + payload_len);

  const uint16_t crc = static_cast<uint16_t>(unescaped[payload_len]) |
                       static_cast<uint16_t>(unescaped[payload_len + 1] << 8);
  ASSERT_EQ(ComputeCrc(payload), crc);
  return payload;
}

uint32_t Decode24(const std::vector<uint8_t>& payload, size_t offset) {
  return (static_cast<uint32_t>(payload[offset]) << 16) |
         (static_cast<uint32_t>(payload[offset + 1]) << 8) |
         static_cast<uint32_t>(payload[offset + 2]);
}

uint32_t EncodeLat(double latitude) {
  const double clamped = std::max(-90.0, std::min(90.0, latitude));
  int32_t value = static_cast<int32_t>(clamped * (0x800000 / 180.0));
  if (value < 0) {
    value = (0x1000000 + value) & 0xFFFFFF;
  }
  return static_cast<uint32_t>(value);
}

uint32_t EncodeLon(double longitude) {
  const double clamped = std::max(-180.0, std::min(180.0, longitude));
  int32_t value = static_cast<int32_t>(clamped * (0x800000 / 180.0));
  if (value < 0) {
    value = (0x1000000 + value) & 0xFFFFFF;
  }
  return static_cast<uint32_t>(value);
}

uint16_t EncodeAltitude(int32_t altitude) {
  int32_t encoded = (altitude + 1000) / 25;
  if (encoded < 0) {
    encoded = 0;
  }
  if (encoded > 0xFFE) {
    encoded = 0xFFE;
  }
  return static_cast<uint16_t>(encoded);
}

uint16_t EncodeVerticalVelocity(int16_t vv_fpm) {
  if (vv_fpm == INT16_MIN) {
    return gdl90::VVELOCITY_INVALID;
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

uint8_t EncodeTrack(uint16_t track) {
  return static_cast<uint8_t>((track % 360) * 256 / 360);
}

}  // namespace

TEST_CASE("Heartbeat encoding sets flags and CRC") {
  gdl90::GDL90Encoder encoder;
  const auto message = encoder.createHeartbeat(false, false);
  const auto payload = ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_HEARTBEAT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x01), payload[1]);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[2] & 0x01);

  const uint32_t timestamp = static_cast<uint32_t>(payload[3]) |
                             (static_cast<uint32_t>(payload[4]) << 8) |
                             ((payload[2] & 0x80) ? 0x10000 : 0x0);
  ASSERT_TRUE(timestamp <= 86399u);
}

TEST_CASE("Heartbeat encoding sets gps and utc bits") {
  gdl90::GDL90Encoder encoder;
  const auto message = encoder.createHeartbeat(true, true);
  const auto payload = ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_HEARTBEAT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x81), payload[1]);
  ASSERT_EQ(static_cast<uint8_t>(0x01), payload[2] & 0x01);
}

TEST_CASE("Ownship report encodes fields and clamps values") {
  gdl90::GDL90Encoder encoder;
  gdl90::PositionData data{};
  data.latitude = 100.0;
  data.longitude = -200.0;
  data.altitude = 1000000;
  data.h_velocity = 0xFFFF;
  data.v_velocity = INT16_MIN;
  data.track = 721;
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = true;
  data.nic = 9;
  data.nacp = 8;
  data.icao_address = 0x00ABC1;
  data.callsign = "CALLSIGN9";
  data.emitter_category = gdl90::EmitterCategory::HEAVY;
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.alert_status = 2;
  data.emergency_code = 3;

  const auto message = encoder.createOwnshipReport(data);
  const auto payload = ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_OWNSHIP_REPORT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x20), payload[1]);

  ASSERT_EQ(EncodeLat(100.0), Decode24(payload, 5));
  ASSERT_EQ(EncodeLon(-200.0), Decode24(payload, 8));

  const uint16_t altitude = EncodeAltitude(1000000);
  ASSERT_EQ(static_cast<uint8_t>((altitude >> 4) & 0xFF), payload[11]);
  ASSERT_EQ(static_cast<uint8_t>(((altitude & 0x0F) << 4) | (1 << 3) | 1),
            payload[12]);

  ASSERT_EQ(static_cast<uint8_t>((data.nic << 4) | data.nacp), payload[13]);

  const uint16_t h_vel = 0xFFE;
  const uint16_t v_vel = EncodeVerticalVelocity(INT16_MIN);
  ASSERT_EQ(static_cast<uint8_t>((h_vel >> 4) & 0xFF), payload[14]);
  ASSERT_EQ(static_cast<uint8_t>(((h_vel & 0x0F) << 4) | ((v_vel >> 8) & 0x0F)),
            payload[15]);
  ASSERT_EQ(static_cast<uint8_t>(v_vel & 0xFF), payload[16]);

  ASSERT_EQ(EncodeTrack(721), payload[17]);
  ASSERT_EQ(static_cast<uint8_t>(gdl90::EmitterCategory::HEAVY), payload[18]);

  std::string callsign(payload.begin() + 19, payload.begin() + 27);
  ASSERT_EQ(std::string("CALLSIGN"), callsign);

  ASSERT_EQ(static_cast<uint8_t>(data.emergency_code << 4), payload[27]);
}

TEST_CASE("Traffic report escapes special bytes") {
  gdl90::GDL90Encoder encoder;
  gdl90::PositionData data{};
  data.latitude = -45.0;
  data.longitude = 120.0;
  data.altitude = -5000;
  data.h_velocity = 100;
  data.v_velocity = -32000;
  data.track = 180;
  data.track_type = gdl90::TrackType::MAG_HEADING;
  data.airborne = false;
  data.nic = 1;
  data.nacp = 2;
  data.icao_address = 0x00BEEF;
  data.callsign = std::string("AB") + '\x7E' + '\x7D' + "CD";
  data.emitter_category = gdl90::EmitterCategory::LIGHT;
  data.address_type = gdl90::AddressType::TISB_ICAO;
  data.alert_status = 0;
  data.emergency_code = 0;

  const auto message = encoder.createTrafficReport(data);
  const auto payload = ExtractPayload(message);

  bool found_escape = false;
  for (size_t i = 0; i + 1 < message.size(); ++i) {
    if (message[i] == 0x7D) {
      found_escape = true;
      break;
    }
  }
  ASSERT_TRUE(found_escape);

  std::string callsign(payload.begin() + 19, payload.begin() + 27);
  ASSERT_TRUE(callsign.find('\x7E') != std::string::npos);
  ASSERT_TRUE(callsign.find('\x7D') != std::string::npos);

  const uint16_t altitude = EncodeAltitude(-5000);
  ASSERT_EQ(static_cast<uint8_t>((altitude >> 4) & 0xFF), payload[11]);
  ASSERT_EQ(EncodeTrack(180), payload[17]);
}
