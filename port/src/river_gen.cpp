// River Raid river generator. See river_gen.h.
// Addresses in comments refer to the original cartridge code ($A000-$BFFF).
#include "river_gen.h"

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
// 6502 ADC / SBC with an explicit carry, for the few places where the original
// relies on a carry left over from an earlier instruction.
inline uint8_t adc(uint8_t a, uint8_t b, int &c) { int r = a + b + c; c = r > 0xFF; return lo(r); }
inline uint8_t sbc(uint8_t a, uint8_t b, int &c) { int r = a - b - (1 - c); c = r >= 0; return lo(r); }
}  // namespace

// sub $B183: 24-bit LFSR. Feedback = bit 7 of ((B << 5) ^ B), shifted in through A -> C -> B.
void RiverGen::stepLfsr() {
	int fb = (lo(s.rndB << 5) ^ s.rndB) >> 7;
	int ca = s.rndA >> 7, cc = s.rndC >> 7;
	s.rndA = lo((s.rndA << 1) | fb);
	s.rndC = lo((s.rndC << 1) | ca);
	s.rndB = lo((s.rndB << 1) | cc);
}

// sub $B194: at the start of the last section before a bridge: remember where the river
// is (respawn point) and advance the level number, which stops growing at $50.
void RiverGen::sectionCheckpoint() {
	uint8_t lvl = s.level >= 0x50 ? 0x4E : s.level;
	std::memcpy(s.prevCheckpoint, s.checkpoint, 4);
	s.checkpoint[0] = s.rndA; s.checkpoint[1] = s.rndB; s.checkpoint[2] = s.rndC; s.checkpoint[3] = s.bank;
	s.level = lo(lvl + 1);
}

// $A711-$A7FA: line counter, blocks of 32 lines, sections of 2 blocks, 16 sections per
// bridge. At each section start a new target width is chosen.
void RiverGen::structureAndWidth() {
	s.bankMargin = s.level < 9 ? 6 : 0;
	s.lineInBlock++;
	if (s.lineInBlock < 0x20) {                       // ordinary line
		if (!(s.blockFlags & 0x80)) stepLfsr();             // no randomness on a bridge approach
		return;
	}
	// $A72B: new block
	s.lineInBlock = 0;
	s.decoFlags = 0;
	s.corridor = 0;
	s.blockFlags = lo((s.blockFlags & 0x04) | 0x10);
	s.prevSectionWidth = s.sectionWidth;
	if (--s.blocksLeft != 0) {                        // second block of the section
		if (lo(s.sectionsLeft - 1) == 0) {            // $A749: last section: bridge follows
			s.bridgeHit = 0;
			s.blockFlags = (s.level & 1) ? 0x80 : 0x84;
		}
		stepLfsr();
		return;
	}
	// $A75C: new section
	if (--s.sectionsLeft == 0) {
		sectionCheckpoint();
		s.sectionsLeft = 0x10;
	}
	stepLfsr();
	uint8_t a;
	uint8_t x = lo(s.sectionsLeft - 1);
	if (x == 0) {                                     // $A76F: section that ends in the bridge
		s.islandFlags = 0;
		s.sectionWidth = 0x0D;
		if (s.level & 1) s.corridor--;
		a = 0x36;
	} else {
		// $A781
		if (s.level & 1) {
			a = 7;
		} else {
			bool toA79E = false, toA7B2 = false, toA7AC = false;
			x--;
			if (x == 0) {                             // $A78D: second-to-last section
				if (s.islandFlags == 0xC0) toA79E = true; else toA7B2 = true;
			} else {                                  // $A793
				uint8_t t = lo(s.islandFlags << 1) ^ s.islandFlags;
				if (t & 0x80) toA7AC = true;
				else if (!(s.rndA & 0x30)) toA79E = true;
			}
			if (toA79E) {                             // $A79E: switch island bits, width 0
				a = s.islandFlags & 0x80;
				if (a == 0) a |= 0x40;
				s.islandFlags = a;
				a = 0;
			} else {
				if (toA7AC) s.islandFlags = (s.islandFlags & 0x40) ? 0xC0 : 0x00;   // $A7AC
				else if (toA7B2) s.islandFlags = 0;                          // $A7B2
				// $A7B6: random width code 2..15 with limits
				uint8_t y = 0x0F;
				a = s.rndA & 0x0F;
				if (a < 2) a += 2;
				if (s.islandFlags & 0x80) y--;
				if (s.level < 0x14) y--;
				if (s.bankMargin != 0) y = 8;
				s.vB1 = y;
				if (a >= y) a = y;
			}
		}
		s.sectionWidth = a;                              // $A7DC
		a = lo(a << 2);
	}
	// $A7E0: new island target; step +-1, or +-2 when the change is 32 or more
	s.blocksLeft = 2;
	s.islandTargetWidth = a;
	uint8_t diff = lo(a - s.islandWidth);
	if (diff == 0) return;
	if (a < s.islandWidth) s.islandStep = diff >= 0xE0 ? 0xFF : 0xFE;
	else s.islandStep = diff >= 0x20 ? 2 : 1;
}

// $A7FB-$A825: the island moves toward its target; the water narrows by twice as much.
void RiverGen::smoothIsland() {
	if (s.islandStep == 0) return;
	bool reached = (s.islandStep & 0x80) ? s.islandTargetWidth >= s.islandWidth : s.islandWidth >= s.islandTargetWidth;
	if (reached) { s.islandStep = 0; return; }
	s.islandWidth = lo(s.islandWidth + s.islandStep);
	s.waterWidth = lo(s.waterWidth - s.islandStep - s.islandStep);
}

// $A826-$A96B: bank position for this line.
void RiverGen::moveBank() {
	if (s.blockFlags & 0x80) {                              // bridge approach: fixed shape table
		s.bankDir = 0;
		s.bank = lo(rom_.at(0xBA8F + s.lineInBlock) + s.bank);
		return;
	}
	if (s.corridor) {                                 // pull the bank into $18..$28
		if (s.bank < 0x18) { s.bank++; return; }
		if (s.bank >= 0x29) { s.bank--; return; }
	}
	// $A84F: limits depend on the level
	uint8_t x = 1, y = 0x40;
	if (s.level < 3) {
		x = 0x1C; y = 0x24;
	} else if (!(s.level & 1) || s.corridor || s.level < 0x0A) {
		y = 0x28; x = 0x19;
	} else {
		if (s.level < 0x14) { x = 0x0D; y = 0x34; }
		if (s.decoFlags != 0) {
			if (s.decoFlags & 0x80) x = 0x19;
			else y = 0x28;
		}
	}
	s.bankMin = x;
	s.bankMax = y;

	// $A889: random walk driven by the low nibble of LFSR byte C
	uint8_t nib = s.rndC & 0x0F;
	if (s.bankDir == 0) {
		s.bankDir = nib >= 8 ? 1 : 0xFF;
	} else {
		if (nib < 8) return;                          // hold
		if (nib == 0x0D) s.bankDir ^= 0xFE;           // turn around
		else if (nib == 0x0F) {
			uint8_t w = (s.islandFlags & 0x80) ? 0x0D : 0x0E;
			if (w < s.sectionWidth || w < s.prevSectionWidth) return;
		}
	}
	// $A8C4: try one step, unless it hits the limits or the object being placed
	uint8_t c9 = s.islandWidth < s.islandTargetWidth ? s.islandTargetWidth : s.islandWidth;
	s.vC9 = c9;
	s.vC8 = lo((0x40 - c9) << 1);
	uint8_t ns = s.enteringSlot;
	bool reverse = false;
	if (s.bankDir & 0x80) {                           // moving down (-1)
		if (s.bank < s.bankMin) reverse = true;
		else if (ns < 0x0B && s.slot[ns].type >= 8) {
			uint8_t a = 0;
			bool viaFE = true;
			if (s.islandFlags & 0x80) {
				if (s.islandObjectSide & 0x80) { a = lo(s.bank + 0x80); viaFE = (a == 0); }
				if (viaFE) {
					a = lo((s.vC8 >> 1) + s.bank);
					if (a == 0) a = lo(a + s.vC8 + s.vC9);
				}
			} else {
				a = lo(s.bank + s.vC8 + s.vC9);
			}
			s.vB7 = a;
			uint8_t o = lo(s.slot[ns].x - 0x17 + s.bankMargin);
			if (s.slot[ns].size) o = lo(o + 8);
			if (o >= a) reverse = true;
		}
		if (!reverse) { s.bank--; return; }
	} else {                                          // moving up (+1)
		if (s.bank >= s.bankMax) reverse = true;
		else if (ns < 0x0B && s.slot[ns].type >= 8) {
			uint8_t a = s.bank;
			if (s.islandFlags & 0x80) {
				if (s.islandObjectSide & 0x80) {
					int c = 1;
					a = adc(s.bank, 0x3F, c);
					a = adc(a, s.vC9, c);
					if (a == 0) a = lo(a + s.vC9);
				}
			} else {
				a = lo(a + s.vC9);
			}
			a = lo(a + 2);
			s.vB7 = a;
			uint8_t o = lo(s.slot[ns].x - 0x20 - s.bankMargin);
			if (o < a) reverse = true;
		}
		if (!reverse) { s.bank++; return; }
	}
	s.bankDir ^= 0xFE;                                // $A95F
}

// $A96C-$A9D6: at the start of some blocks, pick a house or mountain for the bank.
void RiverGen::startDecoration() {
	if (s.blockFlags & 0x80) return;
	if (s.lineInBlock != 0) return;
	if (!(s.rndC & 0x20)) return;
	uint8_t x, a;
	bool haveA = false;
	if (s.level & 1) {
		x = s.rndA & 7;
		a = rom_.at(0xBBCC + x);
		haveA = !(a & 0x80);
	}
	if (!haveA) {
		x = s.sectionWidth < s.prevSectionWidth ? s.sectionWidth : s.prevSectionWidth;
		if (x < 2) return;
		if (s.islandFlags & 0x80) {
			if (x < 5) return;
			x = lo(x + 0x0B);
		}
		a = rom_.at(0xBBD4 + x);
	}
	s.decoOffset = a;
	s.decoWidth = rom_.at(0xBBB2 + x);
	x = s.decoWidth;
	s.decoRows = rom_.at(0xBBEB + x);
	// $A9B1: side: bit 7 of C, flipped when the bank is too narrow on that side
	uint8_t side = s.rndC & 0x80;
	if (side) { if (s.bank < 0x18) side ^= 0x80; }
	else if (s.bank >= 0x29) side ^= 0x80;
	s.decoFlags = lo(side + 1);
	s.decoBitmap = rom_.word(0xBBF0 + lo(x << 1));
}

// $AA8C-$AB75: draw the line. Runs of solid bytes ($55 bank, $FF water, $AA island) with
// 2-byte edge pieces from the table at $B800, plus a row of the decoration bitmap.
void RiverGen::drawLine(uint8_t out[kLineBytes]) {
	uint8_t buf[256] = {};
	int cur = 0;                                      // $B4: bytes written so far
	const uint8_t bankFill = s.flashLines ? 0xAA : 0x55;   // sub $AFF4
	// fill Y bytes (skipped when Y-1 is negative, as DEY/BMI does), return edge index of X
	auto fill = [&](uint8_t y, uint8_t value, uint8_t x) -> uint8_t {
		if (!(lo(y - 1) & 0x80)) {
			for (int i = 0; i < y; i++) buf[(cur + i) & 0xFF] = value;
			cur += y;
		}
		return lo((x & 3) << 2);
	};
	auto edgeRaw = [&](uint8_t a) {                   // sub $B03B
		buf[cur & 0xFF] = rom_.at(0xB800 + a);
		buf[(cur + 1) & 0xFF] = rom_.at(0xB800 + lo(a + 1));
		cur += 2;
	};
	auto edge = [&](uint8_t a) { edgeRaw(s.flashLines ? lo(a + 0x10) : a); };   // sub $B034
	auto copyDeco = [&]() {                           // sub $B01A
		for (int i = s.decoWidth - 1; i >= 0; i--) buf[(cur + i) & 0xFF] = rom_.at(uint16_t(s.decoBitmap + i));
		s.decoBitmap = uint16_t(s.decoBitmap + s.decoWidth);
		cur += s.decoWidth;
	};
	const bool island = s.islandFlags & 0x80;

	uint8_t a = s.bank;
	if (!island) a = lo(a + s.islandWidth);
	uint8_t x = a;
	uint8_t y = lo((a >> 2) - 1);
	if (!island && (s.decoFlags & 0x80) && s.lineInBlock < s.decoRows) {   // decoration on the left bank
		uint8_t saved = y;
		fill(s.decoOffset, bankFill, x);
		copyDeco();
		int c = 1;
		uint8_t r = sbc(saved, s.decoWidth, c);
		y = sbc(r, s.decoOffset, c);
	}
	a = fill(y, bankFill, x);                         // $AAB9 left bank
	edge(a);
	a = s.waterWidth;
	if (island) {
		if (s.islandWidth < 6) {
			int c = 0;
			a = adc(a, s.islandWidth, c);
			a = adc(a, s.islandWidth, c);
		} else {
			// $AAD2 water, island, water
			s.vB7 = x;
			a = lo((a >> 1) + x);
			x = a;
			y = lo((a >> 2) - cur);
			a = lo(fill(y, 0xFF, x) + 2);
			if (s.islandWidth < 8) a = lo(a + 0x10);
			edgeRaw(a);
			s.vB7 = x;                                // $AAEF
			int c = s.islandWidth >> 7;
			a = lo(s.islandWidth << 1);
			a = adc(a, x, c);
			x = a;
			y = lo((a >> 2) - cur - 1);
			if (s.islandWidth < 8) {
				a = lo(fill(y, 0xAA, x) + 0x10);
			} else {
				if (s.decoFlags != 0 && s.lineInBlock < s.decoRows) {   // decoration on the island
					a = lo(s.decoOffset - cur);
					s.vB7 = a;
					int c2 = 1;
					uint8_t r = sbc(y, s.vB7, c2);
					r = sbc(r, s.decoWidth, c2);
					fill(s.vB7, bankFill, x);
					copyDeco();
					y = r;
				}
				a = fill(y, bankFill, x);
			}
			edgeRaw(a);                               // $AB31
			a = s.waterWidth >> 1;
		}
	}
	// $AB37 water up to the right bank
	s.vB7 = x;
	a = lo(a + x);
	x = a;
	y = lo((a >> 2) - cur);
	a = lo(fill(y, 0xFF, x) + 2);
	edge(a);
	// $AB4B right bank to the end of the 48-byte line
	y = lo(0x30 - cur);
	if (!island && s.decoFlags != 0 && !(s.decoFlags & 0x80) && s.lineInBlock < s.decoRows) {
		int c = 1;
		uint8_t r = sbc(y, s.decoOffset, c);
		r = sbc(r, s.decoWidth, c);
		fill(lo(r + 1), bankFill, x);
		copyDeco();
		y = lo(s.decoOffset - 1);
	}
	fill(y, bankFill, x);
	std::memcpy(out, buf, kLineBytes);
}

// $AB76-$ABF2: everything scrolls one line; when the lowest slot leaves the screen the
// slots shift down by one.
void RiverGen::scrollObjects() {
	if (s.flashLines) s.flashLines--;
	s.tankY++; s.shellY++; s.missileY++;
	for (auto &sl : s.slot) sl.y++;
	if (s.slot[kSlots - 1].y == 0 && s.firstSlot < 0x0B) {
		for (int x = 9; x >= 0 && x >= s.firstSlot; x--) s.slot[x + 1] = s.slot[x];
		s.slotParity ^= 1;
		s.firstSlot++;
		if (s.shooterSlot < 0x0B) s.shooterSlot++;
		if (s.enteringSlot < 0x0B) s.enteringSlot++;
	}
}

// $ABF3-$AE94: every 16 lines a new object goes into the next free slot at the top.
void RiverGen::placeObject() {
	uint8_t x = --s.firstSlot;
	s.enteringSlot = x;
	s.wideObject = 0;
	uint8_t a = s.blockFlags, y = 1;
	s.slot[x].flags = a;
	enum { kPos, kAD29, kAC62, kAC8C } next = kPos;   // where the "chooser" continues
	int c = 0;
	if (a & 0x80) {                                   // the bridge itself
		s.sectionBankColour = lo((a & 0x04) | 0xD2);
		s.wideObject = 7;
		y = 0x0E;
		a = lo(s.islandWidth - 6);
	} else if (s.lineInBlock != 0) {                  // mid-block: maybe an enemy, odds grow with level
		a = lo(s.level - 8);
		if (!(a & 0x80)) {
			if (a >= 0x20) next = kAC62;
			else { a = lo(a << 3); if (a >= s.rndB) next = kAC62; }
		}
	} else {                                          // $AC3E block start
		y = 0;
		a = lo(s.sectionsLeft + s.blocksLeft);
		if (a < 0x12) {
			a = s.level >= s.depotLevelLimit ? s.depotLevelLimit : s.level;
			a = lo(lo(~a) + 0x3F + 1);
			a = lo(a << 1);
			if (a < s.rndB) next = kAC62;
			else if (!(s.rndA & 0x40)) { y = 0x0F; next = kAC8C; }   // fuel depot
		}
	}
	if (next == kAC62) {                              // enemy type from the LFSR
		y = rom_.at(0xBB1C + (s.rndB & 7));
		uint8_t lvl = s.level;
		bool done = false;
		if (y == 7) { if (lvl >= 5) done = true; else y = 0x0D; }
		if (!done && y == 0x0C) { if (s.lineInBlock != 0 || lvl < 3) y = 8; }
		if (y == 0x0D) s.wideObject = 5;
		next = kAC8C;
	}
	if (next == kAC8C) {                              // horizontal position
		a = s.sectionWidth;
		bool toACC2 = false;
		if (a == s.prevSectionWidth) {
			s.placeWidth = a;
			if (!(s.level & 1)) { a = s.level >> 1; toACC2 = true; }
			else {
				a = s.wideObject ? 0x37 : 0x3F;
				s.vB1 = lo(a - s.bankMargin);
				a = lo((s.rndA & 0x3F) + s.bankMargin);
				if (a >= s.vB1) a = s.vB1;
				c = 0;
				a = adc(a, s.islandWidth, c);
				next = (s.rndA & 0x80) ? kAD29 : kPos;
			}
		} else {
			s.placeWidth = s.sectionWidth >= s.prevSectionWidth ? s.sectionWidth : s.prevSectionWidth;
			a = s.placeWidth;
			toACC2 = true;
		}
		if (toACC2) {
			uint8_t w = (s.islandFlags & 0x80) ? 0x0D : 0x0E;
			if (w < s.placeWidth && y != 7) {
				y = 1;                                // too narrow: nothing
				next = kPos;
			} else {
				// $ACD5
				bool toACEC = false;
				if (lo(s.sectionsLeft - 1) == 0) { s.bankMargin = lo(s.bankMargin + 2); toACEC = true; }
				else {
					uint8_t w2 = (s.islandFlags & 0x80) ? 0x0A : 0x0D;
					if (w2 < s.placeWidth) toACEC = true;
				}
				if (toACEC && y == 0x0D) { s.wideObject = 0; y = 8; }
				// $ACF6
				c = (s.placeWidth >> 6) & 1;            // carry after two ASL
				a = lo(s.placeWidth << 2);
				enum { kAD10, kAD14, kAD19, kAD1D } p = kAD10;
				if (a != 0 && (s.islandFlags & 0x80)) {
					a = adc(lo(~a), 0x41, c);
					s.islandObjectSide = s.rndA;
					if (!(s.rndA & 0x80)) p = kAD1D;
					else {
						a = adc(lo(~a), 0x80, c);
						p = a != 0 ? kAD14 : kAD10;
					}
				}
				if (p == kAD10) p = (s.rndA & 0x80) ? kAD19 : kAD14;
				if (p == kAD14) {
					c = 1;
					a = adc(a, s.bankMargin, c);
					p = a != 0 ? kAD1D : kAD19;
					if (a != 0) { next = kPos; goto position; }
				}
				if (p == kAD19) a = adc(lo(~a), 0x81, c);
				// $AD1D
				c = y >= 0x0F;
				a = sbc(a, 8, c);
				a = sbc(a, s.bankMargin, c);
				if (s.wideObject) a = sbc(a, 8, c);
				next = kAD29;
			}
		}
	}
	if (next == kAD29 && y != 0x0F) s.slot[s.enteringSlot].flags |= 0x08;
position:
	// $AD37: store the object
	x = s.enteringSlot;
	s.vCA = lo(a + 0x20);
	{
		uint8_t b = s.bank;
		if (s.corridor & 0x80) { if (b < 0x18) b = 0x18; if (b >= 0x28) b = 0x28; }
		s.slot[x].x = lo(b + s.vCA);
	}
	s.slot[x].kind = y;
	s.slot[x].type = y;
	if (y == 8 && s.shooterSlot >= 0x0B && s.sectionsLeft < 0x0E && !(s.rndC & 0x70) && s.level >= 0x0D) {
		s.slot[x].kind = s.slot[x].type = 0x0A;      // helicopter becomes a jet
		s.missileX = 0;
		s.shooterSlot = x;
	}
	s.enteringLines = lo(rom_.at(0xBB60 + y) + 4);
	s.slot[x].size = rom_.at(0xBBA2 + y);
	s.slot[x].y = lo(0x61 - s.enteringLines);
	if ((s.lineInBlock | s.tankKind) == 0) {
		if (s.blockFlags & 0x80) {                          // $ADA6: tank on the bridge road
			s.tankY = 0x0B;
			uint8_t k = 5;
			s.tankFires = 0; s.shellSpeedFrac = 0; s.shellSpeed = 2;
			a = s.rndA & 0x7F;
			if (a >= 0x40) k++;
			s.tankKind = k;
			s.tankX = lo(a + 0x40);
		} else {                                      // $ADCD: maybe something on the bank road
			x = s.enteringSlot;
			uint8_t w = s.sectionWidth < s.prevSectionWidth ? s.sectionWidth : s.prevSectionWidth;
			if (s.slot[x].kind >= 8 && s.level >= 7 && s.sectionsLeft >= 5 && s.sectionsLeft < 0x0E &&
				!(s.rndC & 0xC0) && w >= 2 && w < 0x0E) {
				a = s.rndC >> 3;
				if (s.level == 0x0A) a ^= 7;
				a &= 6;
				if (a == 0 && !(s.islandFlags & 0x80) && w < 8) a = 8;
				s.shellSpeed = rom_.at(0xB7F4 + a);
				s.shellSpeedFrac = rom_.at(0xB7F5 + a);
				y = 5;
				bool store = true;
				if (s.islandFlags & 0x80) {
					if (s.decoFlags != 0 || w >= 0x0B || w < 4) store = false;
					else { if (s.rndC & 1) y++; a = lo(0x4C + 0x30); }
				} else if (s.level & 1) {
					a = s.rndC & 0x1F;
					bool flip;
					if (a >= 0x10) {
						a = lo(a + 0x58);
						if (s.decoFlags == 0 || (s.decoFlags & 0x80)) { y++; flip = false; } else flip = true;
					} else {
						if (!(s.decoFlags & 0x80)) flip = false; else { y++; flip = true; }
					}
					if (flip) a = lo(lo(~a) + 0x78 + 1);
					a = lo(a + s.bank + 0x20);
				} else {
					a = rom_.at(0xBFE4 + w);
					bool flip;
					if (a >= 0x50) {
						if (s.decoFlags == 0 || (s.decoFlags & 0x80)) { y++; flip = false; } else flip = true;
					} else {
						if (!(s.decoFlags & 0x80)) flip = false; else { y++; flip = true; }
					}
					if (flip) a = lo(lo(~a) + 0x98 + 1);
					a = lo(a + 0x30);
				}
				if (store) {
					s.tankX = a; s.tankKind = y; s.tankY = 0x0C;
					s.shellTimer = 0xFF; s.tankFires = 0xFF;
				}
			}
		}
	}
}

// Respawn (also used for the game start): sub $B0F1 resets the river and puts a bridge at
// the bottom of the screen, then $A3C8 restores LFSR and bank from the checkpoint.
// Fields the generator always rewrites before reading (scratch values, unused slots,
// decoration data while no decoration is active) keep whatever they held.
void RiverGen::restoreCheckpoint() {
	// $B0F1: $4D-$5F from the table at $BAE1
	s.firstSlot = rom_.at(0xBAE1 + 0x00);   // $4D
	s.enteringSlot = rom_.at(0xBAE1 + 0x01);     // $4E
	s.flashLines = rom_.at(0xBAE1 + 0x02);       // $4F
	s.shellTimer = rom_.at(0xBAE1 + 0x03);         // $50
	s.shooterSlot = rom_.at(0xBAE1 + 0x04);     // $51
	s.bank = rom_.at(0xBAE1 + 0x05);        // $52
	s.waterWidth = rom_.at(0xBAE1 + 0x06);       // $53
	s.islandWidth = rom_.at(0xBAE1 + 0x07);     // $54
	s.blocksLeft = rom_.at(0xBAE1 + 0x0D);  // $5A
	s.sectionsLeft = rom_.at(0xBAE1 + 0x10);// $5D
	s.sectionWidth = rom_.at(0xBAE1 + 0x12);   // $5F
	// $2B-$4C cleared
	s.prevSectionWidth = s.islandFlags = s.bridgeHit = s.lineInBlock = 0;
	s.enteringLines = s.slotParity = s.islandTargetWidth = s.islandStep = s.decoFlags = s.islandObjectSide = 0;
	s.tankKind = s.tankX = s.tankY = s.shellSpeed = s.shellSpeedFrac = s.shellY = s.tankFires = 0;
	// $0500-$05FF cleared: object kinds, positions, types, sprite sizes, corridor flag
	for (auto &sl : s.slot) { sl.kind = sl.x = sl.type = sl.size = sl.flags = 0; }
	s.corridor = 0;
	// $B15E: the bridge we restart from, in the lowest slot
	s.enteringLines = 0x17;
	s.slot[10].type = 0x0E;
	s.slot[10].y = 0x4A;
	s.sectionBankColour = (s.level & 1) ? 0xD6 : 0xD2;
	s.blockFlags = s.sectionBankColour & 0x84;
	s.slot[10].flags = s.blockFlags;
	s.firstSlot = s.enteringSlot = 0x0A;
	// $A3C8: continue the river from the checkpoint
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
}

void RiverGen::startGame(int gameNumber, bool pal) {
	s = RiverState{};
	s.depotLevelLimit = pal ? 0x2F : 0x2E;                        // $A077
	// $AF9B: starting point of the selected game (2 games per starting bridge)
	int v = ((gameNumber - 1) & 7) >> 1;
	s.checkpoint[0] = rom_.at(0xB494 + v);
	s.checkpoint[1] = rom_.at(0xB498 + v);
	s.checkpoint[2] = rom_.at(0xB49C + v);
	s.checkpoint[3] = rom_.at(0xB4A0 + v);
	s.level = rom_.at(0xB4A4 + v);
	restoreCheckpoint();
}

void RiverGen::nextLine(uint8_t line[kLineBytes]) {
	structureAndWidth();
	smoothIsland();
	moveBank();
	startDecoration();
	drawLine(line);
	scrollObjects();
	// $ABE1: the newest object still occupies the top lines
	if (s.enteringSlot < 0x0B) {
		if (!(lo(s.enteringLines - 1) & 0x80)) { s.enteringLines--; return; }
		s.enteringSlot = 0x0B;
	}
	if ((s.lineInBlock & 0x0F) != 0 || s.firstSlot == 0) return;
	placeObject();
	s.enteringLines--;                                     // $AE94
}

}  // namespace rr
