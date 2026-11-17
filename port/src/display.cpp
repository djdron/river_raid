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
	// (`wanted`: the colour bits looked for; the search stops when all are found)
	static uint8_t playfieldHits(uint8_t bits, int count, uint8_t hpos, uint8_t size, const Game::Runs *runs, uint8_t wanted = 7) {
		if (!bits || !runs) return 0;
		const Span sp = spriteSpan(bits, count, hpos, size);
		uint8_t hits = 0;
		for (int i = 0; i < runs->n && hits != wanted; i++) {
			const uint8_t bit = uint8_t(1 << (runs->colour[i] - 1));
			if ((hits & bit) || !(wanted & bit)) continue;
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
	long m0pf = 0, m0pfLate = 0, clears = 0, clearsLate = 0, frames = 0, slips = 0;
	~SimpleStats() {
		if (std::getenv("RR_STATS"))
			std::fprintf(stderr, "displaySimple: M0PF read after the draw %ld of %ld times, HITCLR %ld of %ld; %ld scanlines skipped in %ld frames\n",
				m0pfLate, m0pf, clearsLate, clears, slips, frames);
	}
};
SimpleStats &simpleStats() { static SimpleStats s; return s; }
}  // namespace

void Game::displaySimple() {
	RiverState &s = gen.s;
	SimpleStats &stats = simpleStats();
	stats.frames++;
	scan = {};
	g.hit = Collisions();
	constexpr int kLines = 262;
	constexpr int kFirstLine = 24;   // the first scanline the DLI sees drawn (it starts on 23, past its draw)

	// The cycles a scanline's start takes (player-missile DMA, the display list instruction:
	// 3 more with a screen address) and the ones its draw takes (refresh, the screen's bytes).
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
	auto drawCycles = [&](int l) {
		const int r = l - kRiverFirstLine;
		return kRefreshCycles + (r >= 0 && r < kRiverLines ? 48 : l == kPanelFirstLine[pal_] ? 20 : 0);
	};
	// The CPU leaves a WSYNC at cycle 106 of a scanline. An access `offset` cycles later, on
	// the next scanline l: after l's draw (cycle 28)?
	auto afterDraw = [&](int l, int offset) { return offset - (kLineCycles - kWsyncCycle) + startCycles(l) >= kDrawCycle; };
	// The code from a WSYNC to the next one, `cycles` long with that STA, run on scanline l:
	// past cycle 106 of l (its draw taken out), it waits for the end of the next one: the DLI
	// is a scanline later from then on.
	int slip = 0;
	auto chunk = [&](int l, int cycles) {
		if (cycles - (kLineCycles - kWsyncCycle) + startCycles(l) + drawCycles(l) > kWsyncCycle) { slip++; stats.slips++; }
	};

	// Registers by scanline: each write marks its first scanline (they come in time order);
	// the values are carried down to a scanline before anything looks at it.
	uint8_t hposP3[kLines], sizeP3[kLines], colourP3[kLines], bank[kLines];
	bool setP3x[kLines] = {}, setSize[kLines] = {}, setColour[kLines] = {}, setBank[kLines] = {};
	auto from = [&](uint8_t *a, bool *set, int l, uint8_t v) {
		if (l < 0 || l >= kLines) return;
		a[l] = v;
		set[l] = true;
	};
	from(hposP3, setP3x, 0, 0);                         // VBI (B411)
	from(sizeP3, setSize, 0, s.slot[kSlots - 1].size);
	from(colourP3, setColour, 0, 0);
	from(bank, setBank, 0, g.colourBank);
	int filled = 0;
	auto fill = [&](int upto) {
		for (; filled < std::min(upto, kLines - 1); ) {
			const int l = ++filled;
			if (!setP3x[l]) hposP3[l] = hposP3[l - 1];
			if (!setSize[l]) sizeP3[l] = sizeP3[l - 1];
			if (!setColour[l]) colourP3[l] = colourP3[l - 1];
			if (!setBank[l]) bank[l] = bank[l - 1];
		}
	};
	int endLine = 187;   // the river's last scanline (X wraps to 0 on the next): 187 + slip
	// Player 3's memory as GTIA fetches it at a scanline's start: a row the DLI writes on that
	// scanline or later (once it has skipped one) shows there only in the next frame.
	uint8_t p3before[256];
	int p3written[256];
	for (int i = 0; i < 256; i++) { p3before[i] = g.pm.player(3, i); p3written[i] = -1; }
	auto writeP3 = [&](int c, uint8_t v, int when) {
		const int i = (c - 0x44) & 0xFF;
		if (p3written[i] < 0) p3written[i] = when;
		g.pm.at(uint16_t(kPlayer3Rows + c)) = v;
	};
	auto p3At = [&](int l) {
		const int i = l & 0xFF;
		return p3written[i] >= l ? p3before[i] : g.pm.player(3, l);
	};

	// What GTIA draws: P3 from player-missile memory with the DLI's registers, the others
	// as the main loop left them (P0, P1 and M0 at x 0 from the panel on).
	auto spriteAt = [&](int sprite, int l, uint8_t &bits, int &count, uint8_t &hpos, uint8_t &size) {
		const bool panel = l >= endLine + 1;
		count = sprite < 4 ? 8 : 2;
		size = 0;
		switch (sprite) {
		case 0: bits = g.pm.player(0, l); hpos = panel ? 0 : g.shellX; break;
		case 1: bits = g.pm.player(1, l); hpos = panel ? 0 : s.tankX; break;
		case 2: bits = g.pm.player(2, l); hpos = g.planeX; break;
		case 3: bits = p3At(l); hpos = hposP3[l]; size = sizeP3[l]; break;
		case 4: bits = uint8_t(g.pm.missiles(l) & 3); hpos = panel ? 0 : s.missileX; size = 1; break;      // M0 (SIZEM: double)
		default: bits = uint8_t((g.pm.missiles(l) >> 4) & 3); hpos = uint8_t(g.planeX + 4); break;    // M2
		}
	};
	// The collisions without the scanlines' registers (docs/DLI_REMOVAL.md). Each sprite's rows
	// are where the game put them: the shell 16 from shellY, the tank 11 from tankY, the plane
	// 14 from $AA, the missile the row missileY, the bullet 8 below bulletY (nothing of them is
	// anywhere else). The scanlines where each collision the DLI reads happens are worked out
	// once, from those rows against the bank's runs and against each other, bit by bit; an
	// object's against its rows where the loop writes them. A read sees the scanlines from the
	// last HITCLR's to its own: so a collision counts if its scanline is in there.
	auto pfAt = [&](int l) -> const Runs * {
		const int r = l - kRiverFirstLine;
		return r >= 0 && r < kRiverLines && size_t(r) < runLines.size() ? &runLines[size_t(r)] : nullptr;
	};
	struct Rows { int y0 = 0, y1 = -1; };   // scanlines y0..y1
	auto box = [&](int y0, int y1) { Rows b; b.y0 = std::max(y0, kFirstLine); b.y1 = std::min(y1, kLines - 1); return b; };
	const Rows shellBox = box(s.shellY, s.shellY + 15), tankBox = box(s.tankY, s.tankY + 10), planeBox = box(0xAA, 0xAA + 13);
	Rows missileBox, bulletBox;
	if (s.missileY >= 0x1C) missileBox = box(s.missileY, s.missileY);
	if (g.bulletY > 1) {
		const int row = std::max(g.bulletY - 1, 0x1B);
		bulletBox = box(row + 1, row + 8);
	}
	// only what the DLI reads: P1PF the edge, P2PF the bank, M0PF and M2PF either; P2 with P0, M0 with P2
	std::vector<int> tankEdge, planeBank, missileBank, bulletBank, planeShell, missilePlane;
	auto pfLines = [&](int sprite, const Rows &b, uint8_t want, std::vector<int> &out) {
		for (int l = b.y0; l <= b.y1; l++) {
			uint8_t bits, hpos, size;
			int count;
			spriteAt(sprite, l, bits, count, hpos, size);
			if (bits && (Beam::playfieldHits(bits, count, hpos, size, pfAt(l), want) & want)) out.push_back(l);
		}
	};
	auto pairLines = [&](int a, const Rows &ba, int b, const Rows &bb, std::vector<int> &out) {
		for (int l = std::max(ba.y0, bb.y0); l <= std::min(ba.y1, bb.y1); l++) {
			uint8_t bitsA, hposA, sizeA, bitsB, hposB, sizeB;
			int countA, countB;
			spriteAt(a, l, bitsA, countA, hposA, sizeA);
			spriteAt(b, l, bitsB, countB, hposB, sizeB);
			if (Beam::spritesTouch(bitsA, countA, hposA, sizeA, bitsB, countB, hposB, sizeB)) out.push_back(l);
		}
	};
	pfLines(1, tankBox, 2, tankEdge);
	pfLines(2, planeBox, 1, planeBank);
	pfLines(4, missileBox, 3, missileBank);
	pfLines(5, bulletBox, 3, bulletBank);
	pairLines(2, planeBox, 0, shellBox, planeShell);
	pairLines(4, missileBox, 2, planeBox, missilePlane);
	int cleared = kFirstLine;   // the last HITCLR's scanline
	auto seenIn = [&](const std::vector<int> &v, int through) {
		for (int l : v)
			if (l >= cleared && l <= through) return true;
		return false;
	};
	auto clearAt = [&](int l) { cleared = l; };
	// Each object's rows as the loop writes them. The edge (P3PF) only for the kinds whose bit
	// moveObjects reads: helicopters, balloons, ships.
	struct ObjectRows { int slot, first; uint8_t x, size; bool edge; std::vector<uint8_t> rows; };
	std::vector<ObjectRows> objects;
	objects.reserve(kSlots + 1);
	auto objectTouches = [&](int sprite, const Rows &b, const ObjectRows &o, int through) {
		const int y0 = std::max({o.first, b.y0, cleared}), y1 = std::min({o.first + int(o.rows.size()) - 1, b.y1, through});
		for (int l = y0; l <= y1; l++) {
			uint8_t bits, hpos, size;
			int count;
			spriteAt(sprite, l, bits, count, hpos, size);
			if (Beam::spritesTouch(bits, count, hpos, size, o.rows[size_t(l - o.first)], 8, o.x, o.size)) return true;
		}
		return false;
	};
	auto objectEdge = [&](const ObjectRows &o, int through) {
		if (!o.edge) return false;
		const int y0 = std::max(o.first, cleared), y1 = std::min(o.first + int(o.rows.size()) - 1, through);
		for (int l = y0; l <= y1; l++) {
			const uint8_t bits = o.rows[size_t(l - o.first)];
			if (bits && (Beam::playfieldHits(bits, 8, o.x, o.size, pfAt(l), 2) & 2)) return true;
		}
		return false;
	};
	auto inProgress = [&](int slot) { return !objects.empty() && objects.back().slot == slot; };
	auto setFlags = [&](int slot, bool touching) {
		if (slot >= kSlots) return;
		Slot &obj = s.slot[slot];
		obj.flags = uint8_t((obj.flags & ~slotflag::kTouchingEdge) | (touching ? slotflag::kTouchingEdge : 0));
	};

	// The DLI's loop (B565-B664) by scanlines: X (`counter`) goes up by one at every WSYNC, so
	// the scanline drawn while X = c is c - 69 (one more for each scanline the DLI skipped).
	auto slotY = [&](int n) -> uint8_t { return n < kSlots ? s.slot[n].y : 0xFF; };
	auto line = [&](int c) { return c - 69 + slip; };
	int counter = 0x5D;
	int slot = s.firstSlot;
	int current = slot;
	bool endedAtB664 = false;
	for (;;) {
		current = slot;
		bool end = false;
		while (counter < slotY(slot)) {             // B567
			counter = (counter + 1) & 0xFF;
			if (!counter) { end = true; break; }
		}
		if (end) break;
		const int start = counter;
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		const Slot sl = slot < kSlots ? s.slot[slot] : Slot{};
		from(hposP3, setP3x, line(counter), sl.x);  // B57D, before the scanline's draw
		writeP3(counter, 0, line(counter));         // blank rows above the object
		writeP3(counter + 1, 0, line(counter));
		const uint8_t flags = sl.flags;
		const bool facingLeft = flags & slotflag::kFacingLeft;
		const bool bridge = flags & slotflag::kBridge;
		const uint16_t shapes = rom_.shapeAddress(sl.kind + (facingLeft ? 0x10 : 0));
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		from(sizeP3, setSize, line(counter) + 1, sl.size);   // B5A8, after it
		const uint16_t colours = rom_.coloursAddress(sl.type);
		int row = uint8_t(sl.y + rom_.objectHeight(sl.type) - start);
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		if (row >= 0x80) { slot = current + 1; continue; }
		chunk(line(counter), 49 + (crosses(shapes, row) ? 1 : 0));   // B5B0-B5D1
		bool wrapped = false;
		const int firstRow = counter - 0x44;        // player 3's memory is indexed by X
		objects.push_back({current, firstRow, sl.x, sl.size, sl.kind >= object::kHelicopter && sl.kind <= object::kShip, {}});
		for (;;) {                                  // B5CB: a row a scanline
			writeP3(counter, rom_.at(uint16_t(shapes + row)), line(counter));
			objects.back().rows.push_back(rom_.at(uint16_t(shapes + row)));
			if (bridge) from(bank, setBank, line(counter) + 1, rom_.roadColour(row));
			from(colourP3, setColour, line(counter) + 1, rom_.at(uint16_t(colours + row)));
			counter = (counter + 1) & 0xFF;
			if (!counter) { wrapped = true; break; }
			if (--row < 0) break;
		}
		if (wrapped) break;
		// B5E8: the object's collisions
		const int e = counter;
		const bool bulletHit = objectTouches(5, bulletBox, objects.back(), line(e) - (bridge ? 0 : 1));   // M2PL
		if (bulletHit) g.hit.bulletHitObject = uint8_t(current);
		if (bridge) chunk(line(e), 46 + (crosses(Rom::kRoadColours, 0) ? 1 : 0) + (crosses(colours, 0) ? 1 : 0) + (bulletHit ? 2 : 0));
		int hits = 0;
		if (bridge) from(bank, setBank, line(e) + 1, s.sectionBankColour);   // B5F3
		else if (objectTouches(0, shellBox, objects.back(), line(e))) { g.hit.shellHitObject = uint8_t(current); hits++; }   // P3PL
		counter = (counter + 1) & 0xFF;             // B60A
		if (!counter) break;
		{
			if (seenIn(planeShell, line(counter) - 1)) { g.hit.planeHitShell = uint8_t(current); hits++; }   // P2PL
			if (objectTouches(2, planeBox, objects.back(), line(counter) - 1)) { g.hit.planeHitObject = uint8_t(current); hits++; }
			const bool p3pf = objectEdge(objects.back(), line(counter));
			chunk(line(counter), (bridge ? 43 : 44) + 2 * hits);   // B5F3-B61F
			setFlags(current, p3pf);
		}
		counter = (counter + 1) & 0xFF;
		if (!counter) break;
		{
			const int l = line(counter);
			const bool m0pl = seenIn(missilePlane, l - 1);
			if (m0pl) g.hit.missileHitPlane = uint8_t(current);
			const bool late = afterDraw(l, 28 + (m0pl ? 2 : 0));       // B631
			stats.m0pf++;
			if (late) stats.m0pfLate++;
			if (seenIn(missileBank, late ? l : l - 1)) g.hit.missileHitBank = uint8_t(current);
			int stored = 0;
			if (seenIn(tankEdge, l)) { g.hit.tankAtEdge = uint8_t(current); stored++; }
			if (seenIn(planeBank, l)) { g.hit.planeHitBank = uint8_t(current); stored++; }
			if (seenIn(bulletBank, l)) { g.hit.bulletHitBank = uint8_t(current); stored++; }
			const bool clearLate = afterDraw(l + 1, 23 + 2 * stored);   // B65F
			stats.clears++;
			if (clearLate) stats.clearsLate++;
			clearAt(clearLate ? l + 2 : l + 1);
		}
		slot = current + 1;
		counter = (counter + 1) & 0xFF;
		if (!counter) { endedAtB664 = true; break; }
	}
	// B66B: the river ends (X wrapped to 0); the panel part reads the collisions of the object
	// still in progress, unless the loop ended right after a HITCLR
	endLine = line(0x100);
	const bool panelReads = current < kSlots && !endedAtB664;
	if (panelReads) {
		const bool rows = inProgress(current);
		setFlags(current, rows && objectEdge(objects.back(), endLine + 1));
		const int r = endLine + 2;
		if (seenIn(missilePlane, r)) g.hit.missileHitPlane = uint8_t(current);
		if (seenIn(missileBank, r)) g.hit.missileHitBank = uint8_t(current);
		if (seenIn(bulletBank, r)) g.hit.bulletHitBank = uint8_t(current);
		if (seenIn(planeBank, r)) g.hit.planeHitBank = uint8_t(current);
		if (seenIn(planeShell, r)) g.hit.planeHitShell = uint8_t(current);
		if (rows && objectTouches(0, shellBox, objects.back(), r)) g.hit.shellHitObject = uint8_t(current);
		if (rows && objectTouches(2, planeBox, objects.back(), r)) g.hit.planeHitObject = uint8_t(current);
		if (seenIn(tankEdge, r)) g.hit.tankAtEdge = uint8_t(current);
		if (rows && objectTouches(5, bulletBox, objects.back(), endLine + 3)) g.hit.bulletHitObject = uint8_t(current);
	}
	// what is drawn on each scanline, up to the one the DLI last waits for
	const int last = endLine + (panelReads ? 3 : 1);
	fill(last);
	for (int l = kFirstLine; l <= last; l++) {
		Scanline &sc = scan[size_t(l)];
		sc.p3 = p3At(l);
		sc.p3x = hposP3[l];
		sc.p3size = sizeP3[l];
		sc.p3colour = colourP3[l];
		sc.bankColour = bank[l];
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
