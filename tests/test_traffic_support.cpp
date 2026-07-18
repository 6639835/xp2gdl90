#include "test_harness.h"

#include <limits>

#include "xp2gdl90/traffic_support.h"

TEST_CASE("TCAS presence rejects empty slots and accepts identity or motion") {
  xp2gdl90::traffic::TcasPresenceSample sample;
  sample.slot = 1;
  sample.ssr_mode = -1;
  ASSERT_TRUE(!xp2gdl90::traffic::IsPopulatedTcasTarget(sample));

  sample.callsign = "AI01";
  ASSERT_TRUE(xp2gdl90::traffic::IsPopulatedTcasTarget(sample));

  sample.callsign.clear();
  sample.velocity_x = 1.0;
  ASSERT_TRUE(xp2gdl90::traffic::IsPopulatedTcasTarget(sample));
}

TEST_CASE("TCAS presence rejects invalid coordinates") {
  xp2gdl90::traffic::TcasPresenceSample sample;
  sample.slot = 2;
  sample.raw_address = 0xABCDEF;
  sample.local_x = std::numeric_limits<double>::quiet_NaN();
  ASSERT_TRUE(!xp2gdl90::traffic::IsPopulatedTcasTarget(sample));

  // X-Plane uses -FLT_MAX for every position component in an unused slot.
  sample.local_x = -static_cast<double>(std::numeric_limits<float>::max());
  sample.local_y = sample.local_x;
  sample.local_z = sample.local_x;
  ASSERT_TRUE(!xp2gdl90::traffic::IsPopulatedTcasTarget(sample));

  sample.raw_address = 0;
  sample.local_x = 0.0;
  sample.local_y = 0.0;
  sample.local_z = 0.0;
  sample.velocity_x = -static_cast<double>(std::numeric_limits<float>::max());
  ASSERT_TRUE(!xp2gdl90::traffic::IsPopulatedTcasTarget(sample));
}

TEST_CASE("Synthetic traffic addresses are stable and avoid ownship") {
  const uint32_t first =
      xp2gdl90::traffic::SyntheticTrafficAddress(3, "AAL123", 0xABCDEF);
  ASSERT_EQ(first, xp2gdl90::traffic::SyntheticTrafficAddress(4, "AAL123", 0));
  ASSERT_TRUE(first !=
              xp2gdl90::traffic::SyntheticTrafficAddress(3, "UAL456", 0));

  const uint32_t slot_only =
      xp2gdl90::traffic::SyntheticTrafficAddress(3, "", 0);
  ASSERT_TRUE(slot_only !=
              xp2gdl90::traffic::SyntheticTrafficAddress(4, "", 0));

  const uint32_t avoided =
      xp2gdl90::traffic::SyntheticTrafficAddress(3, "AAL123", first);
  ASSERT_TRUE(avoided != first);
}

TEST_CASE("Pressure correction preserves traffic relative altitude") {
  ASSERT_EQ(4500, xp2gdl90::traffic::CorrectGeometricToPressureAltitude(
                      5000, 1000.0, 500.0));
  ASSERT_EQ(5000, xp2gdl90::traffic::CorrectGeometricToPressureAltitude(
                      5000, std::numeric_limits<double>::quiet_NaN(), 500.0));
  ASSERT_EQ(std::numeric_limits<int32_t>::min(),
            xp2gdl90::traffic::CorrectGeometricToPressureAltitude(
                std::numeric_limits<int32_t>::min(), 1000.0, 500.0));
}
