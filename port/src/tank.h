// The tank on the bank road, the shell it fires across the river, and the missile of the
// helicopter that fires (one at a time). They scroll with the river, so the river keeps them
// (RiverState); the game moves them (Game::driveTank, moveShell, moveMissile).
#pragma once
#include <cstdint>

namespace rr {

struct Tank {
	uint8_t kind = 0;    // $38 0 none; 5/6 facing right/left (animated); bit 7: destroyed
	uint8_t x = 0;       // $39
	uint8_t y = 0;       // $3A
	uint8_t fires = 0;   // $41 bit 7: drives to the river and fires
	bool facingRight() const { return kind < 6; }
};

struct Shell {
	// timer ($50): bit 7 no shell; 0 launch; 1-$10 flight; $11-$38 explosion
	uint8_t timer = 0;
	uint8_t y = 0;          // $40
	uint8_t x = 0;          // $3C
	uint8_t xFrac = 0;      // $3D
	uint8_t speed = 0;      // $3E sideways speed per frame, whole part
	uint8_t speedFrac = 0;  // $3F and fraction (/256)
	uint8_t frame = 0;      // $3B shape: 0 none, 1 flying, 2.. explosion
	uint8_t colour = 0;     // COLPM0, set at the top of the frame (AED3)
	bool none() const { return (timer & 0x80) != 0; }
	bool exploding() const { return timer >= 0x11; }   // (also with no shell: timer $FF)
};

struct Missile {
	uint8_t x = 0;      // $C1 0: no missile
	uint8_t y = 0;      // $C2
	uint8_t step = 0;   // $C0 +2 or -2 per frame
};

}  // namespace rr
