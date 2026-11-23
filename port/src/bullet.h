// The plane's bullet (missile 2), and player-missile memory, where the original keeps the
// sprites' rows (the bullet draws itself there; display and drawing read it back).
#pragma once
#include <array>
#include <cstdint>

namespace rr {

// Player-missile memory $0B00-$0FFF (PMBASE $08, single-line resolution: one byte per
// scanline). $0B00 missiles (bits 1-0 missile 0, 3-2 missile 1, 5-4 missile 2), then players
// 0-3 at $0C00, $0D00, $0E00, $0F00. Kept as one block: the original's copy loops can run
// past the end of one page into the next.
struct PmMemory {
	static constexpr uint16_t kBase = 0x0B00;
	std::array<uint8_t, 0x500> bytes{};
	uint8_t &at(uint16_t addr) { return bytes[size_t(addr - kBase) % bytes.size()]; }
	uint8_t missiles(int line) const { return bytes[size_t(line & 0xFF)]; }
	uint8_t player(int n, int line) const { return bytes[size_t(0x100 * (n + 1) + (line & 0xFF))]; }
};

// The plane's bullet: 8 rows of missile 2 (one bit wide), at the plane's x + 4. y ($56) is the
// row above its top row; 1 (or 0) is no bullet. It flies up 6 rows a frame from row $A9 and is
// gone at the bank, at an object, or above row $1C (Game::flyPlane).
struct Bullet {
	static constexpr uint16_t kMemory = 0x0B00;   // missiles' memory
	static constexpr uint8_t kBits = 0x20;        // missile 2

	uint8_t y = 0;

	bool shown() const { return y > 1; }
	// Its rows: from row y (at least row $1C) down, 8 of them
	int top() const { return (y - 1 < 0x1B ? 0x1B : y - 1) + 1; }
	// To row y: its rows out of player-missile memory at the old place, in at the new (whole
	// bytes, so the other missiles' bits on those rows go too, as in the original: sub $B221 /
	// $B223, A5AA-A5B4)
	void moveTo(PmMemory &pm, int to) {
		draw(pm, 0);
		y = uint8_t(to);
		draw(pm, kBits);
	}
	// Gone (B204-B220): its rows cleared, y = 1
	void remove(PmMemory &pm) {
		draw(pm, 0);
		y = 1;
	}

private:
	void draw(PmMemory &pm, uint8_t bits) const {
		int row = y - 1;
		if (row == 0) return;
		if (row < 0x1B) row = 0x1B;
		for (int i = 1; i <= 8; i++) pm.at(uint16_t(kMemory + row + i)) = bits;
	}
};

}  // namespace rr
