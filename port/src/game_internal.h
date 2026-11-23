// Names the parts of Game share (game.cpp, game_plane.cpp, game_players.cpp, game_reset.cpp,
// game_state.cpp). Only for those files.
#pragma once
#include <cstdint>

#include "game.h"

namespace rr {
namespace {

inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }

// the plane's states
constexpr uint8_t kFlying = Plane::kFlying, kWaiting = Plane::kWaiting, kLifeLost = Plane::kLifeLost;
constexpr uint8_t kCrashed = Plane::kCrashed, kNoPlane = Plane::kNoPlane;

// channel 1's event sounds
constexpr uint8_t kCrashSound = Sounds::kCrash, kOutOfFuelSound = Sounds::kOutOfFuel, kTankFullSound = Sounds::kTankFull, kRefuelSound = Sounds::kRefuel;

}  // namespace
}  // namespace rr
