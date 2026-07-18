#ifndef XP2GDL90_BROADCAST_CLOCK_H
#define XP2GDL90_BROADCAST_CLOCK_H

namespace xp2gdl90 {

struct BroadcastClockState {
  bool initialized = false;
  bool replay_active = false;
  float last_time = 0.0f;
};

struct BroadcastClockResult {
  float time = 0.0f;
  bool replay_active = false;
  bool schedule_reset_required = false;
};

BroadcastClockResult UpdateBroadcastClock(float simulator_time,
                                          float elapsed_time,
                                          bool replay_active,
                                          BroadcastClockState *state);

} // namespace xp2gdl90

#endif // XP2GDL90_BROADCAST_CLOCK_H
