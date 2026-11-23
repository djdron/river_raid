// The player's plane: where it is, how it banks and how fast the river scrolls under it, its
// fuel, and its state (flying, exploding, waiting at a bridge, none).
#pragma once
#include <cstdint>

#include "ram_map.h"

namespace rr {

struct Plane {
	// state ($76): 0 flying; $FE-$80 exploding; $7F-$30 river refilling; $2F waiting for the
	// player; $FF no plane (attract mode, game over)
	static constexpr uint8_t kFlying = 0;
	static constexpr uint8_t kWaiting = 0x2F;      // at the bridge, until the player moves
	static constexpr uint8_t kLifeLost = 0x30;     // the river has been refilled
	static constexpr uint8_t kCrashed = 0xFE;      // the explosion starts; down to $80
	static constexpr uint8_t kNoPlane = 0xFF;      // attract mode, game over
	// frame ($5E): 7/8/9 banking left / right / level, 6-0 the explosion
	static constexpr uint8_t kLeft = 7, kRight = 8, kLevel = 9, kExploding = 6;

	uint8_t state = 0;
	uint8_t x = 0;          // $57
	uint8_t xFrac = 0;      // $58
	uint8_t accel = 0;      // $2B sideways acceleration
	uint8_t frame = 0;      // $5E
	uint8_t speed = 0;      // $59 scrolling, (lines per frame) x 128: $40 slow .. $FE fast
	uint8_t fuel = 0;       // $5B $FF full
	uint8_t fuelFrac = 0;   // $5C
	uint8_t stick = 0x0F;   // $19 the stick of the player whose turn it is, this frame (0 = pressed)
	uint8_t stickPrev = 0;  // $B9 the previous frame's, << 4

	void mapRam(RamMap &m) {
		m.byte("plane.state", 0x76, state).byte("plane.x", 0x57, x).byte("plane.xFrac", 0x58, xFrac);
		m.byte("plane.accel", 0x2B, accel).byte("plane.frame", 0x5E, frame).byte("plane.speed", 0x59, speed);
		m.byte("plane.fuel", 0x5B, fuel).byte("plane.fuelFrac", 0x5C, fuelFrac);
		m.byte("plane.stick", 0x19, stick).byte("plane.stickPrev", 0xB9, stickPrev);
	}

	bool flying() const { return state == kFlying; }
	bool exploding() const { return (state & 0x80) != 0; }   // (or no plane)

	// A455-A496: left or right accelerates (+8 per frame up to $F8); the plane moves a pixel
	// each time the fraction overflows, every frame at full speed.
	void steer() {
		uint8_t f = kLevel;
		const bool left = !(stick & 0x04), right = !(stick & 0x08);
		if (!left && !right) {
			accel = 0;
			xFrac = 0;
		} else {
			const int a = accel + 8;
			const bool full = a > 0xFF;               // already at full speed
			if (!full) accel = uint8_t(a);
			if (!right) {                             // left
				f = kLeft;
				if (full) {
					x--;
					xFrac = 0x08;                     // (what AND #$08 left in A)
				} else {
					const int frac = xFrac - accel;
					if (frac < 0) x--;
					xFrac = uint8_t(frac);
				}
			} else {
				f = kRight;
				if (full) {
					x++;
					xFrac = 0;                        // (what AND #$08 left in A)
				} else {
					const uint8_t start = (stickPrev & 0x80) ? 0xFF : xFrac;   // just pressed: move now
					const int frac = start + accel;
					if (frac > 0xFF) x++;
					xFrac = uint8_t(frac);
				}
			}
		}
		frame = f;
	}

	// A497-A4B5: up +2 (up to $FE), down -2 (down to $40), neither: back towards $80.
	void changeSpeed() {
		bool faster;
		if (!(stick & 0x01)) faster = true;           // up (also when both are pressed)
		else if (!(stick & 0x02)) faster = false;     // down
		else if (speed == 0x80) return;               // neither, at normal speed
		else faster = speed < 0x80;
		if (faster) {
			if (speed <= 0xFD) speed = uint8_t(speed + 2);
		} else if (speed >= 0x41) {
			speed = uint8_t(speed - 2);
		}
	}

	// A43F: $20 / 256 of fuel a frame. False when it has run out (the plane crashes).
	bool burnFuel() {
		const int frac = fuelFrac - 0x20;
		if (frac < 0) {
			if (fuel == 0) return false;
			fuel--;
		}
		fuelFrac = uint8_t(frac);
		return true;
	}

	// A40A: over a fuel depot, +2 a frame (ADC #1 with the carry of CMP #$0E). True when the
	// tank is full.
	bool refuel() {
		const int f = fuel + 2;
		if (f > 0xFF) {
			fuel = 0xFF;
			fuelFrac = 0xFF;
			return true;
		}
		fuel = uint8_t(f);
		return false;
	}
};

}  // namespace rr
