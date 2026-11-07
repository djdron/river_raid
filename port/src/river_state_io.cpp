// Mapping of RiverState fields to the original's RAM, for loading and byte-exact comparison.
#include <cstdio>

#include "river_gen.h"

namespace rr {
namespace {

struct Field {
	const char *name;
	uint16_t addr;
	bool gameOwned;   // also changed by the rest of the game: injected, not compared
	uint8_t *(*ref)(RiverState &);
};

#define F(name, addr, owned, expr) {name, addr, owned, [](RiverState &s) -> uint8_t * { return &(expr); }}

const Field kFields[] = {
	F("lineInBlock", 0x2F, false, s.lineInBlock),
	F("blocksLeft", 0x5A, false, s.blocksLeft),
	F("sectionsLeft", 0x5D, false, s.sectionsLeft),
	F("level", 0x60, false, s.level),
	F("blockFlags", 0x23, false, s.blockFlags),
	F("islandFlags", 0x2D, false, s.islandFlags),
	F("corridor", 0x052D, false, s.corridor),
	F("bridgeHit", 0x2E, true, s.bridgeHit),
	F("rndA", 0xAA, false, s.rndA),
	F("rndB", 0xAB, false, s.rndB),
	F("rndC", 0xAC, false, s.rndC),
	F("checkpoint0", 0x66, false, s.checkpoint[0]), F("checkpoint1", 0x67, false, s.checkpoint[1]),
	F("checkpoint2", 0x68, false, s.checkpoint[2]), F("checkpoint3", 0x69, false, s.checkpoint[3]),
	F("prevCheckpoint0", 0xAD, false, s.prevCheckpoint[0]), F("prevCheckpoint1", 0xAE, false, s.prevCheckpoint[1]),
	F("prevCheckpoint2", 0xAF, false, s.prevCheckpoint[2]), F("prevCheckpoint3", 0xB0, false, s.prevCheckpoint[3]),
	F("bank", 0x52, false, s.bank),
	F("bankDir", 0xC3, false, s.bankDir),
	F("bankMin", 0xC4, false, s.bankMin),
	F("bankMax", 0xC5, false, s.bankMax),
	F("waterWidth", 0x53, false, s.waterWidth),
	F("islandWidth", 0x54, false, s.islandWidth),
	F("islandTargetWidth", 0x34, false, s.islandTargetWidth),
	F("islandStep", 0x35, false, s.islandStep),
	F("sectionWidth", 0x5F, false, s.sectionWidth),
	F("prevSectionWidth", 0x2C, false, s.prevSectionWidth),
	F("bankMargin", 0xB5, false, s.bankMargin),
	F("decoFlags", 0x36, false, s.decoFlags),
	F("islandObjectSide", 0x37, false, s.islandObjectSide),
	F("decoPtrLo", 0xBA, false, *reinterpret_cast<uint8_t *>(&s.decoBitmap)),
	F("decoPtrHi", 0xBB, false, *(reinterpret_cast<uint8_t *>(&s.decoBitmap) + 1)),
	F("decoWidth", 0xBC, false, s.decoWidth),
	F("decoOffset", 0xBD, false, s.decoOffset),
	F("decoRows", 0xBE, false, s.decoRows),
	F("firstSlot", 0x4D, false, s.firstSlot),
	F("enteringSlot", 0x4E, false, s.enteringSlot),
	F("shooterSlot", 0x51, true, s.shooterSlot),
	F("enteringLines", 0x32, false, s.enteringLines),
	F("slotParity", 0x33, true, s.slotParity),
	F("sectionBankColour", 0x0B, true, s.sectionBankColour),
	F("wideObject", 0xB8, false, s.wideObject),
	F("placeWidth", 0xB3, false, s.placeWidth),
	F("tankKind", 0x38, true, s.tankKind),
	F("tankX", 0x39, true, s.tankX),
	F("tankY", 0x3A, true, s.tankY),
	F("shellSpeed", 0x3E, true, s.shellSpeed), F("shellSpeedFrac", 0x3F, true, s.shellSpeedFrac), F("shellY", 0x40, true, s.shellY),
	F("tankFires", 0x41, true, s.tankFires), F("shellTimer", 0x50, true, s.shellTimer), F("missileX", 0xC1, true, s.missileX),
	F("missileY", 0xC2, true, s.missileY),
	F("flashLines", 0x4F, true, s.flashLines),
	F("depotLevelLimit", 0xCB, false, s.depotLevelLimit),
};
#undef F

template <class Fn>
void forEachSlotField(Fn fn) {
	for (int i = 0; i < kSlots; i++) {
		fn("slot.y", i, uint16_t(0x0C + i), false, &Slot::y);
		fn("slot.x", i, uint16_t(0x0516 + i), true, &Slot::x);
		fn("slot.flags", i, uint16_t(0x42 + i), true, &Slot::flags);
		fn("slot.kind", i, uint16_t(0x0500 + i), true, &Slot::kind);
		fn("slot.type", i, uint16_t(0x050B + i), false, &Slot::type);
		fn("slot.size", i, uint16_t(0x0521 + i), false, &Slot::size);
	}
}

}  // namespace

void RiverGen::loadFromRam(const uint8_t *ram) {
	for (const Field &f : kFields) *f.ref(s) = ram[f.addr];
	forEachSlotField([&](const char *, int i, uint16_t addr, bool, uint8_t Slot::*m) { s.slot[i].*m = ram[addr]; });
}

void RiverGen::saveToRam(uint8_t *ram) const {
	RiverState copy = s;
	for (const Field &f : kFields) ram[f.addr] = *f.ref(copy);
	forEachSlotField([&](const char *, int i, uint16_t addr, bool, uint8_t Slot::*m) { ram[addr] = copy.slot[i].*m; });
}

void RiverGen::loadGameOwned(const uint8_t *ram) {
	for (const Field &f : kFields)
		if (f.gameOwned) *f.ref(s) = ram[f.addr];
	forEachSlotField([&](const char *, int i, uint16_t addr, bool owned, uint8_t Slot::*m) {
		if (owned) s.slot[i].*m = ram[addr];
	});
}

std::vector<std::string> RiverGen::diffWithRam(const uint8_t *ram, bool includeGameOwned) const {
	std::vector<std::string> out;
	RiverState copy = s;
	char buf[160];
	for (const Field &f : kFields) {
		if (f.gameOwned && !includeGameOwned) continue;
		uint8_t v = *f.ref(copy);
		if (v != ram[f.addr]) {
			std::snprintf(buf, sizeof buf, "%s $%02X: port=%02X orig=%02X", f.name, f.addr, v, ram[f.addr]);
			out.push_back(buf);
		}
	}
	forEachSlotField([&](const char *name, int i, uint16_t addr, bool owned, uint8_t Slot::*m) {
		if (owned && !includeGameOwned) return;
		if (i < ram[0x4D]) return;   // free slot: every field is rewritten before it is read
		uint8_t v = copy.slot[i].*m;
		if (v != ram[addr]) {
			std::snprintf(buf, sizeof buf, "%s[%d] $%04X: port=%02X orig=%02X", name, i, addr, v, ram[addr]);
			out.push_back(buf);
		}
	});
	return out;
}

}  // namespace rr
