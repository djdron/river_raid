// The river part of the display: where the objects are this frame, the collisions, and the
// picture of the river. See game.h.
//
// The original shows every object with one hardware sprite (player 3), which its display list
// interrupt (DLI) moves down the river slot by slot, and reads the hardware's collision
// registers after each object. This port has no DLI (docs/DLI_REMOVAL.md, branch dli_removed):
// each object is at its own slot's y, cut to the river, and the collisions are plain geometry,
// each where the sprites' and the objects' rows touch at all (boxes first, then the dots). An
// object hit is the last such object down the screen (as the original's stores overwrite each
// other); the shell is not tested against a bridge. The others (bank, edge, plane and shell,
// missile and plane) hold slot 0 when they happen anywhere on the sprites' rows. An object's
// edge bit (helicopters, balloons, ships) from all its rows, each row by its first and last
// dots against the line's edge runs (no bit by bit, so a run in a hole of a row counts); the
// other sprites against the bank and the edge (tank, plane, missile, bullet) the same way. The
// game differs from the original on 0.01-0.25% of frames (docs/DLI_REMOVAL.md, RR_GEOMCHECK on
// the branch dli_removal): a touch the original sees a frame later or not at all.
#include <algorithm>

#include "game.h"
#include "river_draw.h"

namespace rr {
namespace {

// First scanline of the river (display list: 8 + 8 + 4 blank lines, then mode E from 28).
constexpr int kRiverFirstLine = 28;
constexpr int kRiverLines = 160;
constexpr int kClocks = kLineBytes * 4;   // colour clocks of a line (one per playfield pixel)

// A sprite's row on a scanline: its graphics bits (`count`: 8 a player, 2 a missile), each 1,
// 2 or 4 colour clocks wide (size), and the clocks from its first to its last set bit
// (left..right-1, not clipped to the line).
struct Span {
	uint8_t bits = 0;
	int count = 0, hpos = 0, width = 1;
	int left = 0, right = 0;
};
int dotWidth(uint8_t size) { return (size & 3) == 3 ? 4 : (size & 3) == 1 ? 2 : 1; }
Span spriteSpan(uint8_t bits, int count, uint8_t hpos, uint8_t size) {
	Span sp;
	sp.bits = bits;
	sp.count = count;
	sp.hpos = hpos;
	sp.width = dotWidth(size);
	if (!bits) return sp;
	int first = 0, last = count - 1;
	while (!(bits & (1 << (count - 1 - first)))) first++;
	while (!(bits & (1 << (count - 1 - last)))) last--;
	sp.left = hpos - 32 + first * sp.width;
	sp.right = hpos - 32 + (last + 1) * sp.width;
	return sp;
}

// The colour clocks of a line, one bit each.
struct Mask {
	uint64_t w[3] = {};
	void set(int from, int count) {   // clocks from..from+count-1, clipped to the line
		for (int x = std::max(0, from); x < std::min(kClocks, from + count); x++) w[x >> 6] |= uint64_t(1) << (x & 63);
	}
	bool operator&(const Mask &o) const { return (w[0] & o.w[0]) | (w[1] & o.w[1]) | (w[2] & o.w[2]); }
};
Mask spriteMask(const Span &sp) {
	Mask m;
	for (int b = 0; b < sp.count; b++)
		if (sp.bits & (1 << (sp.count - 1 - b))) m.set(sp.hpos + b * sp.width - 32, sp.width);
	return m;
}

// A sprite's or an object's row against a scanline's playfield: the row's first and last dots
// (its span) against each run of the colours wanted (bit 0 bank, 1 edge), no bit by bit (a run
// between them counts even when it falls into a hole of the row). The colours touched.
uint8_t spanTouches(uint8_t bits, int count, uint8_t hpos, uint8_t size, const Game::Runs *runs, uint8_t wanted) {
	if (!bits || !runs) return 0;
	const Span sp = spriteSpan(bits, count, hpos, size);
	uint8_t hits = 0;
	for (int i = 0; i < runs->n; i++) {
		const uint8_t bit = uint8_t(1 << (runs->colour[i] - 1));
		if ((wanted & bit) && runs->b[i] > sp.left && runs->a[i] < sp.right) hits |= bit;
	}
	return hits;
}
bool edgeTouches(uint8_t bits, int count, uint8_t hpos, uint8_t size, const Game::Runs *runs) {
	return spanTouches(bits, count, hpos, size, runs, 2) != 0;
}
// Two sprites' rows on a scanline: their spans, then bit by bit where they meet (on the line).
bool spritesTouch(uint8_t bitsA, int countA, uint8_t hposA, uint8_t sizeA, uint8_t bitsB, int countB, uint8_t hposB, uint8_t sizeB) {
	if (!bitsA || !bitsB) return false;
	const Span p = spriteSpan(bitsA, countA, hposA, sizeA), q = spriteSpan(bitsB, countB, hposB, sizeB);
	const int a = std::max({p.left, q.left, 0}), b = std::min({p.right, q.right, kClocks});
	if (a >= b) return false;
	return spriteMask(p) & spriteMask(q);
}

}  // namespace

void Game::display() {
	RiverState &s = gen.s;
	scan = {};
	g.hit = Collisions();
	constexpr int kLines = 262;
	constexpr int kFirstLine = 24;
	uint8_t bank[kLines];
	bool setBank[kLines] = {};
	auto from = [&](int l, uint8_t v) {
		if (l < 0 || l >= kLines) return;
		bank[l] = v;
		setBank[l] = true;
	};
	from(0, g.colourBank);
	const int endLine = 187;   // the river's last scanline
	auto spriteAt = [&](int sprite, int l, uint8_t &bits, int &count, uint8_t &hpos, uint8_t &size) {
		const bool panel = l >= endLine + 1;
		count = sprite < 4 ? 8 : 2;
		size = 0;
		switch (sprite) {
		case 0: bits = g.pm.player(0, l); hpos = panel ? 0 : g.shellX; break;
		case 1: bits = g.pm.player(1, l); hpos = panel ? 0 : s.tankX; break;
		case 2: bits = g.pm.player(2, l); hpos = g.planeX; break;
		case 4: bits = uint8_t(g.pm.missiles(l) & 3); hpos = panel ? 0 : s.missileX; size = 1; break;
		default: bits = uint8_t((g.pm.missiles(l) >> 4) & 3); hpos = uint8_t(g.planeX + 4); break;
		}
	};
	auto pfAt = [&](int l) -> const Runs * {
		const int r = l - kRiverFirstLine;
		return r >= 0 && r < kRiverLines && size_t(r) < runLines.size() ? &runLines[size_t(r)] : nullptr;
	};
	struct Rows { int y0 = 0, y1 = -1; };
	auto box = [&](int y0, int y1) { Rows b; b.y0 = std::max(y0, kFirstLine); b.y1 = std::min(y1, kLines - 1); return b; };
	const Rows shellBox = box(s.shellY, s.shellY + 15), tankBox = box(s.tankY, s.tankY + 10), planeBox = box(0xAA, 0xAA + 13);
	Rows missileBox, bulletBox;
	if (s.missileY >= 0x1C) missileBox = box(s.missileY, s.missileY);
	if (g.bullet.shown()) bulletBox = box(g.bullet.top(), g.bullet.top() + 7);
	auto pfAny = [&](int sprite, const Rows &b, uint8_t want) {
		for (int l = b.y0; l <= b.y1; l++) {
			uint8_t bits, hpos, size;
			int count;
			spriteAt(sprite, l, bits, count, hpos, size);
			if (spanTouches(bits, count, hpos, size, pfAt(l), want)) return true;
		}
		return false;
	};
	auto pairAny = [&](int a, const Rows &ba, int b, const Rows &bb) {
		for (int l = std::max(ba.y0, bb.y0); l <= std::min(ba.y1, bb.y1); l++) {
			uint8_t bitsA, hposA, sizeA, bitsB, hposB, sizeB;
			int countA, countB;
			spriteAt(a, l, bitsA, countA, hposA, sizeA);
			spriteAt(b, l, bitsB, countB, hposB, sizeB);
			if (spritesTouch(bitsA, countA, hposA, sizeA, bitsB, countB, hposB, sizeB)) return true;
		}
		return false;
	};
	if (pfAny(1, tankBox, 2)) g.hit.tankAtEdge = 0;
	if (pfAny(2, planeBox, 1)) g.hit.planeHitBank = 0;
	if (pfAny(4, missileBox, 3)) g.hit.missileHitBank = 0;
	if (pfAny(5, bulletBox, 3)) g.hit.bulletHitBank = 0;
	if (pairAny(2, planeBox, 0, shellBox)) g.hit.planeHitShell = 0;
	if (pairAny(4, missileBox, 2, planeBox)) g.hit.missileHitPlane = 0;

	struct ObjectRows { int slot, first; uint8_t x, size; bool edge, bridge; std::vector<uint8_t> rows, colours; };
	std::vector<ObjectRows> objects;
	objects.reserve(kSlots + 1);
	auto touches = [&](int sprite, const Rows &b, const ObjectRows &o) {
		const int y0 = std::max(o.first, b.y0), y1 = std::min(o.first + int(o.rows.size()) - 1, b.y1);
		for (int l = y0; l <= y1; l++) {
			uint8_t bits, hpos, size;
			int count;
			spriteAt(sprite, l, bits, count, hpos, size);
			if (spritesTouch(bits, count, hpos, size, o.rows[size_t(l - o.first)], 8, o.x, o.size)) return true;
		}
		return false;
	};
	// (the edge: each row's first and last dots against the line's edge runs, edgeTouches)
	auto edgeOf = [&](const ObjectRows &o) {
		if (!o.edge) return false;
		for (size_t i = 0; i < o.rows.size(); i++)
			if (edgeTouches(o.rows[i], 8, o.x, o.size, pfAt(o.first + int(i)))) return true;
		return false;
	};

	// Each object at its own y: its rows from scanline y - 65 down (x0 = y + 3; row k, written
	// while X = x0 + k, shows on scanline x0 + k - 68; row `height` first, as the shape tables
	// keep them bottom up), cut at the river's top (scanline 28: the DLI's first wait was
	// X = $5D) and at its end (X past 255)
	auto place = [&](std::vector<ObjectRows> &out) {
		for (int n = s.firstSlot; n < kSlots; n++) {
			const Slot &sl = s.slot[size_t(n)];
			const int start = std::max<int>(sl.y, 0x5D);
			const int row = uint8_t(sl.y + rom_.objectHeight(sl.type) - start);
			if (row >= 0x80) continue;
			const int x0 = start + 3;
			if (x0 > 0xFF) break;
			const bool facingLeft = sl.flags & slotflag::kFacingLeft;
			const bool bridge = sl.flags & slotflag::kBridge;
			const uint16_t shapes = rom_.shapeAddress(sl.kind + (facingLeft ? 0x10 : 0));
			const uint16_t colours = rom_.coloursAddress(sl.type);
			const int count = std::min(row + 1, 0x100 - x0);
			out.push_back({n, x0 - 0x44, sl.x, sl.size, sl.kind >= object::kHelicopter && sl.kind <= object::kShip, bridge, {}, {}});
			for (int k = 0; k < count; k++) {
				out.back().rows.push_back(rom_.at(uint16_t(shapes + row - k)));
				out.back().colours.push_back(rom_.at(uint16_t(colours + row - k)));
				if (bridge && &out == &objects) from(x0 + k - 69 + 1, rom_.roadColour(row - k));
			}
			if (bridge && &out == &objects && x0 + count <= 0xFF) from(x0 + count - 69 + 1, s.sectionBankColour);
		}
	};
	place(objects);
	for (const ObjectRows &o : objects) {
		const uint8_t n = uint8_t(o.slot);
		if (touches(5, bulletBox, o)) g.hit.bulletHitObject = n;
		if (!o.bridge && touches(0, shellBox, o)) g.hit.shellHitObject = n;
		if (touches(2, planeBox, o)) g.hit.planeHitObject = n;
		if (o.slot < kSlots) {
			Slot &obj = s.slot[size_t(o.slot)];
			obj.flags = uint8_t((obj.flags & ~slotflag::kTouchingEdge) | (edgeOf(o) ? slotflag::kTouchingEdge : 0));
		}
	}
	// the bank colour of each scanline
	const int last = endLine + 3;
	for (int l = 1; l <= last; l++)
		if (!setBank[l]) bank[l] = bank[l - 1];
	for (int l = kFirstLine; l <= last; l++) scan[size_t(l)].bankColour = bank[l];
	drawn.clear();
	for (const ObjectRows &o : objects) drawn.push_back({o.slot, o.first, o.x, o.size, o.rows, o.colours});
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
		for (const DrawnObject &o : drawn)                                   // the objects
			if (line >= o.first && line < o.first + int(o.rows.size()))
				player(row, o.rows[size_t(line - o.first)], o.x, o.size, o.colours[size_t(line - o.first)]);
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
