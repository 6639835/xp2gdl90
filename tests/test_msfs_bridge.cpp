#include "test_harness.h"

#include <cmath>
#include <limits>

#include "xp2gdl90/foreflight_encoder.h"
#include "xp2gdl90/gdl90_encoder.h"
#include "xp2gdl90/msfs_bridge.h"
#include "xp2gdl90/settings.h"

using namespace msfs_bridge;

// ---------------------------------------------------------------------------
// NormalizeDegrees360
// ---------------------------------------------------------------------------

TEST_CASE("NormalizeDegrees360 zero") {
  ASSERT_EQ(0.0, NormalizeDegrees360(0.0));
}

TEST_CASE("NormalizeDegrees360 positive in range") {
  ASSERT_EQ(90.0, NormalizeDegrees360(90.0));
  ASSERT_EQ(180.0, NormalizeDegrees360(180.0));
  ASSERT_EQ(359.0, NormalizeDegrees360(359.0));
}

TEST_CASE("NormalizeDegrees360 exactly 360 wraps to 0") {
  ASSERT_EQ(0.0, NormalizeDegrees360(360.0));
}

TEST_CASE("NormalizeDegrees360 over 360 wraps") {
  ASSERT_EQ(1.0, NormalizeDegrees360(361.0));
  ASSERT_EQ(0.0, NormalizeDegrees360(720.0));
}

TEST_CASE("NormalizeDegrees360 negative wraps positive") {
  ASSERT_EQ(270.0, NormalizeDegrees360(-90.0));
  ASSERT_EQ(180.0, NormalizeDegrees360(-180.0));
}

TEST_CASE("NormalizeDegrees360 NaN propagates") {
  ASSERT_TRUE(std::isnan(
      NormalizeDegrees360(std::numeric_limits<double>::quiet_NaN())));
}

TEST_CASE("NormalizeDegrees360 infinity propagates") {
  ASSERT_TRUE(
      std::isnan(NormalizeDegrees360(std::numeric_limits<double>::infinity())));
}

// ---------------------------------------------------------------------------
// NormalizeDegreesToUint16
// ---------------------------------------------------------------------------

TEST_CASE("NormalizeDegreesToUint16 basic") {
  ASSERT_EQ(static_cast<uint16_t>(0), NormalizeDegreesToUint16(0.0));
  ASSERT_EQ(static_cast<uint16_t>(90), NormalizeDegreesToUint16(90.0));
  ASSERT_EQ(static_cast<uint16_t>(180), NormalizeDegreesToUint16(180.0));
  ASSERT_EQ(static_cast<uint16_t>(359), NormalizeDegreesToUint16(359.0));
}

TEST_CASE("NormalizeDegreesToUint16 wraps") {
  ASSERT_EQ(static_cast<uint16_t>(0), NormalizeDegreesToUint16(360.0));
  ASSERT_EQ(static_cast<uint16_t>(270), NormalizeDegreesToUint16(-90.0));
}

TEST_CASE("NormalizeDegreesToUint16 NaN returns 0") {
  ASSERT_EQ(static_cast<uint16_t>(0),
            NormalizeDegreesToUint16(std::numeric_limits<double>::quiet_NaN()));
}

// ---------------------------------------------------------------------------
// ClampKnotsToUint16OrInvalid
// ---------------------------------------------------------------------------

TEST_CASE("ClampKnotsToUint16OrInvalid normal value") {
  ASSERT_EQ(static_cast<uint16_t>(250), ClampKnotsToUint16OrInvalid(250.0));
}

TEST_CASE("ClampKnotsToUint16OrInvalid negative is invalid") {
  ASSERT_EQ(gdl90::foreflight::AHRS_AIRSPEED_INVALID,
            ClampKnotsToUint16OrInvalid(-1.0));
}

TEST_CASE("ClampKnotsToUint16OrInvalid NaN is invalid") {
  ASSERT_EQ(
      gdl90::foreflight::AHRS_AIRSPEED_INVALID,
      ClampKnotsToUint16OrInvalid(std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("ClampKnotsToUint16OrInvalid zero is valid") {
  ASSERT_EQ(static_cast<uint16_t>(0), ClampKnotsToUint16OrInvalid(0.0));
}

TEST_CASE("ClampKnotsToUint16OrInvalid clamps below max valid") {
  const auto result = ClampKnotsToUint16OrInvalid(1e9);
  ASSERT_NE(gdl90::foreflight::AHRS_AIRSPEED_INVALID, result);
  ASSERT_EQ(
      static_cast<uint16_t>(gdl90::foreflight::AHRS_AIRSPEED_INVALID - 1u),
      result);
}

// ---------------------------------------------------------------------------
// ClampFpmToInt16OrInvalid
// ---------------------------------------------------------------------------

TEST_CASE("ClampFpmToInt16OrInvalid normal value") {
  ASSERT_EQ(static_cast<int16_t>(500), ClampFpmToInt16OrInvalid(500.0));
  ASSERT_EQ(static_cast<int16_t>(-500), ClampFpmToInt16OrInvalid(-500.0));
}

TEST_CASE("ClampFpmToInt16OrInvalid NaN returns min") {
  ASSERT_EQ(std::numeric_limits<int16_t>::min(),
            ClampFpmToInt16OrInvalid(std::numeric_limits<double>::quiet_NaN()));
}

TEST_CASE("ClampFpmToInt16OrInvalid clamps to int16 range") {
  ASSERT_EQ(std::numeric_limits<int16_t>::max(), ClampFpmToInt16OrInvalid(1e9));
  // min+1 because min is the sentinel
  ASSERT_EQ(static_cast<int16_t>(std::numeric_limits<int16_t>::min() + 1),
            ClampFpmToInt16OrInvalid(-1e9));
}

// ---------------------------------------------------------------------------
// TrimString
// ---------------------------------------------------------------------------

TEST_CASE("TrimString empty") { ASSERT_EQ(std::string(""), TrimString("")); }

TEST_CASE("TrimString no whitespace") {
  ASSERT_EQ(std::string("N12345"), TrimString("N12345"));
}

TEST_CASE("TrimString leading and trailing spaces") {
  ASSERT_EQ(std::string("N12345"), TrimString("  N12345  "));
}

TEST_CASE("TrimString tabs and newlines") {
  ASSERT_EQ(std::string("ABC"), TrimString("\t\nABC\r\n"));
}

TEST_CASE("TrimString only whitespace") {
  ASSERT_EQ(std::string(""), TrimString("   "));
}

// ---------------------------------------------------------------------------
// SyntheticTrafficAddress
// ---------------------------------------------------------------------------

TEST_CASE("SyntheticTrafficAddress is in F00000 range") {
  for (uint32_t id : {1u, 2u, 100u, 0xFFFFu}) {
    const uint32_t addr = SyntheticTrafficAddress(id);
    ASSERT_EQ(0xF00000u, addr & 0xF00000u);
    ASSERT_EQ(0u, addr & 0xFF000000u);
  }
}

TEST_CASE("SyntheticTrafficAddress different ids give different addresses") {
  ASSERT_NE(SyntheticTrafficAddress(1), SyntheticTrafficAddress(2));
  ASSERT_NE(SyntheticTrafficAddress(100), SyntheticTrafficAddress(200));
}

TEST_CASE("SyntheticTrafficAddress is deterministic") {
  ASSERT_EQ(SyntheticTrafficAddress(42), SyntheticTrafficAddress(42));
}

// ---------------------------------------------------------------------------
// BuildOwnshipPosition
// ---------------------------------------------------------------------------

namespace {
xp2gdl90::Settings DefaultSettings() {
  xp2gdl90::Settings cfg;
  cfg.icao_address = 0xABCDEF;
  cfg.callsign = "N12345";
  cfg.nic = 11;
  cfg.nacp = 11;
  cfg.emitter_category = 1;
  cfg.ahrs_use_magnetic_heading = false;
  return cfg;
}

OwnshipData MakeOwnship(double lat = 37.5, double lon = -122.0,
                        double alt_ft = 2500.0, double press_alt = 2450.0) {
  OwnshipData d;
  d.latitude_deg = lat;
  d.longitude_deg = lon;
  d.altitude_ft = alt_ft;
  d.pressure_altitude_ft = press_alt;
  d.ground_velocity_kt = 120.0;
  d.vertical_speed_fps = 5.0;
  d.true_heading_deg = 180.0;
  d.magnetic_heading_deg = 177.0;
  d.pitch_deg = 2.0;
  d.bank_deg = 0.0;
  d.indicated_airspeed_kt = 115.0;
  d.true_airspeed_kt = 130.0;
  d.sim_on_ground = false;
  d.callsign = "MSTEST";
  return d;
}
} // namespace

TEST_CASE("BuildOwnshipPosition basic fields") {
  const auto cfg = DefaultSettings();
  const auto sim = MakeOwnship();
  const auto data = BuildOwnshipPosition(sim, cfg);

  ASSERT_EQ(cfg.icao_address, data.icao_address);
  ASSERT_EQ(static_cast<uint8_t>(gdl90::AddressType::ADSB_ICAO),
            static_cast<uint8_t>(data.address_type));
  ASSERT_TRUE(data.airborne);
  ASSERT_EQ(std::string("MSTEST"), data.callsign);
  ASSERT_EQ(cfg.nic, data.nic);
  ASSERT_EQ(cfg.nacp, data.nacp);
}

TEST_CASE("BuildOwnshipPosition sim on ground") {
  const auto cfg = DefaultSettings();
  auto sim = MakeOwnship();
  sim.sim_on_ground = true;
  const auto data = BuildOwnshipPosition(sim, cfg);
  ASSERT_TRUE(!data.airborne);
}

TEST_CASE("BuildOwnshipPosition invalid GPS clears NIC") {
  const auto cfg = DefaultSettings();
  auto sim = MakeOwnship(200.0, 200.0); // out of range → invalid
  const auto data = BuildOwnshipPosition(sim, cfg);
  ASSERT_EQ(static_cast<uint8_t>(0), data.nic);
}

TEST_CASE("BuildOwnshipPosition falls back to cfg callsign when empty") {
  const auto cfg = DefaultSettings();
  auto sim = MakeOwnship();
  sim.callsign = "";
  const auto data = BuildOwnshipPosition(sim, cfg);
  ASSERT_EQ(cfg.callsign, data.callsign);
}

TEST_CASE("BuildOwnshipPosition track is normalized") {
  const auto cfg = DefaultSettings();
  auto sim = MakeOwnship();
  sim.true_heading_deg = -10.0;
  const auto data = BuildOwnshipPosition(sim, cfg);
  ASSERT_EQ(static_cast<uint16_t>(350), data.track);
}

// ---------------------------------------------------------------------------
// BuildGeoAltitude
// ---------------------------------------------------------------------------

TEST_CASE("BuildGeoAltitude altitude field") {
  const auto sim = MakeOwnship(37.5, -122.0, 3000.0);
  const auto data = BuildGeoAltitude(sim);
  ASSERT_EQ(static_cast<int32_t>(3000), data.altitude_feet);
  ASSERT_TRUE(!data.vertical_warning);
  ASSERT_EQ(gdl90::GEO_ALTITUDE_VFOM_INVALID, data.vfom_meters);
}

// ---------------------------------------------------------------------------
// BuildAhrs
// ---------------------------------------------------------------------------

TEST_CASE("BuildAhrs true heading when not using magnetic") {
  const auto cfg = DefaultSettings(); // ahrs_use_magnetic_heading = false
  const auto sim = MakeOwnship();
  const auto data = BuildAhrs(sim, cfg);
  ASSERT_EQ(180.0, data.heading_deg);
  ASSERT_TRUE(!data.magnetic_heading);
}

TEST_CASE("BuildAhrs magnetic heading when configured") {
  auto cfg = DefaultSettings();
  cfg.ahrs_use_magnetic_heading = true;
  const auto sim = MakeOwnship();
  const auto data = BuildAhrs(sim, cfg);
  ASSERT_EQ(177.0, data.heading_deg);
  ASSERT_TRUE(data.magnetic_heading);
}

TEST_CASE("BuildAhrs pitch and roll") {
  const auto cfg = DefaultSettings();
  auto sim = MakeOwnship();
  sim.pitch_deg = 5.5;
  sim.bank_deg = -15.0;
  const auto data = BuildAhrs(sim, cfg);
  ASSERT_EQ(5.5, data.pitch_deg);
  ASSERT_EQ(-15.0, data.roll_deg);
}

TEST_CASE("BuildAhrs airspeed values") {
  const auto cfg = DefaultSettings();
  const auto sim = MakeOwnship();
  const auto data = BuildAhrs(sim, cfg);
  ASSERT_EQ(static_cast<uint16_t>(115), data.indicated_airspeed);
  ASSERT_EQ(static_cast<uint16_t>(130), data.true_airspeed);
}

// ---------------------------------------------------------------------------
// BuildDeviceInfo
// ---------------------------------------------------------------------------

TEST_CASE("BuildDeviceInfo name fields") {
  auto cfg = DefaultSettings();
  cfg.device_name = "MYDEV";
  cfg.device_long_name = "My Device Long";
  cfg.internet_policy = 0;
  const auto data = BuildDeviceInfo(cfg);
  ASSERT_EQ(std::string("MYDEV"), data.device_name);
  ASSERT_EQ(std::string("My Device Long"), data.device_long_name);
  ASSERT_EQ(gdl90::foreflight::DEVICE_SERIAL_INVALID, data.serial_number);
}

TEST_CASE("BuildDeviceInfo capabilities_mask encodes internet_policy") {
  auto cfg = DefaultSettings();
  cfg.internet_policy = 2;
  const auto data = BuildDeviceInfo(cfg);
  // capabilities_mask = 0x01 | (internet_policy & 0x03) << 1
  const uint32_t expected = 0x01u | (2u << 1);
  ASSERT_EQ(expected, data.capabilities_mask);
}

// ---------------------------------------------------------------------------
// BuildTrafficPosition
// ---------------------------------------------------------------------------

namespace {
TrafficData MakeTraffic(uint32_t id = 42, double lat = 37.6,
                        double lon = -122.1, double alt = 3000.0) {
  TrafficData d;
  d.object_id = id;
  d.latitude_deg = lat;
  d.longitude_deg = lon;
  d.altitude_ft = alt;
  d.ground_velocity_kt = 100.0;
  d.velocity_world_x_fps = 0.0;
  d.velocity_world_y_fps = 0.0;
  d.velocity_world_z_fps = 100.0;
  d.true_heading_deg = 0.0;
  d.sim_on_ground = false;
  d.callsign = "TFC001";
  d.icao_address = 0;
  return d;
}
} // namespace

TEST_CASE("BuildTrafficPosition basic") {
  const auto cfg = DefaultSettings();
  const auto td = MakeTraffic();
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_EQ(37.6, out.latitude);
  ASSERT_EQ(-122.1, out.longitude);
  ASSERT_EQ(static_cast<int32_t>(3000), out.altitude);
  ASSERT_EQ(std::string("TFC001"), out.callsign);
  ASSERT_TRUE(out.airborne);
}

TEST_CASE("BuildTrafficPosition invalid coords returns false") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic(1, 200.0, 200.0); // out of range → invalid
  gdl90::PositionData out;
  ASSERT_TRUE(!BuildTrafficPosition(td, cfg, &out));
}

TEST_CASE("BuildTrafficPosition null out_data returns false") {
  const auto cfg = DefaultSettings();
  const auto td = MakeTraffic();
  ASSERT_TRUE(!BuildTrafficPosition(td, cfg, nullptr));
}

TEST_CASE("BuildTrafficPosition synthetic address when icao is zero") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  td.icao_address = 0;
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_EQ(static_cast<uint8_t>(gdl90::AddressType::ADSB_SELF_ASSIGNED),
            static_cast<uint8_t>(out.address_type));
  ASSERT_EQ(SyntheticTrafficAddress(td.object_id), out.icao_address);
}

TEST_CASE("BuildTrafficPosition real ICAO when provided") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  td.icao_address = 0xA1B2C3u;
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_EQ(static_cast<uint8_t>(gdl90::AddressType::ADSB_ICAO),
            static_cast<uint8_t>(out.address_type));
  ASSERT_EQ(static_cast<uint32_t>(0xA1B2C3u), out.icao_address);
}

TEST_CASE("BuildTrafficPosition sim on ground") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  td.sim_on_ground = true;
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_TRUE(!out.airborne);
}

TEST_CASE("BuildTrafficPosition velocity vector used for track") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  // velocity pointing east: +X, zero Z → 90 degrees
  td.velocity_world_x_fps = 100.0;
  td.velocity_world_z_fps = 0.0;
  td.true_heading_deg = 0.0;
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_EQ(static_cast<uint16_t>(90), out.track);
}

TEST_CASE("BuildTrafficPosition heading used when speed near zero") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  td.velocity_world_x_fps = 0.0;
  td.velocity_world_z_fps = 0.0;
  td.true_heading_deg = 45.0;
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_EQ(static_cast<uint16_t>(45), out.track);
}

TEST_CASE("BuildTrafficPosition fallback callsign when empty") {
  const auto cfg = DefaultSettings();
  auto td = MakeTraffic();
  td.callsign = "";
  gdl90::PositionData out;
  ASSERT_TRUE(BuildTrafficPosition(td, cfg, &out));
  ASSERT_TRUE(!out.callsign.empty());
}
