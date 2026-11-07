#include "river_draw.h"

#include <algorithm>
#include <cstring>

namespace rr {
namespace {

constexpr uint8_t kBackground = 0x00;
constexpr uint8_t kBankEdge = 0x20;     // COLPF1
constexpr uint8_t kWater = 0x84;        // COLPF2

// Scanline counter value of the row where new lines enter; the DLI counts it down per scanline.
constexpr uint8_t kEntryScanline = 0x5D;
// Sprite horizontal position of the left edge of the screen buffer (HPOSP3 = 32).
constexpr int kSpriteLeft = 32;

// Tables in the cartridge:
constexpr uint16_t kObjectHeight = 0xBB60;   // [type] last row of the object's sprite
constexpr uint16_t kShapeIndex = 0xBB40;     // [kind (+16 when flags bit 3)] offset into $B900
constexpr uint16_t kShapes = 0xB900;         // sprite rows, one byte each
constexpr uint16_t kColourIndex = 0xBB30;    // [type] offset into $B700
constexpr uint16_t kColours = 0xB700;        // sprite colour per row
constexpr uint16_t kRoadColours = 0xB4E5;    // bank colour per row next to a bridge

}  // namespace

uint8_t bankColour(const RiverState &s) {
	uint8_t c = uint8_t((s.mode & 4) ^ 0xD6);
	// While a bridge object is still being drawn in near the top, the shade is switched.
	if (s.newSlot < kSlots) {
		const Slot &sl = s.slot[s.newSlot];
		if ((sl.flags & 0x80) && sl.y < 0x5C) c ^= 4;
	}
	return c;
}

void drawScreenLine(const Rom &rom, const RiverState &s, const uint8_t line[kLineBytes], uint8_t out[kRowWidth]) {
	uint8_t bank = bankColour(s);

	// The first slot (from firstSlot up) whose sprite covers this scanline is drawn;
	// row = how far up from the object's bottom row the scanline is.
	bool haveSprite = false;
	uint8_t shape = 0, spriteColour = 0, spriteX = 0, spriteSize = 0;
	for (int k = s.firstSlot; k < kSlots; k++) {
		const Slot &sl = s.slot[k];
		const uint8_t height = rom.at(uint16_t(kObjectHeight + sl.type));
		const uint8_t row = uint8_t(sl.y + height - kEntryScanline);
		if (row > height || row >= 0x80) continue;
		if (sl.flags & 0x80) bank = rom.at(uint16_t(kRoadColours + row));   // bridge: road
		const int kind = sl.kind + ((sl.flags & 8) ? 16 : 0);
		shape = rom.at(uint16_t(kShapes + rom.at(uint16_t(kShapeIndex + kind)) + row));
		spriteColour = rom.at(uint16_t(kColours + rom.at(uint16_t(kColourIndex + sl.type)) + row));
		spriteX = sl.x;
		spriteSize = sl.size;
		haveSprite = true;
		break;
	}

	const uint8_t colours[4] = {kBackground, bank, kBankEdge, kWater};
	for (int p = 0; p < kLineBytes * 4; p++) {
		const int v = (line[p >> 2] >> (6 - 2 * (p & 3))) & 3;
		out[2 * p] = out[2 * p + 1] = colours[v];
	}

	if (haveSprite) {
		const int width = (spriteSize & 3) == 3 ? 4 : (spriteSize & 3) == 1 ? 2 : 1;   // SIZEP3
		for (int b = 0; b < 8; b++) {
			if (!(shape & (0x80 >> b))) continue;
			const int x0 = 2 * (spriteX + b * width - kSpriteLeft);
			for (int x = std::max(0, x0); x < std::min(kRowWidth, x0 + 2 * width); x++) out[x] = spriteColour;
		}
	}

	std::memset(out, 0, kVisibleFirst);
	std::memset(out + kVisibleLast + 1, 0, kRowWidth - kVisibleLast - 1);
}

}  // namespace rr
