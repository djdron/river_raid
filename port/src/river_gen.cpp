// River Raid river generator. See river_gen.h.
// Addresses in comments refer to the original cartridge code ($A000-$BFFF).
//
// The geometry is computed in bytes with 8-bit wrap-around, as the original does: positions
// and widths are uint8_t and every sum is taken modulo 256 (lo()). In a few places the
// original carries the 6502's carry flag from one addition into the next; addWithCarry() and
// subtractWithBorrow() keep those exact.
#include "river_gen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rr {

bool Rom::load(const std::string &path) {
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f) return false;
	size_t n = std::fread(data_.data(), 1, data_.size(), f);
	std::fclose(f);
	return n == data_.size();
}

namespace {

inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }

// a + b + carry as the 6502's ADC: carry becomes the carry out.
inline uint8_t addWithCarry(uint8_t a, uint8_t b, int &carry) {
	const int r = a + b + carry;
	carry = r > 0xFF;
	return lo(r);
}
// a - b as the 6502's SBC: one more is taken off when carry is 0 (a borrow); carry becomes
// "no borrow".
inline uint8_t subtractWithBorrow(uint8_t a, uint8_t b, int &carry) {
	const int r = a - b - (1 - carry);
	carry = r >= 0;
	return lo(r);
}
// a - b - c with two SBCs: a borrow out of the first subtraction takes one more off.
inline uint8_t subtractTwice(uint8_t a, uint8_t b, uint8_t c) {
	int carry = 1;
	const uint8_t r = subtractWithBorrow(a, b, carry);
	return subtractWithBorrow(r, c, carry);
}

constexpr int kBlockLines = 32;
constexpr int kSectionsPerBridge = 16;
constexpr uint8_t kBankBytes = 0x55;     // a byte of bank (colour 1 in all four pixels)
constexpr uint8_t kFlashBytes = 0xAA;    // a byte of edge colour (2): the bank of a hit bridge
constexpr uint8_t kWaterBytes = 0xFF;    // a byte of water (3)
constexpr uint8_t kIslandBytes = 0xAA;   // a narrow island: edge colour only

}  // namespace

// sub $B183: the 24-bit LFSR. Feedback = bit 7 of ((B << 5) ^ B), shifted in through A, then
// A's top bit into C and C's into B.
void RiverGen::stepLfsr() {
	const int feedback = (lo(s.rndB << 5) ^ s.rndB) >> 7;
	const int outA = s.rndA >> 7, outC = s.rndC >> 7;
	s.rndA = lo((s.rndA << 1) | feedback);
	s.rndC = lo((s.rndC << 1) | outA);
	s.rndB = lo((s.rndB << 1) | outC);
}

// sub $B194: at the start of the last section before a bridge: remember where the river is
// (the restart point after a crash) and go up a level, which stops at $50.
void RiverGen::sectionCheckpoint() {
	const uint8_t level = s.level >= 0x50 ? 0x4E : s.level;
	std::memcpy(s.prevCheckpoint, s.checkpoint, 4);
	s.checkpoint[0] = s.rndA;
	s.checkpoint[1] = s.rndB;
	s.checkpoint[2] = s.rndC;
	s.checkpoint[3] = s.bank;
	s.level = lo(level + 1);
}

// $A711-$A7FA: count the line; at the start of each block and section, set up what comes:
// the run-up to a bridge, a new river width, an island opening or closing.
void RiverGen::countLines() {
	s.bankMargin = s.level < 9 ? 6 : 0;
	if (++s.lineInBlock < kBlockLines) {
		if (!(s.blockFlags & kBridgeBlock)) stepLfsr();   // the run-up to a bridge is straight
		return;
	}
	// $A72B: a new block
	s.lineInBlock = 0;
	s.decoFlags = 0;
	s.corridor = 0;
	s.blockFlags = lo((s.blockFlags & kBankShade) | kNewBlock);
	s.prevSectionWidth = s.sectionWidth;
	if (--s.blocksLeft != 0) {                          // the second block of a section
		if (s.sectionsLeft == 1) {                      // $A749: the last one: run-up to the bridge
			s.bridgeHit = 0;
			s.blockFlags = (s.level & 1) ? kBridgeBlock : uint8_t(kBridgeBlock | kBankShade);
		}
		stepLfsr();
		return;
	}
	// $A75C: a new section
	if (--s.sectionsLeft == 0) {
		sectionCheckpoint();
		s.sectionsLeft = kSectionsPerBridge;
	}
	stepLfsr();
	uint8_t islandTarget;
	if (s.sectionsLeft == 1) {                          // $A76F: the section ending in the bridge
		s.islandFlags = 0;
		s.sectionWidth = 0x0D;
		if (s.level & 1) s.corridor--;
		islandTarget = 0x36;
	} else {
		const uint8_t width = (s.level & 1) ? 7 : chooseSectionWidth();   // odd levels: always 7
		s.sectionWidth = width;                         // $A7DC
		islandTarget = lo(width << 2);
	}
	// $A7E0: the island (or the extra bank) changes towards its new width, 1 per line, or 2
	// when the change is 32 or more
	s.blocksLeft = 2;
	s.islandTargetWidth = islandTarget;
	const uint8_t change = lo(islandTarget - s.islandWidth);
	if (change == 0) return;
	if (islandTarget < s.islandWidth) s.islandStep = change >= 0xE0 ? 0xFF : 0xFE;
	else s.islandStep = change >= 0x20 ? 2 : 1;
}

// $A781-$A7DB, even levels: the width code of a new section; islands open and close here.
uint8_t RiverGen::chooseSectionWidth() {
	bool openOrClose = false;   // $A79E: a section of width 0 before an island opens or after it closes
	if (s.sectionsLeft == 2) {                          // $A78D: the second-to-last section
		if (s.islandFlags == (kIsland | kIslandOpening)) openOrClose = true;   // close the island
		else s.islandFlags = 0;                         // $A7B2: no island before the bridge
	} else if ((lo(s.islandFlags << 1) ^ s.islandFlags) & 0x80) {
		// $A7AC: after an opening ($40) the island is there ($C0); after a closing ($80) it is gone
		s.islandFlags = (s.islandFlags & kIslandOpening) ? uint8_t(kIsland | kIslandOpening) : uint8_t(0);
	} else if (!(s.rndA & 0x30)) {
		openOrClose = true;                             // one time in four
	}
	if (openOrClose) {
		s.islandFlags = (s.islandFlags & kIsland) ? kIsland : kIslandOpening;
		return 0;
	}
	// $A7B6: a random width 2..15, narrower with an island, below level 20 and below level 9
	uint8_t limit = 0x0F;
	if (s.islandFlags & kIsland) limit--;
	if (s.level < 0x14) limit--;
	if (s.bankMargin != 0) limit = 8;
	uint8_t width = s.rndA & 0x0F;
	if (width < 2) width += 2;
	return std::min(width, limit);
}

// $A7FB-$A825: the island moves toward its target width; the water narrows by twice as much.
void RiverGen::changeIsland() {
	if (s.islandStep == 0) return;
	const bool shrinking = s.islandStep & 0x80;
	const bool reached = shrinking ? s.islandTargetWidth >= s.islandWidth : s.islandWidth >= s.islandTargetWidth;
	if (reached) {
		s.islandStep = 0;
		return;
	}
	s.islandWidth = lo(s.islandWidth + s.islandStep);
	s.waterWidth = lo(s.waterWidth - 2 * s.islandStep);
}

// $A8D2-$A95E: would one more bank step run into the object still being drawn in? (The bank
// keeps clear of a new enemy, bridge or depot: type 8 and up.)
bool RiverGen::bankWouldHitObject(bool movingLeft) const {
	if (!obstacle_ || !obstacle_->present) return false;
	const BankObstacle &obj = *obstacle_;
	const bool island = s.islandFlags & kIsland;
	const bool objectRightOfIsland = island && (obj.islandSide & 0x80);
	// the island's width while it changes: the larger of now and target
	const uint8_t islandWidth = std::max(s.islandWidth, s.islandTargetWidth);
	const uint8_t gap = lo((0x40 - islandWidth) << 1);
	if (movingLeft) {
		// the object must stay left of this point
		uint8_t edge = 0;
		bool fromGap = true;
		if (objectRightOfIsland) {
			edge = lo(s.bank + 0x80);
			fromGap = edge == 0;
		}
		if (!island) {
			edge = lo(s.bank + gap + islandWidth);
		} else if (fromGap) {
			edge = lo((gap >> 1) + s.bank);
			if (edge == 0) edge = lo(edge + gap + islandWidth);
		}
		uint8_t left = lo(obj.x - 0x17 + s.bankMargin);
		if (obj.size) left = lo(left + 8);
		return left >= edge;
	}
	// moving right: the object must stay right of this point
	uint8_t edge = s.bank;
	if (!island) {
		edge = lo(edge + islandWidth);
	} else if (objectRightOfIsland) {
		int carry = 1;
		edge = addWithCarry(s.bank, 0x3F, carry);      // bank + $40 ...
		edge = addWithCarry(edge, islandWidth, carry); // ... + island, with the carry of the first sum
		if (edge == 0) edge = lo(edge + islandWidth);
	}
	edge = lo(edge + 2);
	return lo(obj.x - 0x20 - s.bankMargin) < edge;
}

// $A826-$A96B: the bank position for this line.
void RiverGen::moveBank() {
	if (s.blockFlags & kBridgeBlock) {                  // run-up to a bridge: a fixed shape
		s.bankDir = 0;
		s.bank = lo(rom_.bridgeBankStep(s.lineInBlock) + s.bank);
		return;
	}
	if (s.corridor) {                                   // pull the bank into $18..$28
		if (s.bank < 0x18) { s.bank++; return; }
		if (s.bank >= 0x29) { s.bank--; return; }
	}
	// $A84F: walk limits by level
	uint8_t low = 1, high = 0x40;
	if (s.level < 3) {
		low = 0x1C; high = 0x24;
	} else if (!(s.level & 1) || s.corridor || s.level < 0x0A) {
		low = 0x19; high = 0x28;
	} else {
		if (s.level < 0x14) { low = 0x0D; high = 0x34; }
		if (s.decoFlags != 0) {                         // leave room for the decoration
			if (s.decoFlags & kDecoLeft) low = 0x19;
			else high = 0x28;
		}
	}
	s.bankMin = low;
	s.bankMax = high;

	// $A889: random walk driven by the low 4 bits of LFSR byte C
	const uint8_t r = s.rndC & 0x0F;
	if (s.bankDir == 0) {
		s.bankDir = r >= 8 ? 1 : 0xFF;
	} else {
		if (r < 8) return;                              // stay
		if (r == 0x0D) {
			s.bankDir ^= 0xFE;                          // turn round
		} else if (r == 0x0F) {                         // stay where the river is narrow
			const uint8_t narrow = (s.islandFlags & kIsland) ? 0x0D : 0x0E;
			if (narrow < s.sectionWidth || narrow < s.prevSectionWidth) return;
		}
	}
	// $A8C4: one step, unless it crosses the limits or runs into the new object: then turn round
	if (s.bankDir & 0x80) {
		if (s.bank >= s.bankMin && !bankWouldHitObject(true)) { s.bank--; return; }
	} else {
		if (s.bank < s.bankMax && !bankWouldHitObject(false)) { s.bank++; return; }
	}
	s.bankDir ^= 0xFE;                                  // $A95F
}

// $A96C-$A9D6: at the start of some blocks, a house or mountain for the bank.
void RiverGen::startDecoration() {
	if (s.blockFlags & kBridgeBlock) return;
	if (s.lineInBlock != 0) return;
	if (!(s.rndC & 0x20)) return;
	int index = 0;
	uint8_t offset = 0x80;                              // bit 7: none chosen yet
	if (s.level & 1) {
		index = s.rndA & 7;
		offset = rom_.decoOffsetOddLevel(index);
	}
	if (offset & 0x80) {                                // by the river width
		index = std::min(s.sectionWidth, s.prevSectionWidth);
		if (index < 2) return;
		if (s.islandFlags & kIsland) {
			if (index < 5) return;
			index = lo(index + 0x0B);
		}
		offset = rom_.decoOffsetForWidth(index);
	}
	s.decoOffset = offset;
	s.decoWidth = rom_.decoWidth(index);
	s.decoRows = rom_.decoRows(s.decoWidth);
	// $A9B1: the side from bit 7 of C, swapped when the bank there is too narrow
	uint8_t side = s.rndC & kDecoLeft;
	if (side) { if (s.bank < 0x18) side ^= kDecoLeft; }
	else if (s.bank >= 0x29) side ^= kDecoLeft;
	s.decoFlags = lo(side + 1);
	s.decoBitmap = rom_.decoBitmap(s.decoWidth);
}

// $AA8C-$AB75: draw the line. Runs of whole bytes (bank, water, island) with 2-byte edge
// pieces where they meet (from the table at $B800, chosen by the pixel within the byte), plus
// a row of the decoration bitmap. Horizontal positions x are in pixels; 4 pixels per byte.
void RiverGen::drawLine(uint8_t out[kLineBytes], uint8_t *bank) {
	uint8_t buf[256] = {};
	uint8_t plain[256] = {};                            // the same without decoration
	int pos = 0;                                        // $B4: bytes written so far
	const uint8_t bankFill = s.flashLines ? kFlashBytes : kBankBytes;   // sub $AFF4
	// count bytes of one value; nothing when count is 0 or above $80 (a negative count)
	auto fill = [&](uint8_t count, uint8_t value) {
		if (count == 0 || count > 0x80) return;
		for (int i = 0; i < count; i++) buf[(pos + i) & 0xFF] = plain[(pos + i) & 0xFF] = value;
		pos += count;
	};
	auto edgeIndex = [](uint8_t x) { return lo((x & 3) << 2); };   // pixel within the byte
	auto edgePiece = [&](uint8_t piece) {               // sub $B03B
		buf[pos & 0xFF] = plain[pos & 0xFF] = rom_.edgePiece(piece);
		buf[(pos + 1) & 0xFF] = plain[(pos + 1) & 0xFF] = rom_.edgePiece(lo(piece + 1));
		pos += 2;
	};
	// bank edge pieces: the flashing set is $10 further
	auto bankEdge = [&](uint8_t piece) { edgePiece(s.flashLines ? lo(piece + 0x10) : piece); };   // sub $B034
	auto decoRow = [&]() {                              // sub $B01A
		for (int i = s.decoWidth - 1; i >= 0; i--) {
			buf[(pos + i) & 0xFF] = rom_.at(uint16_t(s.decoBitmap + i));
			plain[(pos + i) & 0xFF] = bankFill;
		}
		s.decoBitmap = uint16_t(s.decoBitmap + s.decoWidth);
		pos += s.decoWidth;
	};
	const bool island = s.islandFlags & kIsland;
	const bool decoRowsLeft = s.lineInBlock < s.decoRows;

	// left bank up to the water; without an island the extra width is bank too
	uint8_t x = island ? s.bank : lo(s.bank + s.islandWidth);   // x: where the next part starts
	uint8_t count = lo((x >> 2) - 1);
	if (!island && (s.decoFlags & kDecoLeft) && decoRowsLeft) {
		fill(s.decoOffset, bankFill);
		decoRow();
		count = subtractTwice(count, s.decoWidth, s.decoOffset);
	}
	fill(count, bankFill);                              // $AAB9
	bankEdge(edgeIndex(x));
	uint8_t water = s.waterWidth;                       // water up to the right bank
	if (island) {
		if (s.islandWidth < 6) {
			// too narrow to draw: the island's width is water
			int carry = 0;
			water = addWithCarry(water, s.islandWidth, carry);
			water = addWithCarry(water, s.islandWidth, carry);
		} else {
			// $AAD2: water, the island, water
			x = lo((water >> 1) + x);
			fill(lo((x >> 2) - pos), kWaterBytes);
			const bool small = s.islandWidth < 8;
			edgePiece(lo(edgeIndex(x) + 2 + (small ? 0x10 : 0)));
			int carry = s.islandWidth >> 7;             // $AAEF: x += 2 * island width
			x = addWithCarry(lo(s.islandWidth << 1), x, carry);
			count = lo((x >> 2) - pos - 1);
			uint8_t piece;
			if (small) {
				fill(count, kIslandBytes);
				piece = lo(edgeIndex(x) + 0x10);
			} else {
				if (s.decoFlags != 0 && decoRowsLeft) { // a decoration on the island
					const uint8_t before = lo(s.decoOffset - pos);
					const uint8_t after = subtractTwice(count, before, s.decoWidth);
					fill(before, bankFill);
					decoRow();
					count = after;
				}
				fill(count, bankFill);
				piece = edgeIndex(x);
			}
			edgePiece(piece);                           // $AB31
			water = s.waterWidth >> 1;
		}
	}
	// $AB37: water up to the right bank
	x = lo(water + x);
	fill(lo((x >> 2) - pos), kWaterBytes);
	bankEdge(lo(edgeIndex(x) + 2));
	// $AB4B: the right bank to the end of the 48-byte line
	count = lo(kLineBytes - pos);
	if (!island && s.decoFlags != 0 && !(s.decoFlags & kDecoLeft) && decoRowsLeft) {
		fill(lo(subtractTwice(count, s.decoOffset, s.decoWidth) + 1), bankFill);
		decoRow();
		count = lo(s.decoOffset - 1);
	}
	fill(count, bankFill);
	std::memcpy(out, buf, kLineBytes);
	if (bank) std::memcpy(bank, plain, kLineBytes);
}

// Respawn (also used for the game start): sub $B0F1 resets the river and puts a bridge at
// the bottom of the screen, then $A3C8 continues from the checkpoint.
// Fields the generator always rewrites before reading (unused slots, decoration data while
// no decoration is active) keep whatever they held.
void RiverGen::restoreCheckpoint() {
	// $B0F1: $4D-$5F from the reset table (the objects' part: Objects::restoreCheckpoint)
	auto reset = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	s.flashLines = reset(0x4F);
	s.bank = reset(0x52);
	s.waterWidth = reset(0x53);
	s.islandWidth = reset(0x54);
	s.blocksLeft = reset(0x5A);
	s.sectionsLeft = reset(0x5D);
	s.sectionWidth = reset(0x5F);
	// $2B-$4C cleared
	s.prevSectionWidth = s.islandFlags = s.bridgeHit = s.lineInBlock = 0;
	s.islandTargetWidth = s.islandStep = s.decoFlags = 0;
	// $0500-$05FF cleared: the corridor (and the objects)
	s.corridor = 0;
	// $B15E: the bridge we restart from is in the lowest slot, the section's bank colour
	s.sectionBankColour = (s.level & 1) ? 0xD6 : 0xD2;
	s.blockFlags = s.sectionBankColour & (kBridgeBlock | kBankShade);
	// $A3C8: continue the river from the checkpoint
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
}

void RiverGen::startsFromRom() {
	for (int n = 0; n < 4; n++) {
		Start &start = starts_[n];
		for (int i = 0; i < 4; i++) start.checkpoint[i] = rom_.startCheckpoint(n, i);
		start.level = rom_.startLevel(n);
		const uint8_t bcd = rom_.startBridges(n);
		start.bridge = (bcd >> 4) * 10 + (bcd & 0x0F);
	}
}

void RiverGen::startGame(int gameNumber, bool pal) {
	s = RiverState{};
	s.depotLevelLimit = pal ? 0x2F : 0x2E;              // $A077
	// $AF9B: the starting point of the selected game (2 games per starting bridge)
	const int start = ((gameNumber - 1) & 7) >> 1;
	for (int i = 0; i < 4; i++) s.checkpoint[i] = starts_[start].checkpoint[i];
	s.level = starts_[start].level;
	restoreCheckpoint();
}

void RiverGen::nextLine(uint8_t line[kLineBytes], uint8_t *bank, const BankObstacle &obstacle) {
	obstacle_ = &obstacle;
	countLines();
	changeIsland();
	moveBank();
	startDecoration();
	drawLine(line, bank);
	obstacle_ = nullptr;
	if (s.flashLines) s.flashLines--;   // ($AB76)
}

}  // namespace rr
