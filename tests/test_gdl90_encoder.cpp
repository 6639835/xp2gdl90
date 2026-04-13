#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "frame_test_utils.h"
#include "xp2gdl90/gdl90_encoder.h"

namespace {

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
  const auto payload = xp2gdl90::test::ExtractPayload(message);

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
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_HEARTBEAT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x81), payload[1]);
  ASSERT_EQ(static_cast<uint8_t>(0x01), payload[2] & 0x01);
}

TEST_CASE("Heartbeat encoding sets timestamp high-bit flag") {
  gdl90::GDL90Encoder encoder([] { return 0x10000u; });
  const auto message = encoder.createHeartbeat(false, false);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_HEARTBEAT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[2] & 0x01);
  ASSERT_EQ(static_cast<uint8_t>(0x80), payload[2] & 0x80);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[3]);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[4]);
}

TEST_CASE("Heartbeat clears UTC OK when time provider reports failure") {
  gdl90::GDL90Encoder encoder([](uint32_t* out_time) {
    if (out_time) {
      *out_time = 0;
    }
    return false;
  });
  const auto message = encoder.createHeartbeat(true, true);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_HEARTBEAT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[2] & 0x01);
}

TEST_CASE("Heartbeat handles missing UTC provider and null checked callback output") {
  gdl90::GDL90Encoder missing_provider(gdl90::UtcTimeProvider{});
  const auto missing_provider_msg = missing_provider.createHeartbeat(true, true);
  const auto missing_payload = xp2gdl90::test::ExtractPayload(missing_provider_msg);
  ASSERT_EQ(static_cast<uint8_t>(0x00), missing_payload[2] & 0x01);

  gdl90::CheckedUtcTimeProvider checked_provider = [](uint32_t* out_time) {
    return out_time != nullptr;
  };
  ASSERT_TRUE(!checked_provider(nullptr));
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
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_OWNSHIP_REPORT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(0x20), payload[1]);

  ASSERT_EQ(EncodeLat(100.0), xp2gdl90::test::Decode24(payload, 5));
  ASSERT_EQ(EncodeLon(-200.0), xp2gdl90::test::Decode24(payload, 8));

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

TEST_CASE("Ownship report encodes invalid altitude sentinel") {
  gdl90::GDL90Encoder encoder;
  gdl90::PositionData data{};
  data.latitude = 0.0;
  data.longitude = 0.0;
  data.altitude = std::numeric_limits<int32_t>::min();
  data.h_velocity = 0;
  data.v_velocity = 0;
  data.track = 0;
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = true;
  data.nic = 11;
  data.nacp = 11;
  data.icao_address = 1;
  data.callsign = "TEST";
  data.emitter_category = gdl90::EmitterCategory::LIGHT;
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.alert_status = 0;
  data.emergency_code = 0;

  const auto message = encoder.createOwnshipReport(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(0xFF), payload[11]);
  ASSERT_EQ(static_cast<uint8_t>(0xF9), payload[12]);
}

TEST_CASE("Ownship report encodes invalid horizontal velocity sentinel") {
  gdl90::GDL90Encoder encoder;
  gdl90::PositionData data{};
  data.latitude = 0.0;
  data.longitude = 0.0;
  data.altitude = 0;
  data.h_velocity = gdl90::VELOCITY_INVALID;
  data.v_velocity = 0;
  data.track = 0;
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = true;
  data.nic = 11;
  data.nacp = 11;
  data.icao_address = 1;
  data.callsign = "TEST";
  data.emitter_category = gdl90::EmitterCategory::LIGHT;
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.alert_status = 0;
  data.emergency_code = 0;

  const auto message = encoder.createOwnshipReport(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint8_t>(0xFF), payload[14]);
  ASSERT_EQ(static_cast<uint8_t>(0xF0), payload[15]);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[16]);
}

TEST_CASE("Ownship geometric altitude encodes altitude and vertical metrics") {
  gdl90::GDL90Encoder encoder;
  gdl90::GeoAltitudeData data{};
  data.altitude_feet = 1000;
  data.vertical_warning = true;
  data.vfom_meters = 50;

  const auto message = encoder.createOwnshipGeometricAltitude(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<size_t>(5), payload.size());
  ASSERT_EQ(static_cast<uint8_t>(gdl90::MSG_ID_OWNSHIP_GEO_ALTITUDE), payload[0]);
  ASSERT_EQ(static_cast<uint16_t>(0x00C8), xp2gdl90::test::Decode16BE(payload, 1));
  ASSERT_EQ(static_cast<uint16_t>(0x8032), xp2gdl90::test::Decode16BE(payload, 3));
}

TEST_CASE("Ownship geometric altitude clamps VFOM and altitude bounds") {
  gdl90::GDL90Encoder encoder;
  gdl90::GeoAltitudeData data{};
  data.altitude_feet = std::numeric_limits<int32_t>::max();
  data.vertical_warning = false;
  data.vfom_meters = 0xFFFFu;

  const auto message = encoder.createOwnshipGeometricAltitude(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint16_t>(0x7FFF), xp2gdl90::test::Decode16BE(payload, 1));
  ASSERT_EQ(static_cast<uint16_t>(gdl90::GEO_ALTITUDE_VFOM_EXCESSIVE),
            xp2gdl90::test::Decode16BE(payload, 3));
}

TEST_CASE("Ownship geometric altitude clamps low altitude and preserves invalid VFOM") {
  gdl90::GDL90Encoder encoder;
  gdl90::GeoAltitudeData data{};
  data.altitude_feet = std::numeric_limits<int32_t>::min();
  data.vertical_warning = false;
  data.vfom_meters = gdl90::GEO_ALTITUDE_VFOM_INVALID;

  const auto message = encoder.createOwnshipGeometricAltitude(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint16_t>(0x8000), xp2gdl90::test::Decode16BE(payload, 1));
  ASSERT_EQ(static_cast<uint16_t>(gdl90::GEO_ALTITUDE_VFOM_INVALID),
            xp2gdl90::test::Decode16BE(payload, 3));
}

TEST_CASE("Ownship report clamps vertical velocity bounds") {
  gdl90::GDL90Encoder encoder;
  gdl90::PositionData data{};
  data.latitude = 0.0;
  data.longitude = 0.0;
  data.altitude = 0;
  data.h_velocity = 0;
  data.v_velocity = std::numeric_limits<int16_t>::max();
  data.track = 0;
  data.track_type = gdl90::TrackType::TRUE_TRACK;
  data.airborne = true;
  data.nic = 0;
  data.nacp = 0;
  data.icao_address = 0x000001;
  data.callsign = "TEST";
  data.emitter_category = gdl90::EmitterCategory::LIGHT;
  data.address_type = gdl90::AddressType::ADSB_ICAO;
  data.alert_status = 0;
  data.emergency_code = 0;

  {
    const auto message = encoder.createOwnshipReport(data);
    const auto payload = xp2gdl90::test::ExtractPayload(message);
    ASSERT_EQ(static_cast<uint8_t>(0x01), payload[15] & 0x0F);
    ASSERT_EQ(static_cast<uint8_t>(0xFE), payload[16]);
  }

  data.v_velocity = static_cast<int16_t>(-32600);
  {
    const auto message = encoder.createTrafficReport(data);
    const auto payload = xp2gdl90::test::ExtractPayload(message);
    ASSERT_EQ(static_cast<uint8_t>(0x0E), payload[15] & 0x0F);
    ASSERT_EQ(static_cast<uint8_t>(0x02), payload[16]);
  }
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
  const auto payload = xp2gdl90::test::ExtractPayload(message);

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
