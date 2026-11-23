// The plane's bullet (missile 2).
#pragma once
#include <cstdint>

namespace rr {

// The plane's bullet: 8 rows of missile 2 (one bit wide), at the plane's x + 4. y ($56) is the
// row above its top row; 1 (or 0) is no bullet. It flies up 6 rows a frame from row $A9 and is
// gone at the bank, at an object, or above row $1C (Game::flyPlane).
struct Bullet {
	uint8_t y = 0;

	bool shown() const { return y > 1; }
	// Its rows: from row y (at least row $1C) down, 8 of them
	int top() const { return (y - 1 < 0x1B ? 0x1B : y - 1) + 1; }
	void moveTo(int to) { y = uint8_t(to); }   // A5AA-A5B4
	void remove() { y = 1; }                   // B204-B220
};

}  // namespace rr
