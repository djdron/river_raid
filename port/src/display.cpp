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
#include <chrono>
#include <functional>

#include "game.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static int g_debugKind = -1;   // RR_BANK_DEBUG: the kind of the object being drawn

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

	// One sprite on the scanline: its graphics bits, where it is, and the colour clocks from
	// its first to its last set bit (left..right-1, not clipped to the line).
	struct Span {
		uint8_t bits = 0;
		int count = 0, hpos = 0, width = 1;
		int left = 0, right = 0;
		bool on() const { return bits != 0; }
	};
	static Span spriteSpan(uint8_t bits, int count, uint8_t hpos, uint8_t size) {
		Span sp;
		sp.bits = bits; sp.count = count; sp.hpos = hpos;
		sp.width = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		if (!bits) return sp;
		int first = 0, last = count - 1;
		while (!(bits & (1 << (count - 1 - first)))) first++;
		while (!(bits & (1 << (count - 1 - last)))) last--;
		sp.left = hpos - 32 + first * sp.width;
		sp.right = hpos - 32 + (last + 1) * sp.width;
		return sp;
	}
	// Does the sprite cover a clock of a..b-1? Its first and last set bits decide it when the
	// run reaches past either of them; a run strictly between them may fall into a hole: then
	// bit by bit.
	static bool spanHits(const Span &sp, int a, int b, bool &exact) {
		if (b <= sp.left || a >= sp.right) return false;
		if (a <= sp.left || b >= sp.right) {
			// the first or the last set bit is over the run, unless that clock is off the line
			const int x = a <= sp.left ? sp.left : sp.right - 1;
			if (x >= 0 && x < kClocks) return true;
		}
		exact = true;
		static const bool rough = std::getenv("RR_ROUGH_BANK") != nullptr;   // (an experiment: spans only)
		if (rough) return true;
		for (int k = 0; k < sp.count; k++) {
			if (!(sp.bits & (1 << (sp.count - 1 - k)))) continue;
			const int from = std::max({sp.hpos - 32 + k * sp.width, a, 0});
			const int to = std::min({sp.hpos - 32 + (k + 1) * sp.width, b, kClocks});
			if (from < to) return true;
		}
		return false;
	}
	// Two sprites: their spans first, then bit by bit where they meet.
	static bool spritesMeet(const Span &p, const Span &q, bool &exact) {
		const int a = std::max({p.left, q.left, 0}), b = std::min({p.right, q.right, kClocks});
		if (a >= b) return false;
		exact = true;
		static const bool rough = std::getenv("RR_ROUGH_PAIRS") != nullptr;   // (the same)
		if (rough) return true;
		return spriteMask(p.bits, p.count, uint8_t(p.hpos), p.width == 4 ? 3 : p.width == 2 ? 1 : 0) &
			spriteMask(q.bits, q.count, uint8_t(q.hpos), q.width == 4 ? 3 : q.width == 2 ? 1 : 0);
	}

	// The scanline's playfield as runs of one colour (1 bank, 2 edge, 3 water): from the line
	// without decoration, 5-9 runs (docs/DLI_REMOVAL.md).
	struct Run { int a, b, colour; };
	static int lineRuns(const uint8_t *src, Run out[kClocks]) {
		int n = 0, prev = -1;
		for (int p = 0; p < kClocks; p++) {
			const int v = (src[p >> 2] >> (6 - 2 * (p & 3))) & 3;
			if (v != prev) {
				if (prev > 0) out[n - 1].b = p;     // the run going on ends here
				if (v) out[n++] = Run{p, kClocks, v};
				prev = v;
			}
		}
		return n;
	}

	struct Stats {
		long lines = 0, runTests = 0, runsInside = 0, runsInsideMissed = 0, pairs = 0, pairsMissed = 0;
		~Stats() {
			if (std::getenv("RR_STATS"))
				std::fprintf(stderr, "collisions: %ld lines; sprite against a run %ld times, %ld of them a run between the first and last bit"
					" (%ld of those in a hole); sprites' spans met %ld times (%ld of them not their bits)\n",
					lines, runTests, runsInside, runsInsideMissed, pairs, pairsMissed);
		}
	};
	static Stats &stats() { static Stats s; return s; }

public:
	// The collision registers of one scanline (bits as above).
	struct Regs {
		uint8_t mpf[4] = {}, ppf[4] = {}, mpl[4] = {}, ppl[4] = {};
		void add(const Regs &o) {
			for (int n = 0; n < 4; n++) { mpf[n] |= o.mpf[n]; ppf[n] |= o.ppf[n]; mpl[n] |= o.mpl[n]; ppl[n] |= o.ppl[n]; }
		}
	};
	// What GTIA draws on a scanline: the players' and missiles' graphics and registers.
	struct Sprites {
		uint8_t p[4] = {}, m = 0;
		uint8_t hposP[4] = {}, hposM[4] = {}, sizeP[4] = {}, sizeM = 0;
	};
	// GTIA's collision detection for one scanline, by spans: each sprite's first and last set
	// bit against the playfield's runs (`playfield`: the line, nullptr outside the river) and
	// against the other sprites, bit by bit only where a run lies between them or two spans meet.
	static Regs collideLine(const Sprites &sp, const Game::Runs *playfield) {
		Regs r;
		Span pl[4], ms[4];
		bool any = false;
		for (int n = 0; n < 4; n++) {
			pl[n] = spriteSpan(sp.p[n], 8, sp.hposP[n], sp.sizeP[n]);
			const uint8_t mb = uint8_t((sp.m >> (2 * n)) & 3);
			ms[n] = spriteSpan(mb, 2, sp.hposM[n], uint8_t(sp.sizeM >> (2 * n)));
			any |= pl[n].on() || ms[n].on();
		}
		if (!any) return r;
		Stats &st = stats();
		st.lines++;
		const int nRuns = playfield ? playfield->n : 0;
		auto playfieldBits = [&](const Span &x) {
			uint8_t bits = 0;
			for (int i = 0; i < nRuns; i++) {
				const uint8_t bit = uint8_t(1 << (playfield->colour[i] - 1));
				if (bits & bit) continue;
				bool exact = false;
				const bool hit = spanHits(x, playfield->a[i], playfield->b[i], exact);
				st.runTests++;
				if (exact) { st.runsInside++; if (!hit) st.runsInsideMissed++; }
				if (hit) bits |= bit;
			}
			return bits;
		};
		auto meet = [&](const Span &a, const Span &b) {
			bool exact = false;
			const bool hit = spritesMeet(a, b, exact);
			if (exact) { st.pairs++; if (!hit) st.pairsMissed++; }
			return hit;
		};
		for (int n = 0; n < 4; n++) {
			if (pl[n].on()) {
				r.ppf[n] |= playfieldBits(pl[n]);
				for (int k = 0; k < 4; k++)
					if (k != n && pl[k].on() && meet(pl[n], pl[k])) r.ppl[n] |= uint8_t(1 << k);
			}
			if (ms[n].on()) {
				r.mpf[n] |= playfieldBits(ms[n]);
				for (int k = 0; k < 4; k++)
					if (pl[k].on() && meet(ms[n], pl[k])) r.mpl[n] |= uint8_t(1 << k);
			}
		}
		return r;
	}
	// One sprite (`count` graphics bits: 8 a player, 2 a missile) against a scanline's
	// playfield: its colours under the sprite (1 bank, 2 edge, 4 water).
	static uint8_t playfieldHits(uint8_t bits, int count, uint8_t hpos, uint8_t size, const uint8_t *playfield) {
		if (!bits || !playfield) return 0;
		const Span sp = spriteSpan(bits, count, hpos, size);
		Run runs[kClocks];
		const int n = lineRuns(playfield, runs);
		uint8_t hits = 0;
		for (int i = 0; i < n; i++) {
			bool exact = false;
			if (spanHits(sp, runs[i].a, runs[i].b, exact)) hits |= uint8_t(1 << (runs[i].colour - 1));
		}
		return hits;
	}
	static uint8_t playfieldHits(uint8_t bits, int count, uint8_t hpos, uint8_t size, const Game::Runs *runs) {
		if (!bits || !runs) return 0;
		const Span sp = spriteSpan(bits, count, hpos, size);
		uint8_t hits = 0;
		for (int i = 0; i < runs->n; i++) {
			const uint8_t bit = uint8_t(1 << (runs->colour[i] - 1));
			if (hits & bit) continue;
			bool exact = false;
			if (spanHits(sp, runs->a[i], runs->b[i], exact)) hits |= bit;
		}
		return hits;
	}
	// Two sprites on a scanline: their spans, then bit by bit.
	static bool spritesTouch(uint8_t bitsA, int countA, uint8_t hposA, uint8_t sizeA, uint8_t bitsB, int countB, uint8_t hposB, uint8_t sizeB) {
		if (!bitsA || !bitsB) return false;
		bool exact = false;
		return spritesMeet(spriteSpan(bitsA, countA, hposA, sizeA), spriteSpan(bitsB, countB, hposB, sizeB), exact);
	}
private:
	void collideSpans() {
		Sprites sp;
		for (int n = 0; n < 4; n++) { sp.p[n] = p_[n]; sp.hposP[n] = hposP[n]; sp.hposM[n] = hposM[n]; sp.sizeP[n] = sizeP[n]; }
		sp.m = m_;
		sp.sizeM = sizeM;
		const int row = river(line_);
		const Game::Runs *pf = row >= 0 && row < kRiverLines && size_t(row) < game_.runLines.size() ? &game_.runLines[size_t(row)] : nullptr;
		const Regs r = collideLine(sp, pf);
		for (int n = 0; n < 4; n++) { mpf[n] |= r.mpf[n]; ppf[n] |= r.ppf[n]; mpl[n] |= r.mpl[n]; ppl[n] |= r.ppl[n]; }
	}

	// GTIA's collision detection for the scanline being drawn: which sprites overlap the
	// playfield colours and each other. RR_COLLIDE=masks: pixel masks of the whole line (as
	// before), otherwise by spans (collideSpans).
	void collide() {
		static const bool masks = std::getenv("RR_COLLIDE") && !std::strcmp(std::getenv("RR_COLLIDE"), "masks");
		if (!masks) { collideSpans(); return; }
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
		static const bool debug = std::getenv("RR_BANK_DEBUG") != nullptr;
		if (debug && row >= 0 && row < kRiverLines && size_t(row) < game_.screen.size()) {
			// where the lines with and without decoration give different collisions
			Mask full[3];
			const uint8_t *src = game_.screen[size_t(row)].data();
			for (int p = 0; p < kClocks; p++) {
				const int value = (src[p >> 2] >> (6 - 2 * (p & 3))) & 3;
				if (value) full[value - 1].w[p >> 6] |= uint64_t(1) << (p & 63);
			}
			auto fullBits = [&](const Mask &m) {
				return uint8_t((m & full[0] ? 1 : 0) | (m & full[1] ? 2 : 0) | (m & full[2] ? 4 : 0));
			};
			for (int n = 0; n < 4; n++) {
				const uint8_t a = pl[n].empty() ? 0 : uint8_t(fullBits(pl[n]) & 3), b = pl[n].empty() ? 0 : uint8_t(playfieldBits(pl[n]) & 3);
				const uint8_t c = ms[n].empty() ? 0 : uint8_t(fullBits(ms[n]) & 3), d = ms[n].empty() ? 0 : uint8_t(playfieldBits(ms[n]) & 3);
				if (a != b || c != d) {
					std::fprintf(stderr, "frame %u line %d row %d kind %d: %s%d full %d plain %d (hpos %d)\n  ", game_.g.frameCount, line_, row, g_debugKind,
						a != b ? "P" : "M", n, a != b ? a : c, a != b ? b : d, a != b ? hposP[n] : hposM[n]);
					for (int p = 0; p < kClocks; p++) std::fputc(".B=~"[(src[p >> 2] >> (6 - 2 * (p & 3))) & 3], stderr);
					std::fputc('\n', stderr);
				}
			}
		}
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

void Game::displayBeam() {
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
	Beam beam(*this, bankRuns ? bankLines : screen, 23, 76, lms, pal_, record);
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
		g_debugKind = current < kSlots ? s.slot[current].kind : -1;
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

namespace {
struct SimpleStats {
	long m0pf = 0, m0pfLate = 0, clears = 0, clearsLate = 0;
	~SimpleStats() {
		if (std::getenv("RR_STATS"))
			std::fprintf(stderr, "displaySimple: M0PF read after the draw %ld of %ld times, HITCLR %ld of %ld\n", m0pfLate, m0pf, clearsLate, clears);
	}
};
SimpleStats &simpleStats() { static SimpleStats s; return s; }
}  // namespace

void Game::displaySimple() {
	RiverState &s = gen.s;
	scan = {};
	g.hit = Collisions();
	constexpr int kLines = 262;
	constexpr int kFirstLine = 24;   // the first scanline the DLI sees drawn (it starts on 23, past its draw)
	constexpr int kLastLine = 190;
	// registers by scanline: a write from a scanline on
	uint8_t hposP3[kLines], sizeP3[kLines], colourP3[kLines], bank[kLines];
	// (each write marks its first scanline; the values are carried down once, at the end)
	bool setP3x[kLines] = {}, setSize[kLines] = {}, setColour[kLines] = {}, setBank[kLines] = {};
	auto from = [&](uint8_t *a, int line, uint8_t v) {
		if (line < 0 || line >= kLines) return;
		a[line] = v;
		(a == hposP3 ? setP3x : a == sizeP3 ? setSize : a == colourP3 ? setColour : setBank)[line] = true;
	};
	from(hposP3, 0, 0);                        // VBI (B411)
	from(sizeP3, 0, s.slot[kSlots - 1].size);
	from(colourP3, 0, 0);
	from(bank, 0, g.colourBank);

	// Where the CPU is after a WSYNC is fixed; only two accesses can fall on either side of a
	// scanline's draw (cycle 28), by the cycles its start takes (DMA, display list) and by
	// the hits stored just before them.
	bool lms[kRiverLines] = {};
	{
		const int top = g.screenTop;
		const int blockEnd = (top >> 8) >= 0x30 ? 0x3F00 : 0x2F00;
		const int k1 = (blockEnd - top) / kLineBytes;
		lms[0] = true;
		if (k1 < kRiverLines) lms[k1] = true;
		if (k1 + 80 < kRiverLines) lms[k1 + 80] = true;
	}
	auto startCycles = [&](int l) {
		int c = 5;
		const int r = l - kRiverFirstLine;
		if (r >= 0 && r < kRiverLines) c += lms[r] ? 3 : 1;
		else if (l == 24 || l == kRiverFirstLine + kRiverLines) c += 1;
		else if (l == kPanelFirstLine[pal_]) c += 3;
		return c;
	};
	// an access `offset` cycles after a WSYNC (cycle 106 of the line before `l`): after l's draw?
	auto afterDraw = [&](int l, int offset) { return offset - (kLineCycles - kWsyncCycle) + startCycles(l) >= kDrawCycle; };

	// The collision reads and clears, in the DLI's order: what each sees is every scanline
	// from the last HITCLR's to `through`.
	enum What { M2PL, P3PL, P2PL, P3PF, M0PL, M0PF, M0PF_END, P1PF, P2PF, M2PF, CLEAR, FLAGS_END };
	struct Read { What what; int through; int slot; };
	std::vector<Read> reads;
	reads.reserve(256);
	std::vector<std::pair<int, int>> objectRows;   // the scanlines of each object's rows
	auto slotY = [&](int n) -> uint8_t { return n < kSlots ? s.slot[n].y : 0xFF; };
	int counter = 0x5D;
	int slot = s.firstSlot;
	int current = slot;
	bool endedAtB664 = false;
	auto line = [&](int c) { return c - 69; };   // the scanline being drawn while X = c
	for (;;) {
		current = slot;
		bool end = false;
		while (counter < slotY(slot)) {            // B567
			counter = (counter + 1) & 0xFF;
			if (!counter) { end = true; break; }
		}
		if (end) break;
		const int start = counter;
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		const Slot sl = slot < kSlots ? s.slot[slot] : Slot{};
		from(hposP3, line(counter), sl.x);          // before the line's draw
		g.pm.at(uint16_t(kPlayer3Rows + counter)) = 0;
		g.pm.at(uint16_t(kPlayer3Rows + counter + 1)) = 0;
		const uint8_t flags = sl.flags;
		const bool facingLeft = flags & slotflag::kFacingLeft;
		const uint16_t shapes = rom_.shapeAddress(sl.kind + (facingLeft ? 0x10 : 0));
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		from(sizeP3, line(counter) + 1, sl.size);    // after it
		const uint16_t colours = rom_.coloursAddress(sl.type);
		int row = uint8_t(sl.y + rom_.objectHeight(sl.type) - start);
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		if (row >= 0x80) { slot = current + 1; continue; }
		bool wrapped = false;
		const int firstRowLine = line(counter) + 1;
		objectRows.push_back({firstRowLine, firstRowLine + row});
		for (;;) {                                  // B5CB: a row a scanline
			g.pm.at(uint16_t(kPlayer3Rows + counter)) = rom_.at(uint16_t(shapes + row));
			if (flags & slotflag::kBridge) from(bank, line(counter) + 1, rom_.roadColour(row));
			from(colourP3, line(counter) + 1, rom_.at(uint16_t(colours + row)));
			counter = (counter + 1) & 0xFF;
			if (!counter) { wrapped = true; objectRows.back().second = 0xFF - 68; break; }   // (the last row: X = $FF)
			if (--row < 0) break;
		}
		if (wrapped) break;
		const int e = counter;                      // B5E8
		const bool bridge = flags & slotflag::kBridge;
		reads.push_back({M2PL, line(e) - (bridge ? 0 : 1), current});
		if (bridge) from(bank, line(e) + 1, s.sectionBankColour);
		else reads.push_back({P3PL, line(e), current});
		counter = (counter + 1) & 0xFF;             // B60A
		if (!counter) break;
		reads.push_back({P2PL, line(counter) - 1, current});
		reads.push_back({P3PF, line(counter), current});
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		reads.push_back({M0PL, line(counter) - 1, current});
		reads.push_back({M0PF, line(counter), current});        // (on this line: before or after its draw)
		reads.push_back({P1PF, line(counter), current});
		reads.push_back({P2PF, line(counter), current});
		reads.push_back({M2PF, line(counter), current});
		reads.push_back({CLEAR, line(counter) + 1, current});   // HITCLR on the next line, before or after its draw
		slot = current + 1;
		counter = (counter + 1) & 0xFF;
		if (!counter) { endedAtB664 = true; break; }
	}
	// B66B: the panel part
	const int endLine = 187;
	const bool panelReads = current < kSlots && !endedAtB664;
	if (panelReads) {
		reads.push_back({FLAGS_END, endLine + 1, current});
		reads.push_back({M0PL, endLine + 2, current});
		reads.push_back({M0PF_END, endLine + 2, current});
		reads.push_back({M2PF, endLine + 2, current});
		reads.push_back({P2PF, endLine + 2, current});
		reads.push_back({P2PL, endLine + 2, current});
		reads.push_back({P3PL, endLine + 2, current});
		reads.push_back({P1PF, endLine + 2, current});
		reads.push_back({M2PL, endLine + 3, current});
	}

	for (int l = 1; l < kLines; l++) {
		if (!setP3x[l]) hposP3[l] = hposP3[l - 1];
		if (!setSize[l]) sizeP3[l] = sizeP3[l - 1];
		if (!setColour[l]) colourP3[l] = colourP3[l - 1];
		if (!setBank[l]) bank[l] = bank[l - 1];
	}
	// What GTIA draws on each scanline (P3: the rows the DLI wrote, its position and size by
	// scanline; the other sprites as the main loop left them)
	auto spriteAt = [&](int sprite, int l, uint8_t &bits, int &count, uint8_t &hpos, uint8_t &size) {
		const bool panel = l >= endLine + 1;
		count = sprite < 4 ? 8 : 2;
		size = 0;
		switch (sprite) {
		case 0: bits = g.pm.player(0, l); hpos = panel ? 0 : g.shellX; break;
		case 1: bits = g.pm.player(1, l); hpos = panel ? 0 : s.tankX; break;
		case 2: bits = g.pm.player(2, l); hpos = g.planeX; break;
		case 3: bits = g.pm.player(3, l); hpos = hposP3[l]; size = sizeP3[l]; break;
		case 4: bits = uint8_t(g.pm.missiles(l) & 3); hpos = panel ? 0 : s.missileX; size = 1; break;      // M0 (SIZEM: double)
		default: bits = uint8_t((g.pm.missiles(l) >> 4) & 3); hpos = uint8_t(g.planeX + 4); break;    // M2
		}
	};
	for (int l = kFirstLine; l <= kLastLine; l++) {
		if (l > endLine + (panelReads ? 3 : 1)) break;   // (what the DLI waits for is drawn)
		Scanline &sc = scan[size_t(l)];
		sc.p3 = g.pm.player(3, l);
		sc.p3x = hposP3[l];
		sc.p3size = sizeP3[l];
		sc.p3colour = colourP3[l];
		sc.bankColour = bank[l];
	}
	// Each sprite's box: the scanlines it has graphics on and the clocks it can cover, from
	// where the game put it: the shell (16 rows from shellY), the tank (11 from tankY), the
	// plane (14 from $AA), the helicopter's missile (row missileY), the bullet (8 rows below
	// bulletY), each object's rows as the DLI wrote them. RR_BOXES=scan: found by looking down
	// player-missile memory instead.
	struct Box { int sprite, y0, y1, x0, x1; };
	std::vector<Box> boxes;
	static const bool scanBoxes = std::getenv("RR_BOXES") && !std::strcmp(std::getenv("RR_BOXES"), "scan");
	auto addBox = [&](int sprite, int y0, int y1) {
		y0 = std::max(y0, kFirstLine);
		y1 = std::min(y1, sprite == 2 || sprite == 3 || sprite == 5 ? kLastLine : endLine);   // (at x 0 below)
		if (y0 > y1) return;
		uint8_t bits, hpos, size; int count;
		spriteAt(sprite, y0, bits, count, hpos, size);
		const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
		boxes.push_back({sprite, y0, y1, hpos - 32, hpos - 32 + count * w});
	};
	if (!scanBoxes) {
		addBox(0, s.shellY, s.shellY + 15);
		addBox(1, s.tankY, s.tankY + 10);
		addBox(2, 0xAA, 0xAA + 13);
		for (const auto &r : objectRows) addBox(3, r.first, r.second);
		if (s.missileY >= 0x1C) addBox(4, s.missileY, s.missileY);
		if (g.bulletY > 1) {
			const int row = std::max(g.bulletY - 1, 0x1B);
			addBox(5, row + 1, row + 8);
		}
	} else {
		for (int sprite = 0; sprite < 6; sprite++) {
			for (int l = kFirstLine; l <= kLastLine; l++) {
				uint8_t bits, hpos, size; int count;
				spriteAt(sprite, l, bits, count, hpos, size);
				if (!bits) continue;
				const int w = (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1;
				const int x0 = hpos - 32, x1 = hpos - 32 + count * w;
				Box *b = boxes.empty() ? nullptr : &boxes.back();
				const bool joins = b && b->sprite == sprite && b->y1 == l - 1 &&
					(sprite != 3 || (hposP3[l] == hposP3[l - 1] && sizeP3[l] == sizeP3[l - 1]));
				if (joins) { b->y1 = l; b->x0 = std::min(b->x0, x0); b->x1 = std::max(b->x1, x1); }
				else boxes.push_back({sprite, l, l, x0, x1});
			}
		}
	}
	// The collisions the DLI reads, on the scanlines they happen: each sprite's rows against
	// the bank's runs, and the pairs it reads where their boxes meet, bit by bit.
	std::vector<Beam::Regs> regs(kLines);
	auto playfield = [&](int l) -> const Runs * {
		const int r = l - kRiverFirstLine;
		return r >= 0 && r < kRiverLines && size_t(r) < runLines.size() ? &runLines[size_t(r)] : nullptr;
	};
	for (const Box &b : boxes) {
		if (b.sprite == 0) continue;               // (P0PF is not read)
		for (int l = b.y0; l <= b.y1; l++) {
			uint8_t bits, hpos, size; int count;
			spriteAt(b.sprite, l, bits, count, hpos, size);
			const uint8_t pf = Beam::playfieldHits(bits, count, hpos, size, playfield(l));
			Beam::Regs &r = regs[size_t(l)];
			if (b.sprite < 4) r.ppf[b.sprite] |= pf;
			else r.mpf[b.sprite == 4 ? 0 : 2] |= pf;
		}
	}
	// the pairs read: M2 (bullet) with P3, P3 with P0 (shell), P2 (plane) with P0 and P3,
	// M0 (missile) with P2
	const int pairs[][2] = {{5, 3}, {3, 0}, {2, 0}, {2, 3}, {4, 2}};
	for (const auto &pr : pairs)
		for (const Box &a : boxes) {
			if (a.sprite != pr[0]) continue;
			for (const Box &b : boxes) {
				if (b.sprite != pr[1]) continue;
				if (a.y1 < b.y0 || b.y1 < a.y0 || a.x1 <= b.x0 || b.x1 <= a.x0) continue;
				for (int l = std::max(a.y0, b.y0); l <= std::min(a.y1, b.y1); l++) {
					uint8_t ba, ha, sa, bb, hb, sb; int ca, cb;
					spriteAt(a.sprite, l, ba, ca, ha, sa);
					spriteAt(b.sprite, l, bb, cb, hb, sb);
					if (!Beam::spritesTouch(ba, ca, ha, sa, bb, cb, hb, sb)) continue;
					Beam::Regs &r = regs[size_t(l)];
					if (a.sprite < 4) r.ppl[a.sprite] |= uint8_t(1 << b.sprite);
					else r.mpl[a.sprite - 4 == 0 ? 0 : 2] |= uint8_t(1 << b.sprite);
				}
			}
		}
	int cleared = kFirstLine;
	// the reads of an object see more and more scanlines: the OR grows from one to the next
	Beam::Regs acc;
	int accTo = cleared - 1;
	auto seen = [&](int through) {
		through = std::min(through, kLastLine);
		if (through < accTo) {                     // (never: the reads go down the screen)
			Beam::Regs r;
			for (int l = cleared; l <= through; l++) r.add(regs[size_t(l)]);
			return r;
		}
		for (; accTo < through; accTo++) acc.add(regs[size_t(accTo + 1)]);
		return acc;
	};
	bool m0plHit = false;
	int stored = 0;   // B645-B65E: hits stored before HITCLR
	for (const Read &rd : reads) {
		int through = rd.through;
		if (rd.what == M0PF) {
			through = afterDraw(rd.through, 28 + (m0plHit ? 2 : 0)) ? rd.through : rd.through - 1;   // B631
			simpleStats().m0pf++;
			if (through == rd.through) simpleStats().m0pfLate++;
		}
		const Beam::Regs r = seen(through);
		const uint8_t c = uint8_t(rd.slot);
		switch (rd.what) {
		case M2PL: if (r.mpl[2] & 0x08) g.hit.bulletHitObject = c; break;
		case P3PL: if (r.ppl[3] & 0x01) g.hit.shellHitObject = c; break;
		case P2PL:
			if (r.ppl[2] & 0x01) g.hit.planeHitShell = c;
			if (r.ppl[2] & 0x08) g.hit.planeHitObject = c;
			break;
		case P3PF:
		case FLAGS_END:
			if (rd.slot < kSlots) {
				Slot &obj = s.slot[rd.slot];
				obj.flags = uint8_t((obj.flags & ~slotflag::kTouchingEdge) | ((r.ppf[3] >> 1) & slotflag::kTouchingEdge));
			}
			break;
		case M0PL: m0plHit = r.mpl[0] & 0x04; if (m0plHit) g.hit.missileHitPlane = c; break;
		case M0PF: case M0PF_END: if (r.mpf[0] & 0x03) g.hit.missileHitBank = c; break;
		case P1PF: stored = 0; if (r.ppf[1] & 0x02) { g.hit.tankAtEdge = c; stored++; } break;
		case P2PF: if (r.ppf[2] & 0x01) { g.hit.planeHitBank = c; stored++; } break;
		case M2PF: if (r.mpf[2] & 0x03) { g.hit.bulletHitBank = c; stored++; } break;
		case CLEAR:
			cleared = afterDraw(rd.through, 23 + 2 * stored) ? rd.through + 1 : rd.through;   // B65F
			acc = Beam::Regs();
			accTo = cleared - 1;
			simpleStats().clears++;
			if (cleared != rd.through) simpleStats().clearsLate++;
			break;
		}
	}
}

void Game::display() {
	static const char *mode = std::getenv("RR_DISPLAY");
	// RR_TIME: the time display() takes, printed at the end
	struct Timer {
		bool on = std::getenv("RR_TIME") != nullptr;
		double total = 0;
		long frames = 0;
		~Timer() { if (on && frames) std::fprintf(stderr, "display: %ld frames, %.2f us a frame\n", frames, total * 1e6 / double(frames)); }
	};
	static Timer timer;
	if (timer.on && (!mode || !std::strcmp(mode, "simple") || !std::strcmp(mode, "beam"))) {
		const auto t0 = std::chrono::steady_clock::now();
		if (mode && !std::strcmp(mode, "beam")) displayBeam(); else displaySimple();
		timer.total += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
		timer.frames++;
		return;
	}
	if (!mode || !std::strcmp(mode, "simple")) { displaySimple(); return; }
	if (!std::strcmp(mode, "beam")) { displayBeam(); return; }
	// check: both, from the same state; the beam's result is kept
	const GameState g0 = g;
	const RiverState s0 = gen.s;
	displaySimple();
	const GameState g1 = g;
	const RiverState s1 = gen.s;
	const auto scan1 = scan;
	g = g0;
	gen.s = s0;
	displayBeam();
	std::string d;
	auto cmp = [&](const char *name, int a, int b) { if (a != b) d += std::string(" ") + name + " beam=" + std::to_string(a) + " simple=" + std::to_string(b); };
	cmp("bulletHitBank", g.hit.bulletHitBank, g1.hit.bulletHitBank);
	cmp("planeHitBank", g.hit.planeHitBank, g1.hit.planeHitBank);
	cmp("bulletHitObject", g.hit.bulletHitObject, g1.hit.bulletHitObject);
	cmp("planeHitObject", g.hit.planeHitObject, g1.hit.planeHitObject);
	cmp("planeHitShell", g.hit.planeHitShell, g1.hit.planeHitShell);
	cmp("shellHitObject", g.hit.shellHitObject, g1.hit.shellHitObject);
	cmp("tankAtEdge", g.hit.tankAtEdge, g1.hit.tankAtEdge);
	cmp("missileHitPlane", g.hit.missileHitPlane, g1.hit.missileHitPlane);
	cmp("missileHitBank", g.hit.missileHitBank, g1.hit.missileHitBank);
	for (int i = 0; i < kSlots; i++) cmp(("flags" + std::to_string(i)).c_str(), gen.s.slot[i].flags, s1.slot[i].flags);
	for (size_t i = 0; i < g.pm.bytes.size(); i++)
		if (g.pm.bytes[i] != g1.pm.bytes[i]) { cmp(("pm" + std::to_string(i)).c_str(), g.pm.bytes[i], g1.pm.bytes[i]); break; }
	for (size_t l = 0; l < scan.size(); l++) {
		const Scanline &a = scan[l], &b = scan1[l];
		if (a.p3 != b.p3 || a.p3x != b.p3x || a.p3size != b.p3size || a.p3colour != b.p3colour || a.bankColour != b.bankColour) {
			d += " scan" + std::to_string(l) + " beam=" + std::to_string(a.p3) + "," + std::to_string(a.p3x) + "," + std::to_string(a.p3size) + "," +
				std::to_string(a.p3colour) + "," + std::to_string(a.bankColour) + " simple=" + std::to_string(b.p3) + "," + std::to_string(b.p3x) + "," +
				std::to_string(b.p3size) + "," + std::to_string(b.p3colour) + "," + std::to_string(b.bankColour);
			break;
		}
	}
	if (!d.empty()) std::fprintf(stderr, "display frame %u:%s\n", g.frameCount, d.c_str());
}

}  // namespace rr
