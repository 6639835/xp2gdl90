#include <limits>
#include <string>

#include "frame_test_utils.h"
#include "xp2gdl90/foreflight_encoder.h"

TEST_CASE("ForeFlight ID message encodes metadata in big-endian order") {
  gdl90::foreflight::ForeFlightEncoder encoder;
  gdl90::foreflight::DeviceInfo info{};
  info.serial_number = 0x0123456789ABCDEFull;
  info.device_name = "XP2GDL90";
  info.device_long_name = "XP2GDL90 AHRS";
  info.capabilities_mask = 0x00000005u;

  const auto message = encoder.createIdMessage(info);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<size_t>(39), payload.size());
  ASSERT_EQ(static_cast<uint8_t>(gdl90::foreflight::MSG_ID_FORE_FLIGHT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(gdl90::foreflight::SUB_ID_DEVICE_INFO), payload[1]);
  ASSERT_EQ(static_cast<uint8_t>(0x01), payload[2]);
  ASSERT_EQ(static_cast<uint64_t>(0x0123456789ABCDEFull),
            xp2gdl90::test::Decode64BE(payload, 3));

  std::string device_name(payload.begin() + 11, payload.begin() + 19);
  ASSERT_EQ(std::string("XP2GDL90"), device_name);

  std::string device_long_name(payload.begin() + 19, payload.begin() + 32);
  ASSERT_EQ(std::string("XP2GDL90 AHRS"), device_long_name);
  ASSERT_EQ(static_cast<uint8_t>(0x00), payload[32]);
  ASSERT_EQ(static_cast<uint32_t>(0x00000005u),
            xp2gdl90::test::Decode32BE(payload, 35));
}

TEST_CASE("ForeFlight AHRS message encodes attitude and heading fields") {
  gdl90::foreflight::ForeFlightEncoder encoder;
  gdl90::foreflight::AhrsData data{};
  data.roll_deg = 12.3;
  data.pitch_deg = -4.5;
  data.heading_deg = 271.2;

  const auto message = encoder.createAhrsMessage(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<size_t>(12), payload.size());
  ASSERT_EQ(static_cast<uint8_t>(gdl90::foreflight::MSG_ID_FORE_FLIGHT), payload[0]);
  ASSERT_EQ(static_cast<uint8_t>(gdl90::foreflight::SUB_ID_AHRS), payload[1]);
  ASSERT_EQ(static_cast<uint16_t>(123), xp2gdl90::test::Decode16BE(payload, 2));
  ASSERT_EQ(static_cast<uint16_t>(static_cast<int16_t>(-45)),
            xp2gdl90::test::Decode16BE(payload, 4));
  ASSERT_EQ(static_cast<uint16_t>(2712), xp2gdl90::test::Decode16BE(payload, 6));
  ASSERT_EQ(static_cast<uint16_t>(gdl90::foreflight::AHRS_AIRSPEED_INVALID),
            xp2gdl90::test::Decode16BE(payload, 8));
  ASSERT_EQ(static_cast<uint16_t>(gdl90::foreflight::AHRS_AIRSPEED_INVALID),
            xp2gdl90::test::Decode16BE(payload, 10));
}

TEST_CASE("ForeFlight AHRS message invalidates out-of-range attitude") {
  gdl90::foreflight::ForeFlightEncoder encoder;
  gdl90::foreflight::AhrsData data{};
  data.roll_deg = 181.0;
  data.pitch_deg = std::numeric_limits<double>::quiet_NaN();
  data.heading_deg = 360.0;
  data.magnetic_heading = true;
  data.indicated_airspeed = 120;
  data.true_airspeed = 135;

  const auto message = encoder.createAhrsMessage(data);
  const auto payload = xp2gdl90::test::ExtractPayload(message);

  ASSERT_EQ(static_cast<uint16_t>(gdl90::foreflight::AHRS_ATTITUDE_INVALID),
            xp2gdl90::test::Decode16BE(payload, 2));
  ASSERT_EQ(static_cast<uint16_t>(gdl90::foreflight::AHRS_ATTITUDE_INVALID),
            xp2gdl90::test::Decode16BE(payload, 4));
  ASSERT_EQ(static_cast<uint16_t>(0x8000), xp2gdl90::test::Decode16BE(payload, 6));
  ASSERT_EQ(static_cast<uint16_t>(120), xp2gdl90::test::Decode16BE(payload, 8));
  ASSERT_EQ(static_cast<uint16_t>(135), xp2gdl90::test::Decode16BE(payload, 10));
}

TEST_CASE("ForeFlight AHRS heading normalizes wraparound and invalid heading") {
  gdl90::foreflight::ForeFlightEncoder encoder;

  gdl90::foreflight::AhrsData wrapped{};
  wrapped.roll_deg = 0.0;
  wrapped.pitch_deg = 0.0;
  wrapped.heading_deg = -90.0;
  const auto wrapped_message = encoder.createAhrsMessage(wrapped);
  const auto wrapped_payload = xp2gdl90::test::ExtractPayload(wrapped_message);
  ASSERT_EQ(static_cast<uint16_t>(2700), xp2gdl90::test::Decode16BE(wrapped_payload, 6));

  gdl90::foreflight::AhrsData rounded{};
  rounded.roll_deg = 0.0;
  rounded.pitch_deg = 0.0;
  rounded.heading_deg = 359.95;
  const auto rounded_message = encoder.createAhrsMessage(rounded);
  const auto rounded_payload = xp2gdl90::test::ExtractPayload(rounded_message);
  ASSERT_EQ(static_cast<uint16_t>(0), xp2gdl90::test::Decode16BE(rounded_payload, 6));

  gdl90::foreflight::AhrsData invalid{};
  invalid.roll_deg = 0.0;
  invalid.pitch_deg = 0.0;
  invalid.heading_deg = std::numeric_limits<double>::quiet_NaN();
  const auto invalid_message = encoder.createAhrsMessage(invalid);
  const auto invalid_payload = xp2gdl90::test::ExtractPayload(invalid_message);
  ASSERT_EQ(static_cast<uint16_t>(gdl90::foreflight::AHRS_HEADING_INVALID),
            xp2gdl90::test::Decode16BE(invalid_payload, 6));
}
