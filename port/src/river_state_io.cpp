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
	F("mode", 0x23, false, s.mode),
	F("shape", 0x2D, false, s.shape),
	F("corridor", 0x052D, false, s.corridor),
	F("bridgeHit", 0x2E, true, s.bridgeHit),
	F("rndA", 0xAA, false, s.rndA),
	F("rndB", 0xAB, false, s.rndB),
	F("rndC", 0xAC, false, s.rndC),
	F("save0", 0x66, false, s.save[0]), F("save1", 0x67, false, s.save[1]),
	F("save2", 0x68, false, s.save[2]), F("save3", 0x69, false, s.save[3]),
	F("savePrev0", 0xAD, false, s.savePrev[0]), F("savePrev1", 0xAE, false, s.savePrev[1]),
	F("savePrev2", 0xAF, false, s.savePrev[2]), F("savePrev3", 0xB0, false, s.savePrev[3]),
	F("bank", 0x52, false, s.bank),
	F("bankDir", 0xC3, false, s.bankDir),
	F("bankMin", 0xC4, false, s.bankMin),
	F("bankMax", 0xC5, false, s.bankMax),
	F("water", 0x53, false, s.water),
	F("islandW", 0x54, false, s.islandW),
	F("islandTarget", 0x34, false, s.islandTarget),
	F("islandStep", 0x35, false, s.islandStep),
	F("widthCode", 0x5F, false, s.widthCode),
	F("widthCodePrev", 0x2C, false, s.widthCodePrev),
	F("margin", 0xB5, false, s.margin),
	F("deco", 0x36, false, s.deco),
	F("deco37", 0x37, false, s.deco37),
	F("decoPtrLo", 0xBA, false, *reinterpret_cast<uint8_t *>(&s.decoPtr)),
	F("decoPtrHi", 0xBB, false, *(reinterpret_cast<uint8_t *>(&s.decoPtr) + 1)),
	F("decoW", 0xBC, false, s.decoW),
	F("decoOfs", 0xBD, false, s.decoOfs),
	F("decoRows", 0xBE, false, s.decoRows),
	F("firstSlot", 0x4D, false, s.firstSlot),
	F("newSlot", 0x4E, false, s.newSlot),
	F("jetSlot", 0x51, true, s.jetSlot),
	F("objLines", 0x32, false, s.objLines),
	F("toggle33", 0x33, true, s.toggle33),
	F("bridgeFlags", 0x0B, true, s.bridgeFlags),
	F("objFlag", 0xB8, false, s.objFlag),
	F("objWidth", 0xB3, false, s.objWidth),
	F("roadKind", 0x38, true, s.roadKind),
	F("roadX", 0x39, true, s.roadX),
	F("roadY", 0x3A, true, s.roadY),
	F("v3E", 0x3E, true, s.v3E), F("v3F", 0x3F, true, s.v3F), F("v40", 0x40, true, s.v40),
	F("v41", 0x41, true, s.v41), F("v50", 0x50, true, s.v50), F("vC1", 0xC1, true, s.vC1),
	F("vC2", 0xC2, true, s.vC2),
	F("flash", 0x4F, true, s.flash),
	F("vCB", 0xCB, false, s.vCB),
	F("vB1", 0xB1, true, s.vB1),
	F("vB7", 0xB7, true, s.vB7),
	F("vC8", 0xC8, false, s.vC8),
	F("vC9", 0xC9, false, s.vC9),
	F("vCA", 0xCA, false, s.vCA),
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
		fn("slot.height", i, uint16_t(0x0521 + i), false, &Slot::height);
	}
}

}  // namespace

void RiverGen::loadFromRam(const uint8_t *ram) {
	for (const Field &f : kFields) *f.ref(s) = ram[f.addr];
	forEachSlotField([&](const char *, int i, uint16_t addr, bool, uint8_t Slot::*m) { s.slot[i].*m = ram[addr]; });
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
