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
// Timing. Whether a register write still changes the current scanline, and whether a
// collision read already sees it, depends on where in the scanline the instruction runs. The
// model follows the reference emulator (atari800, without its "cycle exact" option), whose
// traces the port is checked against. Per scanline of 114 machine cycles:
//   - at the start ANTIC fetches player and missile data (5 cycles) and the display list
//     instruction (1 cycle, 3 when it carries a screen address);
//   - the CPU runs until cycle 28; an instruction that starts before that point runs in full;
//   - the whole scanline is drawn with the registers as they are now, collisions included;
//     ANTIC then takes 9 refresh cycles and its screen data (48 cycles for a river line);
//   - the CPU runs to the end of the scanline; STA WSYNC makes it wait until cycle 106.
// So display() executes the DLI's instructions with their cycle counts (branches, page
// crossings) and lets the scanlines happen in between.
#include <algorithm>
#include <functional>

#include "game.h"
#include "river_draw.h"

namespace rr {
namespace {

// First scanline of the river (display list: 8 + 8 + 4 blank lines, then mode E from 28).
constexpr int kRiverFirstLine = 28;
constexpr int kRiverLines = 160;
// display list: 2 blank lines (6 on PAL), then the panel (mode 6)
constexpr int kPanelFirstLine[2] = {190, 194};

constexpr int kLineCycles = 114;
constexpr int kDrawCycle = 28;
constexpr int kWsyncCycle = 106;
constexpr int kRefreshCycles = 9;

// Player 3 memory ($0F00) less $44: indexed by the DLI's scanline counter.
constexpr uint16_t kPlayer3Rows = 0x0EBC;

class Beam {
public:
	using Record = std::function<void(int line)>;

	// lmsLines: river rows whose display list instruction carries a screen address
	Beam(const Game &game, const std::vector<std::array<uint8_t, kLineBytes>> &screen, int line, int xpos,
		const bool lmsLines[kRiverLines], bool pal, Record record)
		: game_(game), screen_(screen), line_(line), xpos_(xpos), drawn_(xpos >= kDrawCycle), lms_(lmsLines),
			panelFirstLine_(kPanelFirstLine[pal]), record_(std::move(record)) {
		latch();
	}

	// Hardware registers (horizontal positions in colour clocks).
	uint8_t hposP[4] = {}, hposM[4] = {}, sizeP[4] = {};
	uint8_t sizeM = 0x01;      // SIZEM: missile 0 double width (A116)
	uint8_t colourP3 = 0, bankColour = 0;

	// Collision registers, accumulated over the scanlines drawn since the last HITCLR, as GTIA's
	// M0PF-M3PF (missile with playfield), P0PF-P3PF, M0PL-M3PL (missile with player), P0PL-P3PL.
	// Playfield bits: 1 bank, 2 edge, 4 water; player bits: 1 << player.
	uint8_t mpf[4] = {}, ppf[4] = {}, mpl[4] = {}, ppl[4] = {};
	void hitclr() {
		std::fill(std::begin(mpf), std::end(mpf), 0);
		std::fill(std::begin(ppf), std::end(ppf), 0);
		std::fill(std::begin(mpl), std::end(mpl), 0);
		std::fill(std::begin(ppl), std::end(ppl), 0);
	}

	// One instruction of `cycles` cycles; its effect (the caller's next statement) happens
	// after the scanline events that come before its start.
	void op(int cycles) {
		settle();
		xpos_ += cycles;
	}
	// A branch: 2 cycles, 3 when taken, 4 when taken to another page.
	bool branch(bool taken, bool otherPage = false) {
		op(taken ? (otherPage ? 4 : 3) : 2);
		return taken;
	}
	// STA WSYNC (after its op(4)): the CPU waits for cycle 106. In the reference emulator a
	// write before the scanline is drawn halts the CPU there; the line is drawn and the CPU
	// goes on at cycle 106 of the same line. A write after cycle 106 waits for the next line.
	void wsync() {
		if (drawn_ && xpos_ <= kWsyncCycle) {
			xpos_ = kWsyncCycle;
			return;
		}
		if (drawn_) {
			xpos_ = kLineCycles;
			settle();   // next scanline
		}
		xpos_ = kDrawCycle;
		settle();       // draw it
		xpos_ = kWsyncCycle;
	}
	// Run to the start of the next instruction (used before the end of the routine).
	void finish() { settle(); }

private:
	const Game &game_;
	const std::vector<std::array<uint8_t, kLineBytes>> &screen_;
	int line_;
	int xpos_;
	bool drawn_;
	const bool *lms_;
	int panelFirstLine_;
	Record record_;
	uint8_t p_[4] = {}, m_ = 0;   // player / missile graphics fetched at the start of the line

	int river(int line) const { return line - kRiverFirstLine; }
	int startCycles(int line) const {
		int c = 5;                                                   // player and missile DMA
		const int r = river(line);
		if (r >= 0 && r < kRiverLines) c += lms_[r] ? 3 : 1;          // one instruction per river line
		else if (line == 24 || line == kRiverFirstLine + kRiverLines) c += 1;   // blank-line instructions
		else if (line == panelFirstLine_) c += 3;                    // panel: mode 6 with address
		return c;
	}
	int screenCycles(int line) const {
		const int r = river(line);
		if (r >= 0 && r < kRiverLines) return 48;                    // wide mode E line
		if (line == panelFirstLine_) return 20;                      // first line of a mode 6 row
		return 0;
	}

	void settle() {
		for (;;) {
			if (!drawn_ && xpos_ >= kDrawCycle) {
				draw();
			} else if (xpos_ >= kLineCycles) {
				xpos_ -= kLineCycles;
				line_++;
				latch();
				xpos_ += startCycles(line_);
				drawn_ = false;
			} else {
				break;
			}
		}
	}
	void draw() {
		collide();
		record_(line_);
		xpos_ += kRefreshCycles + screenCycles(line_);
		drawn_ = true;
	}
	void latch() {
		for (int n = 0; n < 4; n++) p_[n] = game_.g.pm.player(n, line_);
		m_ = game_.g.pm.missiles(line_);
	}
	friend void recordScan(const Beam &, Game::Scanline &);

	// The 192 colour clocks of a scanline (one per playfield pixel, two columns of the screen
	// buffer), one bit each.
	struct Mask {
		uint64_t w[3] = {};
		void set(int from, int count) {   // colour clocks from..from+count-1, clipped to the line
			for (int x = std::max(0, from); x < std::min(kClocks, from + count); x++) w[x >> 6] |= uint64_t(1) << (x & 63);
		}
		bool operator&(const Mask &o) const { return (w[0] & o.w[0]) | (w[1] & o.w[1]) | (w[2] & o.w[2]); }
		bool empty() const { return !(w[0] | w[1] | w[2]); }
	};
	static constexpr int kClocks = kLineBytes * 4;

	// The colour clocks a sprite covers: `count` graphics bits (8 for a player, 2 for a
	// missile), each 1, 2 or 4 clocks wide.
	static Mask spriteMask(uint8_t bits, int count, uint8_t hpos, uint8_t size) {
		Mask m;
		const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		for (int b = 0; b < count; b++)
			if (bits & (1 << (count - 1 - b))) m.set(hpos + b * w - 32, w);
		return m;
	}

	// GTIA's collision detection for the scanline being drawn: which sprites overlap the
	// playfield colours and each other.
	void collide() {
		Mask pl[4], ms[4];
		bool any = false;
		for (int n = 0; n < 4; n++) {
			if (p_[n]) { pl[n] = spriteMask(p_[n], 8, hposP[n], sizeP[n]); any = true; }
			const uint8_t mb = uint8_t((m_ >> (2 * n)) & 3);
			if (mb) { ms[n] = spriteMask(mb, 2, hposM[n], uint8_t(sizeM >> (2 * n))); any = true; }
		}
		if (!any) return;
		// playfield of this scanline: the river line's pixels by colour (1 bank, 2 edge, 3
		// water: PF0-PF2); nothing outside the river
		Mask pf[3];
		const int row = river(line_);
		if (row >= 0 && row < kRiverLines && size_t(row) < screen_.size()) {
			const uint8_t *src = screen_[size_t(row)].data();
			for (int p = 0; p < kClocks; p++) {
				const int value = (src[p >> 2] >> (6 - 2 * (p & 3))) & 3;
				if (value) pf[value - 1].w[p >> 6] |= uint64_t(1) << (p & 63);
			}
		}
		auto playfieldBits = [&](const Mask &m) {
			return uint8_t((m & pf[0] ? 1 : 0) | (m & pf[1] ? 2 : 0) | (m & pf[2] ? 4 : 0));
		};
		for (int n = 0; n < 4; n++) {
			if (!pl[n].empty()) {
				ppf[n] |= playfieldBits(pl[n]);
				for (int k = 0; k < 4; k++)
					if (k != n && (pl[n] & pl[k])) ppl[n] |= uint8_t(1 << k);
			}
			if (!ms[n].empty()) {
				mpf[n] |= playfieldBits(ms[n]);
				for (int k = 0; k < 4; k++)
					if (ms[n] & pl[k]) mpl[n] |= uint8_t(1 << k);
			}
		}
	}

public:
	uint8_t p3Shape() const { return p_[3]; }
};

bool crosses(uint16_t base, int index) { return ((base & 0xFF) + index) > 0xFF; }

}  // namespace

void Game::display() {
	RiverState &s = gen.s;
	scan = {};
	// river rows whose display list instruction has a screen address: the top row and the
	// rows where screen memory continues in the other 4 KB block ($2000-$2EFF, $3000-$3EFF)
	bool lms[kRiverLines] = {};
	{
		const int top = g.screenTop;
		const int blockEnd = (top >> 8) >= 0x30 ? 0x3F00 : 0x2F00;
		const int k1 = (blockEnd - top) / kLineBytes;
		lms[0] = true;
		if (k1 < kRiverLines) lms[k1] = true;
		if (k1 + 80 < kRiverLines) lms[k1 + 80] = true;
	}
	Beam *bp = nullptr;
	auto record = [&](int line) {
		if (line < 0 || line >= int(scan.size())) return;
		Scanline &sl = scan[size_t(line)];
		sl.p3 = bp->p3Shape();
		sl.p3x = bp->hposP[3];
		sl.p3size = bp->sizeP[3];
		sl.p3colour = bp->colourP3;
		sl.bankColour = bp->bankColour;
	};
	// The DLI starts on scanline 23 (the last blank line before the river); B500-B51D take
	// it to about cycle 76.
	Beam beam(*this, screen, 23, 76, lms, pal_, record);
	bp = &beam;
	beam.bankColour = g.colourBank;   // set by the VBI (sub $AFD0)
	beam.hposP[3] = 0;                 // VBI (B411)
	beam.sizeP[3] = s.slot[kSlots - 1].size;
	g.hit = Collisions();              // B510
	beam.op(4); beam.wsync();          // B51E
	beam.op(3 + 3 + 3);                // B521-B525
	beam.op(3 + 2 + 2 + 2 + 2 + 2); beam.op(4); beam.hposM[1] = uint8_t(g.fuel / 8 + 0x5C);   // B527-B52F
	beam.op(3); beam.op(4); beam.hposP[2] = g.planeX;                                         // B532
	beam.op(2 + 2); beam.op(4); beam.hposM[2] = uint8_t(g.planeX + 4);                        // B537
	beam.op(2); beam.op(4);                                                                   // COLPM1
	beam.op(2 + 3); beam.branch(g.player != 0); if (!g.player) beam.op(2); beam.op(4);        // COLPM2
	beam.op(3); beam.op(4); beam.hposP[1] = s.tankX;                                          // B54D
	beam.op(3); beam.op(4); beam.hposM[0] = s.missileX;                                       // B552
	beam.op(3); beam.op(4); beam.hposP[0] = g.shellX;                                         // B557
	beam.op(3 + 2); beam.branch(true);                                                        // B55C-B560

	// The 6502's X register counts scanlines ($5D on the first river line); the river ends when
	// it wraps to 0. Y walks the slots; the slot after the last one has y = $FF.
	auto slotY = [&](int n) -> uint8_t { return n < kSlots ? s.slot[n].y : 0xFF; };   // $0C,Y ($17 = $FF)
	int counter = 0x5D;
	int slot = s.firstSlot;
	int current = slot;         // $06
	bool endedAtB664 = false;
	// leaving the loop through B5E3 (BEQ to another page) and B60B
	auto exitViaB5E3 = [&]() { beam.branch(true, true); beam.branch(true); };
	for (;;) {
		beam.op(3);                                         // B565 STY $06
		current = slot;
		// B567: wait for the scanline where the slot's object starts
		bool end = false;
		for (;;) {
			beam.op(4); beam.wsync();
			beam.op(2 + 4);                                 // TXA, CMP $0C,Y
			if (beam.branch(counter >= slotY(slot))) break;
			beam.op(2);                                     // INX
			counter = (counter + 1) & 0xFF;
			if (!beam.branch(counter != 0)) {                     // BNE B567 / BEQ B5E3
				beam.branch(true);
				exitViaB5E3();
				end = true;
				break;
			}
		}
		if (end) break;
		const int start = counter;                          // B575 STX $04
		beam.op(3 + 2);
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) { exitViaB5E3(); break; }
		const Slot sl = slot < kSlots ? s.slot[slot] : Slot{};
		beam.op(4); beam.op(4); beam.hposP[3] = sl.x;       // B57A-B57D
		beam.op(2); beam.op(5); g.pm.at(uint16_t(kPlayer3Rows + counter)) = 0;   // blank rows above the object
		beam.op(5); g.pm.at(uint16_t(kPlayer3Rows + counter + 1)) = 0;
		const uint8_t flags = sl.flags;
		beam.op(4 + 3);                                     // $07
		beam.op(4); beam.wsync();                           // B58D
		beam.op(2);
		const bool facingLeft = flags & slotflag::kFacingLeft;
		if (!beam.branch(!facingLeft)) beam.op(2);
		const uint16_t shapes = rom_.shapeAddress(sl.kind + (facingLeft ? 0x10 : 0));   // mirrored frames
		beam.op(2 + 4 + 2 + 4 + 3 + 2);                     // CLC ... INX
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) { exitViaB5E3(); break; }
		beam.op(3 + 4); beam.op(4); beam.sizeP[3] = sl.size;   // B5A3-B5A8
		beam.op(4 + 3);
		beam.op(4); beam.wsync();                           // B5B0
		const uint16_t colours = rom_.coloursAddress(sl.type);
		int row = uint8_t(sl.y + rom_.objectHeight(sl.type) - start);   // the shape row on this scanline
		beam.op(4 + 2 + 4 + 3 + 3 + 2 + 4 + 2 + 3 + 2);     // B5B3-B5C5
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) { exitViaB5E3(); break; }
		beam.op(2);                                         // TAY
		if (beam.branch(row >= 0x80)) {                     // B5C9: object already gone: next slot
			beam.op(3 + 2);
			slot = current + 1;
			continue;
		}
		// B5CB: one shape row per scanline, bottom row first
		bool wrapped = false;
		for (;;) {
			beam.op(crosses(shapes, row) ? 6 : 5);
			beam.op(5); g.pm.at(uint16_t(kPlayer3Rows + counter)) = rom_.at(uint16_t(shapes + row));
			beam.op(4); beam.wsync();
			beam.op(3);                                     // BIT $07
			if (!beam.branch(!(flags & slotflag::kBridge))) {   // bridge: the road across the bank
				beam.op(crosses(Rom::kRoadColours, row) ? 5 : 4);
				beam.op(4); beam.bankColour = rom_.roadColour(row);
			}
			beam.op(crosses(colours, row) ? 6 : 5);
			beam.op(4); beam.colourP3 = rom_.at(uint16_t(colours + row));
			beam.op(2);
			counter = (counter + 1) & 0xFF;
			if (beam.branch(counter == 0)) { wrapped = true; break; }
			beam.op(2);
			if (!beam.branch(--row >= 0)) break;
		}
		if (wrapped) { exitViaB5E3(); break; }
		// B5E8: collisions of this object
		beam.op(3); beam.op(4);
		const bool bulletHit = beam.mpl[2] & 0x08;         // M2PL: the bullet touched player 3
		beam.op(2);
		if (!beam.branch(!bulletHit)) { beam.op(3); g.hit.bulletHitObject = uint8_t(current); }
		beam.op(4); beam.wsync();                           // B5F3
		beam.op(3);
		if (!beam.branch(!(flags & slotflag::kBridge), true)) {   // bridge: the bank colour back
			beam.op(3); beam.op(4); beam.bankColour = s.sectionBankColour;
			beam.op(3);
		} else {
			beam.op(4);
			const bool shellHit = beam.ppl[3] & 0x01;      // P3PL: player 3 touched the shell
			beam.op(2);
			if (!beam.branch(!shellHit)) { beam.op(3); g.hit.shellHitObject = uint8_t(current); }
		}
		beam.op(2);                                         // B60A INX
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) break;
		beam.op(4);
		const uint8_t planeTouched = beam.ppl[2];           // P2PL: the plane touched players
		beam.op(2);
		if (!beam.branch(!(planeTouched & 0x01))) { beam.op(3); g.hit.planeHitShell = uint8_t(current); }
		beam.op(2);
		if (!beam.branch(!(planeTouched & 0x08))) { beam.op(3); g.hit.planeHitObject = uint8_t(current); }
		beam.op(4);
		const bool touchingEdge = beam.ppf[3] & 0x02;       // P3PF: the object touched the bank edge
		beam.op(2);
		beam.op(4); beam.wsync();                           // B61F
		beam.op(4 + 2 + 4); beam.op(5);
		if (current < kSlots) {
			Slot &obj = s.slot[current];
			obj.flags = uint8_t((obj.flags & ~slotflag::kTouchingEdge) | (touchingEdge ? slotflag::kTouchingEdge : 0));
		}
		beam.op(2);
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) break;
		beam.op(4);
		const bool missileHit = beam.mpl[0] & 0x04;        // M0PL: the missile touched the plane
		beam.op(2);
		if (!beam.branch(!missileHit)) { beam.op(3); g.hit.missileHitPlane = uint8_t(current); }
		beam.op(4);
		const bool missileAtBank = beam.mpf[0] & 0x03;     // M0PF: bank or edge
		beam.op(2);
		if (!beam.branch(!missileAtBank)) { beam.op(3); g.hit.missileHitBank = uint8_t(current); }
		beam.op(4);
		const bool tankEdge = beam.ppf[1] & 0x02;           // P1PF: the tank reached the edge
		beam.op(4); beam.wsync();                           // B645
		beam.op(2);
		if (!beam.branch(!tankEdge)) { beam.op(3); g.hit.tankAtEdge = uint8_t(current); }
		beam.op(4);
		const bool planeAtBank = beam.ppf[2] & 0x01;       // P2PF: the plane touched the bank
		beam.op(2);
		if (!beam.branch(!planeAtBank)) { beam.op(3); g.hit.planeHitBank = uint8_t(current); }
		beam.op(4);
		const bool bulletAtBank = beam.mpf[2] & 0x03;      // M2PF: bank or edge
		beam.op(2);
		if (!beam.branch(!bulletAtBank)) { beam.op(3); g.hit.bulletHitBank = uint8_t(current); }
		beam.op(4); beam.hitclr();                          // B65F
		beam.op(2 + 2);
		slot = current + 1;
		counter = (counter + 1) & 0xFF;
		if (beam.branch(counter == 0)) { beam.op(3); endedAtB664 = true; break; }
		beam.op(3);                                         // JMP B565
	}
	// B66B: end of the river; the panel DLI part (B6BF-B725) reads the collisions of the
	// object still in progress, unless the loop ended right after a HITCLR.
	beam.op(4); beam.wsync();
	beam.op(4); beam.hposP[1] = 0;
	beam.op(4); beam.hposP[0] = 0;
	beam.op(4); beam.hposM[0] = 0;
	beam.op(4 + 2 + 4 + 2 + 4 + 2 + 4 + 2 + 4 + 3);
	if (!beam.branch(g.respawned == 0)) {
		beam.op(3);
		if (!beam.branch(g.player == 0)) beam.op(2);
	}
	beam.op(4 + 3);
	if (!beam.branch(g.planeState == 0)) {
		beam.op(3);
		if (!beam.branch(((g.planeState | g.attractFlags) & 0x80) != 0)) {
			beam.op(3 + 2);
			if (!beam.branch((g.frameCount & 0x30) != 0)) beam.op(2 + 5);
		}
	}
	beam.op(2 + 4 + 2);
	beam.op(4); beam.wsync();                               // B6B4
	beam.op(4 + 2 + 4 + 3 + 2);
	if (beam.branch(current >= kSlots)) return;
	beam.op(2 + 3 + 3);
	if (beam.branch(endedAtB664)) return;
	Slot &obj = s.slot[current];
	beam.op(4);
	obj.flags = uint8_t((obj.flags & ~slotflag::kTouchingEdge) | ((beam.ppf[3] >> 1) & slotflag::kTouchingEdge));
	beam.op(2 + 4 + 2 + 4 + 5);
	beam.op(4); if (beam.mpl[0] & 0x04) g.hit.missileHitPlane = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4); if (beam.mpf[0] & 0x03) g.hit.missileHitBank = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4); if (beam.mpf[2] & 0x03) g.hit.bulletHitBank = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4); if (beam.ppf[2] & 0x01) g.hit.planeHitBank = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4);
	if (beam.ppl[2] & 0x01) g.hit.planeHitShell = uint8_t(current);
	if (beam.ppl[2] & 0x08) g.hit.planeHitObject = uint8_t(current);
	beam.op(2 + 2 + 3 + 2 + 2 + 3);
	beam.op(4); if (beam.ppl[3] & 0x01) g.hit.shellHitObject = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4); if (beam.ppf[1] & 0x02) g.hit.tankAtEdge = uint8_t(current);
	beam.op(2 + 2 + 3);
	beam.op(4); if (beam.mpl[2] & 0x08) g.hit.bulletHitObject = uint8_t(current);
	beam.finish();
}

void Game::drawRiver(uint8_t out[240][kRowWidth]) const {
	const RiverState &s = gen.s;
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
		const uint8_t colours[4] = {0x00, sc.bankColour, g.colourEdge, g.colourWater};
		if (size_t(k) < screen.size()) {
			const uint8_t *src = screen[size_t(k)].data();
			for (int i = 0; i < kLineBytes; i++) {   // 4 pixels per byte, 2 columns per pixel
				const uint8_t v = src[i];
				uint8_t *d = row + 8 * i;
				d[0] = d[1] = colours[v >> 6];
				d[2] = d[3] = colours[(v >> 4) & 3];
				d[4] = d[5] = colours[(v >> 2) & 3];
				d[6] = d[7] = colours[v & 3];
			}
		} else {
			std::fill(row, row + kRowWidth, 0);
		}
		// players over the playfield (PRIOR 1), player 0 on top
		player(row, sc.p3, sc.p3x, sc.p3size, sc.p3colour);
		const uint8_t planeColour = g.player ? 0x8E : 0x1E;                 // COLPM2 (B542)
		player(row, g.pm.player(2, line), g.planeX, 0, planeColour);        // the plane
		const uint8_t m2 = (g.pm.missiles(line) >> 4) & 0x03;               // its bullet
		for (int b = 0; b < 2; b++)
			if (m2 & (2 >> b)) put(row, 2 * (g.planeX + 4 + b - 32), 2, planeColour);
		player(row, g.pm.player(1, line), s.tankX, 0, 0x00);                // tank, COLPM1 = 0
		const uint8_t m0 = g.pm.missiles(line) & 0x03;                      // helicopter missile, double width
		for (int b = 0; b < 2; b++)
			if (m0 & (2 >> b)) put(row, 2 * (s.missileX + b * 2 - 32), 4, g.shellColour);
		player(row, g.pm.player(0, line), g.shellX, 0, g.shellColour);      // tank shell
		std::fill(row, row + 24, 0);
		std::fill(row + 360, row + kRowWidth, 0);
	}
}

}  // namespace rr
