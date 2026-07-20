#include "xp2gdl90/broadcast_clock.h"

#include <cmath>

namespace xp2gdl90 {

BroadcastClockResult UpdateBroadcastClock(float simulator_time,
                                          float elapsed_time,
                                          bool replay_active,
                                          BroadcastClockState *state) {
  const bool simulator_time_valid =
      std::isfinite(static_cast<double>(simulator_time)) &&
      simulator_time >= 0.0f;
  const bool elapsed_time_valid =
      std::isfinite(static_cast<double>(elapsed_time)) && elapsed_time >= 0.0f;

  float resolved_time = 0.0f;
  if ((replay_active || !simulator_time_valid) && elapsed_time_valid) {
    resolved_time = elapsed_time;
  } else if (simulator_time_valid) {
    resolved_time = simulator_time;
  } else if (elapsed_time_valid) {
    resolved_time = elapsed_time;
  }

  BroadcastClockResult result;
  result.time = resolved_time;
  result.replay_active = replay_active;
  if (!state) {
    return result;
  }

  result.schedule_reset_required =
      state->initialized && (state->replay_active != replay_active ||
                             resolved_time < state->last_time);
  state->initialized = true;
  state->replay_active = replay_active;
  state->last_time = resolved_time;
  return result;
}

} // namespace xp2gdl90
