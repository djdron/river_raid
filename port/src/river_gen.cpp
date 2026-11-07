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
	std::memcpy(s.savePrev, s.save, 4);
	s.save[0] = s.rndA; s.save[1] = s.rndB; s.save[2] = s.rndC; s.save[3] = s.bank;
	s.level = lo(lvl + 1);
}

// $A711-$A7FA: line counter, blocks of 32 lines, sections of 2 blocks, 16 sections per
// bridge. At each section start a new target width is chosen.
void RiverGen::structureAndWidth() {
	s.margin = s.level < 9 ? 6 : 0;
	s.lineInBlock++;
	if (s.lineInBlock < 0x20) {                       // ordinary line
		if (!(s.mode & 0x80)) stepLfsr();             // no randomness on a bridge approach
		return;
	}
	// $A72B: new block
	s.lineInBlock = 0;
	s.deco = 0;
	s.corridor = 0;
	s.mode = lo((s.mode & 0x04) | 0x10);
	s.widthCodePrev = s.widthCode;
	if (--s.blocksLeft != 0) {                        // second block of the section
		if (lo(s.sectionsLeft - 1) == 0) {            // $A749: last section: bridge follows
			s.bridgeHit = 0;
			s.mode = (s.level & 1) ? 0x80 : 0x84;
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
		s.shape = 0;
		s.widthCode = 0x0D;
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
				if (s.shape == 0xC0) toA79E = true; else toA7B2 = true;
			} else {                                  // $A793
				uint8_t t = lo(s.shape << 1) ^ s.shape;
				if (t & 0x80) toA7AC = true;
				else if (!(s.rndA & 0x30)) toA79E = true;
			}
			if (toA79E) {                             // $A79E: switch island bits, width 0
				a = s.shape & 0x80;
				if (a == 0) a |= 0x40;
				s.shape = a;
				a = 0;
			} else {
				if (toA7AC) s.shape = (s.shape & 0x40) ? 0xC0 : 0x00;   // $A7AC
				else if (toA7B2) s.shape = 0;                          // $A7B2
				// $A7B6: random width code 2..15 with limits
				uint8_t y = 0x0F;
				a = s.rndA & 0x0F;
				if (a < 2) a += 2;
				if (s.shape & 0x80) y--;
				if (s.level < 0x14) y--;
				if (s.margin != 0) y = 8;
				s.vB1 = y;
				if (a >= y) a = y;
			}
		}
		s.widthCode = a;                              // $A7DC
		a = lo(a << 2);
	}
	// $A7E0: new island target; step +-1, or +-2 when the change is 32 or more
	s.blocksLeft = 2;
	s.islandTarget = a;
	uint8_t diff = lo(a - s.islandW);
	if (diff == 0) return;
	if (a < s.islandW) s.islandStep = diff >= 0xE0 ? 0xFF : 0xFE;
	else s.islandStep = diff >= 0x20 ? 2 : 1;
}

// $A7FB-$A825: the island moves toward its target; the water narrows by twice as much.
void RiverGen::smoothIsland() {
	if (s.islandStep == 0) return;
	bool reached = (s.islandStep & 0x80) ? s.islandTarget >= s.islandW : s.islandW >= s.islandTarget;
	if (reached) { s.islandStep = 0; return; }
	s.islandW = lo(s.islandW + s.islandStep);
	s.water = lo(s.water - s.islandStep - s.islandStep);
}

// $A826-$A96B: bank position for this line.
void RiverGen::moveBank() {
	if (s.mode & 0x80) {                              // bridge approach: fixed shape table
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
		if (s.deco != 0) {
			if (s.deco & 0x80) x = 0x19;
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
			uint8_t w = (s.shape & 0x80) ? 0x0D : 0x0E;
			if (w < s.widthCode || w < s.widthCodePrev) return;
		}
	}
	// $A8C4: try one step, unless it hits the limits or the object being placed
	uint8_t c9 = s.islandW < s.islandTarget ? s.islandTarget : s.islandW;
	s.vC9 = c9;
	s.vC8 = lo((0x40 - c9) << 1);
	uint8_t ns = s.newSlot;
	bool reverse = false;
	if (s.bankDir & 0x80) {                           // moving down (-1)
		if (s.bank < s.bankMin) reverse = true;
		else if (ns < 0x0B && s.slot[ns].type >= 8) {
			uint8_t a = 0;
			bool viaFE = true;
			if (s.shape & 0x80) {
				if (s.deco37 & 0x80) { a = lo(s.bank + 0x80); viaFE = (a == 0); }
				if (viaFE) {
					a = lo((s.vC8 >> 1) + s.bank);
					if (a == 0) a = lo(a + s.vC8 + s.vC9);
				}
			} else {
				a = lo(s.bank + s.vC8 + s.vC9);
			}
			s.vB7 = a;
			uint8_t o = lo(s.slot[ns].x - 0x17 + s.margin);
			if (s.slot[ns].size) o = lo(o + 8);
			if (o >= a) reverse = true;
		}
		if (!reverse) { s.bank--; return; }
	} else {                                          // moving up (+1)
		if (s.bank >= s.bankMax) reverse = true;
		else if (ns < 0x0B && s.slot[ns].type >= 8) {
			uint8_t a = s.bank;
			if (s.shape & 0x80) {
				if (s.deco37 & 0x80) {
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
			uint8_t o = lo(s.slot[ns].x - 0x20 - s.margin);
			if (o < a) reverse = true;
		}
		if (!reverse) { s.bank++; return; }
	}
	s.bankDir ^= 0xFE;                                // $A95F
}

// $A96C-$A9D6: at the start of some blocks, pick a house or mountain for the bank.
void RiverGen::startDecoration() {
	if (s.mode & 0x80) return;
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
		x = s.widthCode < s.widthCodePrev ? s.widthCode : s.widthCodePrev;
		if (x < 2) return;
		if (s.shape & 0x80) {
			if (x < 5) return;
			x = lo(x + 0x0B);
		}
		a = rom_.at(0xBBD4 + x);
	}
	s.decoOfs = a;
	s.decoW = rom_.at(0xBBB2 + x);
	x = s.decoW;
	s.decoRows = rom_.at(0xBBEB + x);
	// $A9B1: side: bit 7 of C, flipped when the bank is too narrow on that side
	uint8_t side = s.rndC & 0x80;
	if (side) { if (s.bank < 0x18) side ^= 0x80; }
	else if (s.bank >= 0x29) side ^= 0x80;
	s.deco = lo(side + 1);
	s.decoPtr = rom_.word(0xBBF0 + lo(x << 1));
}

// $AA8C-$AB75: draw the line. Runs of solid bytes ($55 bank, $FF water, $AA island) with
// 2-byte edge pieces from the table at $B800, plus a row of the decoration bitmap.
void RiverGen::drawLine(uint8_t out[kLineBytes]) {
	uint8_t buf[256] = {};
	int cur = 0;                                      // $B4: bytes written so far
	const uint8_t bankFill = s.flash ? 0xAA : 0x55;   // sub $AFF4
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
	auto edge = [&](uint8_t a) { edgeRaw(s.flash ? lo(a + 0x10) : a); };   // sub $B034
	auto copyDeco = [&]() {                           // sub $B01A
		for (int i = s.decoW - 1; i >= 0; i--) buf[(cur + i) & 0xFF] = rom_.at(uint16_t(s.decoPtr + i));
		s.decoPtr = uint16_t(s.decoPtr + s.decoW);
		cur += s.decoW;
	};
	const bool island = s.shape & 0x80;

	uint8_t a = s.bank;
	if (!island) a = lo(a + s.islandW);
	uint8_t x = a;
	uint8_t y = lo((a >> 2) - 1);
	if (!island && (s.deco & 0x80) && s.lineInBlock < s.decoRows) {   // decoration on the left bank
		uint8_t saved = y;
		fill(s.decoOfs, bankFill, x);
		copyDeco();
		int c = 1;
		uint8_t r = sbc(saved, s.decoW, c);
		y = sbc(r, s.decoOfs, c);
	}
	a = fill(y, bankFill, x);                         // $AAB9 left bank
	edge(a);
	a = s.water;
	if (island) {
		if (s.islandW < 6) {
			int c = 0;
			a = adc(a, s.islandW, c);
			a = adc(a, s.islandW, c);
		} else {
			// $AAD2 water, island, water
			s.vB7 = x;
			a = lo((a >> 1) + x);
			x = a;
			y = lo((a >> 2) - cur);
			a = lo(fill(y, 0xFF, x) + 2);
			if (s.islandW < 8) a = lo(a + 0x10);
			edgeRaw(a);
			s.vB7 = x;                                // $AAEF
			int c = s.islandW >> 7;
			a = lo(s.islandW << 1);
			a = adc(a, x, c);
			x = a;
			y = lo((a >> 2) - cur - 1);
			if (s.islandW < 8) {
				a = lo(fill(y, 0xAA, x) + 0x10);
			} else {
				if (s.deco != 0 && s.lineInBlock < s.decoRows) {   // decoration on the island
					a = lo(s.decoOfs - cur);
					s.vB7 = a;
					int c2 = 1;
					uint8_t r = sbc(y, s.vB7, c2);
					r = sbc(r, s.decoW, c2);
					fill(s.vB7, bankFill, x);
					copyDeco();
					y = r;
				}
				a = fill(y, bankFill, x);
			}
			edgeRaw(a);                               // $AB31
			a = s.water >> 1;
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
	if (!island && s.deco != 0 && !(s.deco & 0x80) && s.lineInBlock < s.decoRows) {
		int c = 1;
		uint8_t r = sbc(y, s.decoOfs, c);
		r = sbc(r, s.decoW, c);
		fill(lo(r + 1), bankFill, x);
		copyDeco();
		y = lo(s.decoOfs - 1);
	}
	fill(y, bankFill, x);
	std::memcpy(out, buf, kLineBytes);
}

// $AB76-$ABF2: everything scrolls one line; when the lowest slot leaves the screen the
// slots shift down by one.
void RiverGen::scrollObjects() {
	if (s.flash) s.flash--;
	s.roadY++; s.v40++; s.vC2++;
	for (auto &sl : s.slot) sl.y++;
	if (s.slot[kSlots - 1].y == 0 && s.firstSlot < 0x0B) {
		for (int x = 9; x >= 0 && x >= s.firstSlot; x--) s.slot[x + 1] = s.slot[x];
		s.toggle33 ^= 1;
		s.firstSlot++;
		if (s.jetSlot < 0x0B) s.jetSlot++;
		if (s.newSlot < 0x0B) s.newSlot++;
	}
}

// $ABF3-$AE94: every 16 lines a new object goes into the next free slot at the top.
void RiverGen::placeObject() {
	uint8_t x = --s.firstSlot;
	s.newSlot = x;
	s.objFlag = 0;
	uint8_t a = s.mode, y = 1;
	s.slot[x].flags = a;
	enum { kPos, kAD29, kAC62, kAC8C } next = kPos;   // where the "chooser" continues
	int c = 0;
	if (a & 0x80) {                                   // the bridge itself
		s.bridgeFlags = lo((a & 0x04) | 0xD2);
		s.objFlag = 7;
		y = 0x0E;
		a = lo(s.islandW - 6);
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
			a = s.level >= s.vCB ? s.vCB : s.level;
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
		if (y == 0x0D) s.objFlag = 5;
		next = kAC8C;
	}
	if (next == kAC8C) {                              // horizontal position
		a = s.widthCode;
		bool toACC2 = false;
		if (a == s.widthCodePrev) {
			s.objWidth = a;
			if (!(s.level & 1)) { a = s.level >> 1; toACC2 = true; }
			else {
				a = s.objFlag ? 0x37 : 0x3F;
				s.vB1 = lo(a - s.margin);
				a = lo((s.rndA & 0x3F) + s.margin);
				if (a >= s.vB1) a = s.vB1;
				c = 0;
				a = adc(a, s.islandW, c);
				next = (s.rndA & 0x80) ? kAD29 : kPos;
			}
		} else {
			s.objWidth = s.widthCode >= s.widthCodePrev ? s.widthCode : s.widthCodePrev;
			a = s.objWidth;
			toACC2 = true;
		}
		if (toACC2) {
			uint8_t w = (s.shape & 0x80) ? 0x0D : 0x0E;
			if (w < s.objWidth && y != 7) {
				y = 1;                                // too narrow: nothing
				next = kPos;
			} else {
				// $ACD5
				bool toACEC = false;
				if (lo(s.sectionsLeft - 1) == 0) { s.margin = lo(s.margin + 2); toACEC = true; }
				else {
					uint8_t w2 = (s.shape & 0x80) ? 0x0A : 0x0D;
					if (w2 < s.objWidth) toACEC = true;
				}
				if (toACEC && y == 0x0D) { s.objFlag = 0; y = 8; }
				// $ACF6
				c = (s.objWidth >> 6) & 1;            // carry after two ASL
				a = lo(s.objWidth << 2);
				enum { kAD10, kAD14, kAD19, kAD1D } p = kAD10;
				if (a != 0 && (s.shape & 0x80)) {
					a = adc(lo(~a), 0x41, c);
					s.deco37 = s.rndA;
					if (!(s.rndA & 0x80)) p = kAD1D;
					else {
						a = adc(lo(~a), 0x80, c);
						p = a != 0 ? kAD14 : kAD10;
					}
				}
				if (p == kAD10) p = (s.rndA & 0x80) ? kAD19 : kAD14;
				if (p == kAD14) {
					c = 1;
					a = adc(a, s.margin, c);
					p = a != 0 ? kAD1D : kAD19;
					if (a != 0) { next = kPos; goto position; }
				}
				if (p == kAD19) a = adc(lo(~a), 0x81, c);
				// $AD1D
				c = y >= 0x0F;
				a = sbc(a, 8, c);
				a = sbc(a, s.margin, c);
				if (s.objFlag) a = sbc(a, 8, c);
				next = kAD29;
			}
		}
	}
	if (next == kAD29 && y != 0x0F) s.slot[s.newSlot].flags |= 0x08;
position:
	// $AD37: store the object
	x = s.newSlot;
	s.vCA = lo(a + 0x20);
	{
		uint8_t b = s.bank;
		if (s.corridor & 0x80) { if (b < 0x18) b = 0x18; if (b >= 0x28) b = 0x28; }
		s.slot[x].x = lo(b + s.vCA);
	}
	s.slot[x].kind = y;
	s.slot[x].type = y;
	if (y == 8 && s.jetSlot >= 0x0B && s.sectionsLeft < 0x0E && !(s.rndC & 0x70) && s.level >= 0x0D) {
		s.slot[x].kind = s.slot[x].type = 0x0A;      // helicopter becomes a jet
		s.vC1 = 0;
		s.jetSlot = x;
	}
	s.objLines = lo(rom_.at(0xBB60 + y) + 4);
	s.slot[x].size = rom_.at(0xBBA2 + y);
	s.slot[x].y = lo(0x61 - s.objLines);
	if ((s.lineInBlock | s.roadKind) == 0) {
		if (s.mode & 0x80) {                          // $ADA6: tank on the bridge road
			s.roadY = 0x0B;
			uint8_t k = 5;
			s.v41 = 0; s.v3F = 0; s.v3E = 2;
			a = s.rndA & 0x7F;
			if (a >= 0x40) k++;
			s.roadKind = k;
			s.roadX = lo(a + 0x40);
		} else {                                      // $ADCD: maybe something on the bank road
			x = s.newSlot;
			uint8_t w = s.widthCode < s.widthCodePrev ? s.widthCode : s.widthCodePrev;
			if (s.slot[x].kind >= 8 && s.level >= 7 && s.sectionsLeft >= 5 && s.sectionsLeft < 0x0E &&
				!(s.rndC & 0xC0) && w >= 2 && w < 0x0E) {
				a = s.rndC >> 3;
				if (s.level == 0x0A) a ^= 7;
				a &= 6;
				if (a == 0 && !(s.shape & 0x80) && w < 8) a = 8;
				s.v3E = rom_.at(0xB7F4 + a);
				s.v3F = rom_.at(0xB7F5 + a);
				y = 5;
				bool store = true;
				if (s.shape & 0x80) {
					if (s.deco != 0 || w >= 0x0B || w < 4) store = false;
					else { if (s.rndC & 1) y++; a = lo(0x4C + 0x30); }
				} else if (s.level & 1) {
					a = s.rndC & 0x1F;
					bool flip;
					if (a >= 0x10) {
						a = lo(a + 0x58);
						if (s.deco == 0 || (s.deco & 0x80)) { y++; flip = false; } else flip = true;
					} else {
						if (!(s.deco & 0x80)) flip = false; else { y++; flip = true; }
					}
					if (flip) a = lo(lo(~a) + 0x78 + 1);
					a = lo(a + s.bank + 0x20);
				} else {
					a = rom_.at(0xBFE4 + w);
					bool flip;
					if (a >= 0x50) {
						if (s.deco == 0 || (s.deco & 0x80)) { y++; flip = false; } else flip = true;
					} else {
						if (!(s.deco & 0x80)) flip = false; else { y++; flip = true; }
					}
					if (flip) a = lo(lo(~a) + 0x98 + 1);
					a = lo(a + 0x30);
				}
				if (store) {
					s.roadX = a; s.roadKind = y; s.roadY = 0x0C;
					s.v50 = 0xFF; s.v41 = 0xFF;
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
	s.newSlot = rom_.at(0xBAE1 + 0x01);     // $4E
	s.flash = rom_.at(0xBAE1 + 0x02);       // $4F
	s.v50 = rom_.at(0xBAE1 + 0x03);         // $50
	s.jetSlot = rom_.at(0xBAE1 + 0x04);     // $51
	s.bank = rom_.at(0xBAE1 + 0x05);        // $52
	s.water = rom_.at(0xBAE1 + 0x06);       // $53
	s.islandW = rom_.at(0xBAE1 + 0x07);     // $54
	s.blocksLeft = rom_.at(0xBAE1 + 0x0D);  // $5A
	s.sectionsLeft = rom_.at(0xBAE1 + 0x10);// $5D
	s.widthCode = rom_.at(0xBAE1 + 0x12);   // $5F
	// $2B-$4C cleared
	s.widthCodePrev = s.shape = s.bridgeHit = s.lineInBlock = 0;
	s.objLines = s.toggle33 = s.islandTarget = s.islandStep = s.deco = s.deco37 = 0;
	s.roadKind = s.roadX = s.roadY = s.v3E = s.v3F = s.v40 = s.v41 = 0;
	// $0500-$05FF cleared: object kinds, positions, types, sprite sizes, corridor flag
	for (auto &sl : s.slot) { sl.kind = sl.x = sl.type = sl.size = sl.flags = 0; }
	s.corridor = 0;
	// $B15E: the bridge we restart from, in the lowest slot
	s.objLines = 0x17;
	s.slot[10].type = 0x0E;
	s.slot[10].y = 0x4A;
	s.bridgeFlags = (s.level & 1) ? 0xD6 : 0xD2;
	s.mode = s.bridgeFlags & 0x84;
	s.slot[10].flags = s.mode;
	s.firstSlot = s.newSlot = 0x0A;
	// $A3C8: continue the river from the checkpoint
	s.rndA = s.save[0];
	s.rndB = s.save[1];
	s.rndC = s.save[2];
	s.bank = s.save[3];
}

void RiverGen::startGame(int gameNumber, bool pal) {
	s = RiverState{};
	s.vCB = pal ? 0x2F : 0x2E;                        // $A077
	// $AF9B: starting point of the selected game (2 games per starting bridge)
	int v = ((gameNumber - 1) & 7) >> 1;
	s.save[0] = rom_.at(0xB494 + v);
	s.save[1] = rom_.at(0xB498 + v);
	s.save[2] = rom_.at(0xB49C + v);
	s.save[3] = rom_.at(0xB4A0 + v);
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
	if (s.newSlot < 0x0B) {
		if (!(lo(s.objLines - 1) & 0x80)) { s.objLines--; return; }
		s.newSlot = 0x0B;
	}
	if ((s.lineInBlock & 0x0F) != 0 || s.firstSlot == 0) return;
	placeObject();
	s.objLines--;                                     // $AE94
}

}  // namespace rr
