// The tank on the bank road, the shell it fires across the river, and the missile of the
// helicopter that fires (one at a time). They scroll with the river, so the river keeps them
// (RiverState); the game moves them (Game::driveTank, moveShell, moveMissile).
#pragma once
#include <cstdint>

namespace rr {

class Rom;
struct Slot;
struct Shell;

struct Tank {
	uint8_t kind = 0;    // $38 0 none; 5/6 facing right/left (animated); bit 7: destroyed
	uint8_t x = 0;       // $39
	uint8_t y = 0;       // $3A
	uint8_t fires = 0;   // $41 bit 7: drives to the river and fires
	bool facingRight() const { return kind < 6; }
	// A21B-A26A: towards the river every 16 frames (every 4 while its kind is 4-7), stops at
	// the edge and fires; its kind animates. `moving`: the plane flies (or the attract mode);
	// `atEdge`: the display saw it touch the river edge; `bridgeHit`: the bridge ahead is down.
	void drive(uint8_t frameCount, Shell &shell, bool moving, bool atEdge, bool bridgeHit, const Rom &rom);
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
	// A607-A671: across the river from the tank, then the explosion (timer 0..$38: launch,
	// flight 1-$10, explosion $11-$38). `noPlane`: none flying, so the tank does not fire again.
	void move(const Tank &tank, bool noPlane, const Rom &rom);
};

struct Missile {
	uint8_t x = 0;      // $C1 0: no missile
	uint8_t y = 0;      // $C2
	uint8_t step = 0;   // $C0 +2 or -2 per frame
	// A5B7-A605: the shooting helicopter (`shooter`, nullptr if there is none) fires one the way
	// it faces; it flies until it hits the bank (`hitBank`, seen by the display). `moving`: the
	// plane flies (or the attract mode). True when one is fired (its sound starts).
	bool move(const Slot *shooter, bool moving, bool hitBank);
};

}  // namespace rr
