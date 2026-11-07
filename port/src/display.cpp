// The river part of the display, scanline by scanline, as the original's display list
// interrupt (DLI, B510-B6BC and B6C5-B727) drives it. See game.h.
//
// One hardware sprite, player 3, shows every object: the DLI waits for the scanline where
// the next object slot starts, sets its position and size, then on each following scanline
// writes one row of the shape into player 3 memory and sets the row colour. Because the
// player data of a scanline is fetched at the start of that scanline, the rows written now
// appear in the next frame (one scanline lower relative to the river, which has scrolled by
// then). After each object the DLI reads the collision registers and clears them, so every
// collision is known per object slot.
//
// Timing model: the DLI's counter X is the scanline + $44 (it starts at $5D on scanline 25
// and is incremented once per WSYNC, except in the waiting loop where it trails by one after
// the first object). Register writes done right after a WSYNC (colours) apply to that
// scanline, writes later in the scanline (position, size) to the next one.
#include <algorithm>

#include "game.h"
#include "river_draw.h"

namespace rr {
namespace {

// First scanline of the river (display list: 8 + 8 + 4 blank lines, then mode E from 28).
constexpr int kRiverFirstLine = 28;
constexpr int kRiverLines = 160;

class Beam {
public:
	Beam(const Game &game, const std::vector<std::array<uint8_t, kLineBytes>> &screen, int line)
	    : game_(game), screen_(screen), line_(line) { latch(); }

	int line() const { return line_; }

	// Hardware registers (horizontal positions in colour clocks).
	uint8_t hposP[4] = {}, hposM[4] = {}, sizeP[4] = {};
	uint8_t sizeM = 0x01;      // SIZEM: missile 0 double width (A116)
	uint8_t colourP3 = 0, bankColour = 0;
	// Registers written late in a scanline: they take effect on the next one.
	void setLateHposP3(uint8_t v) { pendingHpos_ = v; hasPendingHpos_ = true; }
	void setLateSizeP3(uint8_t v) { pendingSize_ = v; hasPendingSize_ = true; }

	// Collision registers, accumulated over finished scanlines since the last HITCLR.
	uint8_t mpf[4] = {}, ppf[4] = {}, mpl[4] = {}, ppl[4] = {};
	void hitclr() {
		std::fill(std::begin(mpf), std::end(mpf), 0);
		std::fill(std::begin(ppf), std::end(ppf), 0);
		std::fill(std::begin(mpl), std::end(mpl), 0);
		std::fill(std::begin(ppl), std::end(ppl), 0);
	}

	// STA WSYNC: finish the current scanline and start the next.
	template <class Record>
	void wsync(Record &&record) {
		finishLine();
		record(line_);
		line_++;
		if (hasPendingHpos_) { hposP[3] = pendingHpos_; hasPendingHpos_ = false; }
		if (hasPendingSize_) { sizeP[3] = pendingSize_; hasPendingSize_ = false; }
		latch();
	}
	uint8_t p3Shape() const { return p_[3]; }

private:
	const Game &game_;
	const std::vector<std::array<uint8_t, kLineBytes>> &screen_;
	int line_;
	uint8_t p_[4] = {}, m_ = 0;   // player / missile graphics fetched at the start of the line
	uint8_t pendingHpos_ = 0, pendingSize_ = 0;
	bool hasPendingHpos_ = false, hasPendingSize_ = false;

	void latch() {
		for (int n = 0; n < 4; n++) p_[n] = game_.g.pm.player(n, line_);
		m_ = game_.g.pm.missiles(line_);
	}

	// Columns of the 384-column screen buffer (2 per colour clock) covered by a sprite.
	static void playerMask(uint8_t bits, uint8_t hpos, uint8_t size, bool mask[kRowWidth]) {
		const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		for (int b = 0; b < 8; b++) {
			if (!(bits & (0x80 >> b))) continue;
			const int x0 = 2 * (hpos + b * w - 32);
			for (int x = std::max(0, x0); x < std::min(kRowWidth, x0 + 2 * w); x++) mask[x] = true;
		}
	}
	static void missileMask(uint8_t bits, uint8_t hpos, uint8_t size, bool mask[kRowWidth]) {
		const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		for (int b = 0; b < 2; b++) {
			if (!(bits & (2 >> b))) continue;
			const int x0 = 2 * (hpos + b * w - 32);
			for (int x = std::max(0, x0); x < std::min(kRowWidth, x0 + 2 * w); x++) mask[x] = true;
		}
	}

	void finishLine() {
		bool pl[4][kRowWidth] = {}, ms[4][kRowWidth] = {};
		bool any = false;
		for (int n = 0; n < 4; n++) {
			if (p_[n]) { playerMask(p_[n], hposP[n], sizeP[n], pl[n]); any = true; }
			const uint8_t mb = uint8_t((m_ >> (2 * n)) & 3);
			if (mb) { missileMask(mb, hposM[n], uint8_t(sizeM >> (2 * n)), ms[n]); any = true; }
		}
		if (!any) return;
		// playfield of this scanline: 2-bit values of the river line, 0 outside the river
		uint8_t pf[kRowWidth] = {};
		const int row = line_ - kRiverFirstLine;
		if (row >= 0 && row < kRiverLines && size_t(row) < screen_.size()) {
			const uint8_t *src = screen_[size_t(row)].data();
			for (int p = 0; p < kLineBytes * 4; p++) pf[2 * p] = pf[2 * p + 1] = uint8_t((src[p >> 2] >> (6 - 2 * (p & 3))) & 3);
		}
		for (int x = 0; x < kRowWidth; x++) {
			const uint8_t pfBit = pf[x] ? uint8_t(1 << (pf[x] - 1)) : 0;   // value 1-3 = PF0-PF2
			for (int n = 0; n < 4; n++) {
				if (pl[n][x]) {
					ppf[n] |= pfBit;
					for (int k = 0; k < 4; k++)
						if (k != n && pl[k][x]) ppl[n] |= uint8_t(1 << k);
				}
				if (ms[n][x]) {
					mpf[n] |= pfBit;
					for (int k = 0; k < 4; k++)
						if (pl[k][x]) mpl[n] |= uint8_t(1 << k);
				}
			}
		}
	}
};

}  // namespace

void Game::display() {
	RiverState &s = gen.s;
	scan = {};
	Beam beam(*this, screen, 23);   // the DLI starts on the last blank line before the river
	auto record = [&](int line) {
		if (line < 0 || line >= int(scan.size())) return;
		Scanline &sl = scan[size_t(line)];
		sl.p3 = beam.p3Shape();
		sl.p3x = beam.hposP[3];
		sl.p3size = beam.sizeP[3];
		sl.p3colour = beam.colourP3;
		sl.bankColour = beam.bankColour;
	};
	beam.bankColour = bankColour(s);   // set by sub $AFD0 at the top of the frame
	beam.hposP[3] = 0;                 // VBI (B411)
	beam.sizeP[3] = s.slot[kSlots - 1].size;
	// B510
	g.hit = Collisions();
	beam.wsync(record);                // B51E
	// B527-B559 (scanline 24)
	beam.hposM[1] = uint8_t(g.fuel / 8 + 0x5C);
	beam.hposP[2] = g.planeX;
	beam.hposM[2] = uint8_t(g.planeX + 4);
	beam.hposP[1] = s.roadX;
	beam.hposM[0] = s.vC1;
	beam.hposP[0] = g.shellX;

	auto slotY = [&](int y) -> uint8_t { return y < kSlots ? s.slot[y].y : 0xFF; };   // $0C,Y ($17 = $FF)
	int X = 0x5D;
	int Y = s.firstSlot;
	int current = Y;            // $06
	bool endedAtB664 = false;
	for (;;) {
		current = Y;                                        // B565
		// B567: wait for the scanline where the slot's object starts
		bool end = false;
		for (;;) {
			beam.wsync(record);
			if (X >= slotY(Y)) break;
			if (++X == 0x100) { end = true; break; }
		}
		if (end) break;
		const int start = X;                                // B575 $04
		if (++X == 0x100) break;
		const Slot sl = Y < kSlots ? s.slot[Y] : Slot{};
		beam.setLateHposP3(sl.x);
		g.pm.at(uint16_t(0x0EBC + X)) = 0;
		g.pm.at(uint16_t(0x0EBD + X)) = 0;
		const uint8_t flags = sl.flags;                     // $07
		beam.wsync(record);                                 // B58D
		const int kind = sl.kind + ((flags & 0x08) ? 0x10 : 0);
		const uint16_t shapes = uint16_t(0xB900 + rom_.at(uint16_t(0xBB40 + kind)));
		if (++X == 0x100) break;
		beam.setLateSizeP3(sl.size);
		beam.wsync(record);                                 // B5B0
		const uint16_t colours = uint16_t(0xB700 + rom_.at(uint16_t(0xBB30 + sl.type)));
		int row = uint8_t(sl.y + rom_.at(uint16_t(0xBB60 + sl.type)) - start);
		if (++X == 0x100) break;
		if (row >= 0x80) {                                  // B5C9: object already gone: next slot
			Y = current + 1;
			continue;
		}
		// B5CB: one shape row per scanline, bottom row first
		bool wrapped = false;
		for (;;) {
			g.pm.at(uint16_t(0x0EBC + X)) = rom_.at(uint16_t(shapes + row));
			beam.wsync(record);
			if (flags & 0x80) beam.bankColour = rom_.at(uint16_t(0xB4E5 + row));   // bridge: road
			beam.colourP3 = rom_.at(uint16_t(colours + row));
			if (++X == 0x100) { wrapped = true; break; }
			if (--row < 0) break;
		}
		if (wrapped) break;
		// B5E8: collisions of this object
		if (beam.mpl[2] & 0x08) g.hit.m2p3 = uint8_t(current);
		beam.wsync(record);                                 // B5F3
		if (flags & 0x80) {
			beam.bankColour = s.bridgeFlags;                // bank colour back after the bridge
		} else if (beam.ppl[3] & 0x01) {
			g.hit.p3p0 = uint8_t(current);
		}
		if (++X == 0x100) break;                            // B60A
		if (beam.ppl[2] & 0x01) g.hit.p2p0 = uint8_t(current);
		if (beam.ppl[2] & 0x08) g.hit.p2p3 = uint8_t(current);
		const bool bank = beam.ppf[3] & 0x02;               // P3PF bit 1: the bank edge colour
		beam.wsync(record);                                 // B61F
		if (current < kSlots) s.slot[current].flags = uint8_t((s.slot[current].flags & 0xFE) | (bank ? 1 : 0));
		if (++X == 0x100) break;
		if (beam.mpl[0] & 0x04) g.hit.m0p2 = uint8_t(current);
		if (beam.mpf[0] & 0x03) g.hit.m0pf = uint8_t(current);
		const bool tankEdge = beam.ppf[1] & 0x02;
		beam.wsync(record);                                 // B645
		if (tankEdge) g.hit.p1pf = uint8_t(current);
		if (beam.ppf[2] & 0x01) g.hit.p2pf = uint8_t(current);
		if (beam.mpf[2] & 0x03) g.hit.m2pf = uint8_t(current);
		beam.hitclr();                                      // B65F
		Y = current + 1;
		if (++X == 0x100) { endedAtB664 = true; break; }
	}
	// B66B: end of the river; the panel DLI part (B6BF-B725) reads the collisions of the
	// object still in progress, unless the loop ended right after a HITCLR.
	beam.wsync(record);
	beam.hposP[1] = 0;
	beam.hposM[0] = 0;
	beam.wsync(record);                                     // B6B4
	if (current < kSlots && !endedAtB664) {
		Slot &sl = s.slot[current];
		sl.flags = uint8_t((sl.flags & 0xFE) | ((beam.ppf[3] >> 1) & 0x01));
		if (beam.mpl[0] & 0x04) g.hit.m0p2 = uint8_t(current);
		if (beam.mpf[0] & 0x03) g.hit.m0pf = uint8_t(current);
		if (beam.mpf[2] & 0x03) g.hit.m2pf = uint8_t(current);
		if (beam.ppf[2] & 0x01) g.hit.p2pf = uint8_t(current);
		if (beam.ppl[2] & 0x01) g.hit.p2p0 = uint8_t(current);
		if (beam.ppl[2] & 0x08) g.hit.p2p3 = uint8_t(current);
		if (beam.ppl[3] & 0x01) g.hit.p3p0 = uint8_t(current);
		if (beam.ppf[1] & 0x02) g.hit.p1pf = uint8_t(current);
		if (beam.mpl[2] & 0x08) g.hit.m2p3 = uint8_t(current);
	}
}

void Game::drawRiver(uint8_t out[240][kRowWidth]) const {
	const RiverState &s = gen.s;
	// COLPF2: water, flashing red while a destroyed bridge flashes (VBI B414-B427)
	const uint8_t water = (g.state24 < 2 && g.bridgeFlash && (g.bridgeFlash & 1)) ? 0x42 : 0x84;
	auto put = [&](uint8_t *row, int x0, int w, uint8_t c) {
		for (int x = std::max(0, x0); x < std::min(kRowWidth, x0 + w); x++) row[x] = c;
	};
	auto player = [&](uint8_t *row, uint8_t bits, uint8_t hpos, uint8_t size, uint8_t c) {
		const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		for (int b = 0; b < 8; b++)
			if (bits & (0x80 >> b)) put(row, 2 * (hpos + b * w - 32), 2 * w, c);
	};
	for (int k = 0; k < kRiverLines; k++) {
		const int line = kRiverFirstLine + k;
		uint8_t *row = out[line - 8];
		const Scanline &sc = scan[size_t(line)];
		const uint8_t colours[4] = {0x00, sc.bankColour, 0x20, water};
		if (size_t(k) < screen.size()) {
			const uint8_t *src = screen[size_t(k)].data();
			for (int p = 0; p < kLineBytes * 4; p++) row[2 * p] = row[2 * p + 1] = colours[(src[p >> 2] >> (6 - 2 * (p & 3))) & 3];
		} else {
			std::fill(row, row + kRowWidth, 0);
		}
		// players over the playfield (PRIOR 1), player 0 on top
		player(row, sc.p3, sc.p3x, sc.p3size, sc.p3colour);
		player(row, g.pm.player(1, line), s.roadX, 0, 0x00);                // tank, COLPM1 = 0
		const uint8_t m0 = g.pm.missiles(line) & 0x03;                      // helicopter missile, double width
		for (int b = 0; b < 2; b++)
			if (m0 & (2 >> b)) put(row, 2 * (s.vC1 + b * 2 - 32), 4, g.shellColour);
		player(row, g.pm.player(0, line), g.shellX, 0, g.shellColour);      // tank shell
		std::fill(row, row + 24, 0);
		std::fill(row + 360, row + kRowWidth, 0);
	}
}

}  // namespace rr
