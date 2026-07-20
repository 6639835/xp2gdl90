#include "test_harness.h"

#include <limits>

#include "xp2gdl90/broadcast_clock.h"

TEST_CASE("Broadcast clock uses simulator time during normal flight") {
  xp2gdl90::BroadcastClockState state;
  const auto result =
      xp2gdl90::UpdateBroadcastClock(125.0f, 900.0f, false, &state);
  ASSERT_EQ(125.0f, result.time);
  ASSERT_TRUE(!result.replay_active);
  ASSERT_TRUE(!result.schedule_reset_required);
}

TEST_CASE("Broadcast clock uses monotonic elapsed time during replay") {
  xp2gdl90::BroadcastClockState state;
  xp2gdl90::UpdateBroadcastClock(125.0f, 900.0f, false, &state);
  const auto replay =
      xp2gdl90::UpdateBroadcastClock(30.0f, 901.0f, true, &state);
  ASSERT_EQ(901.0f, replay.time);
  ASSERT_TRUE(replay.replay_active);
  ASSERT_TRUE(replay.schedule_reset_required);

  const auto rewound =
      xp2gdl90::UpdateBroadcastClock(5.0f, 902.0f, true, &state);
  ASSERT_EQ(902.0f, rewound.time);
  ASSERT_TRUE(!rewound.schedule_reset_required);
}

TEST_CASE("Broadcast clock resets schedule on replay exit or time reversal") {
  xp2gdl90::BroadcastClockState state;
  xp2gdl90::UpdateBroadcastClock(100.0f, 500.0f, true, &state);
  const auto exited =
      xp2gdl90::UpdateBroadcastClock(80.0f, 501.0f, false, &state);
  ASSERT_TRUE(exited.schedule_reset_required);

  const auto advanced =
      xp2gdl90::UpdateBroadcastClock(90.0f, 502.0f, false, &state);
  ASSERT_TRUE(!advanced.schedule_reset_required);
  const auto reversed =
      xp2gdl90::UpdateBroadcastClock(10.0f, 503.0f, false, &state);
  ASSERT_TRUE(reversed.schedule_reset_required);
}

TEST_CASE("Broadcast clock falls back when simulator time is invalid") {
  xp2gdl90::BroadcastClockState state;
  const auto result = xp2gdl90::UpdateBroadcastClock(
      std::numeric_limits<float>::quiet_NaN(), 42.0f, false, &state);
  ASSERT_EQ(42.0f, result.time);
}
