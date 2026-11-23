// Names the parts of Game share (game.cpp, game_plane.cpp, game_players.cpp, game_reset.cpp,
// game_state.cpp; each has `using namespace game_detail`). Only for those files.
#pragma once
#include <cstdint>

#include "game.h"

namespace rr::game_detail {

inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }

// the plane's states
inline constexpr uint8_t kFlying = Plane::kFlying, kWaiting = Plane::kWaiting, kLifeLost = Plane::kLifeLost;
inline constexpr uint8_t kCrashed = Plane::kCrashed, kNoPlane = Plane::kNoPlane;

// channel 1's event sounds
inline constexpr uint8_t kCrashSound = Sounds::kCrash, kOutOfFuelSound = Sounds::kOutOfFuel;
inline constexpr uint8_t kTankFullSound = Sounds::kTankFull, kRefuelSound = Sounds::kRefuel;

}  // namespace rr::game_detail
