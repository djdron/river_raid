// The objects in the river's slots. See objects.h.
#include "objects.h"

#include "rom.h"

namespace rr {
namespace {

using namespace object;

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

// One step left or right, wrapping round the screen (A2CA)
void step(Slot &obj) {
	if (obj.flags & slotflag::kFacingLeft) {
		obj.x = uint8_t(obj.x - 1);
		if (obj.x < 0x2C) obj.x = 0xD4;
	} else {
		obj.x = uint8_t(obj.x + 1);
		if (obj.x >= 0xD5) obj.x = 0x2C;
	}
}

// The animated kinds: the next frame from the ROM's table every `every + 1` frames
class Animated : public ObjectKind {
public:
	explicit Animated(uint8_t every) : every_(every) {}
	void animate(Slot &obj, uint8_t frameCount, const Rom &rom) const override {
		if (!(every_ & frameCount)) obj.kind = rom.nextKind(obj.kind);
	}
private:
	uint8_t every_;
};

// Nothing, or the frames of an explosion (2 shot, 3 rammed, down to 6, then gone): every 8
// frames the next; it does not move.
class Explosion : public Animated {
public:
	Explosion() : Animated(0x07) {}
	const char *name() const override { return "explosion"; }
};

// The jet: flies straight across the river and over the land, every frame, round the screen
class Jet : public Animated {
public:
	Jet() : Animated(0x01) {}
	const char *name() const override { return "jet"; }
	void move(Slot &obj, const MoveContext &c) const override {
		if (c.moving) step(obj);
	}
};

// What stays in the water and goes to and fro: once woken (a random slot every 16 frames), on
// alternate frames for odd and even slots, turning round once at the first touch of the edge
class Wanderer {
protected:
	static void wander(Slot &obj, const MoveContext &c) {
		if (!c.moving) return;
		if ((c.slot ^ c.frameCount ^ c.slotParity) & 1) return;   // A1F9
		const uint8_t flags = obj.flags;
		if (!(flags & slotflag::kAwake)) return;
		if (flags & slotflag::kTouchingEdge) {
			if (!(flags & slotflag::kTurned)) obj.flags = flags ^ (slotflag::kTurned | slotflag::kFacingLeft);
		} else {
			obj.flags = flags & ~slotflag::kTurned;
		}
		step(obj);
	}
};

// The helicopter (kinds 8-9, and $0A-$0B for the one that fires missiles): its rotor turns
// every 2 frames
class Helicopter : public Animated, Wanderer {
public:
	Helicopter() : Animated(0x01) {}
	const char *name() const override { return "helicopter"; }
	void move(Slot &obj, const MoveContext &c) const override { wander(obj, c); }
	bool turnsAtEdge() const override { return true; }
};

// The balloon: no animation, half the speed
class Balloon : public ObjectKind, Wanderer {
public:
	const char *name() const override { return "balloon"; }
	void move(Slot &obj, const MoveContext &c) const override {
		if (c.moving && (c.frameCount & 2)) return;
		wander(obj, c);
	}
	bool turnsAtEdge() const override { return true; }
};

class Ship : public ObjectKind, Wanderer {
public:
	const char *name() const override { return "ship"; }
	void move(Slot &obj, const MoveContext &c) const override { wander(obj, c); }
	bool turnsAtEdge() const override { return true; }
};

// Bridges and fuel depots stay put
class Bridge : public ObjectKind {
public:
	const char *name() const override { return "bridge"; }
};
class FuelDepot : public ObjectKind {
public:
	const char *name() const override { return "fuel depot"; }
};

}  // namespace

// $AB76-$ABF2: everything moves down one line; when the lowest slot leaves the screen the
// slots shift down by one.
void Objects::scroll() {
	tank.y++;
	shell.y++;
	missile.y++;
	for (Slot &s : slot) s.y++;
	if (slot[kSlots - 1].y == 0 && firstSlot < kNoSlot) {
		for (int n = kSlots - 2; n >= 0 && n >= firstSlot; n--) slot[n + 1] = slot[n];
		slotParity ^= 1;
		firstSlot++;
		if (shooterSlot < kNoSlot) shooterSlot++;
		if (enteringSlot < kNoSlot) enteringSlot++;
	}
}

// $AC62-$AC8B: an enemy type, from the LFSR.
uint8_t Objects::chooseEnemy(const RiverState &r, const Rom &rom) {
	uint8_t type = rom.enemyChoice(r.rndB & 7);
	if (type == object::kJet) {
		if (r.level < 5) type = object::kShip;          // jets from level 5 on
	} else if (type == object::kBalloon) {
		if (r.lineInBlock != 0 || r.level < 3) type = object::kHelicopter;   // balloons at block starts
	}
	if (type == object::kShip) wideObject = 5;
	return type;
}

// $AC8C-$AD28: where across the river the new object goes (x: relative to the bank, less $20).
// May change the type (nothing when there is no room, a helicopter instead of a ship where
// the river is narrow).
void Objects::positionObject(RiverState &r, uint8_t &type, uint8_t &x, bool &facingLeft) {
	using namespace object;
	const bool island = r.islandFlags & kIsland;
	if (r.sectionWidth == r.prevSectionWidth) {
		placeWidth = r.sectionWidth;
		if (r.level & 1) {
			// odd levels: anywhere across, at random
			const uint8_t limit = lo((wideObject ? 0x37 : 0x3F) - r.bankMargin);
			x = std::min(lo((r.rndA & 0x3F) + r.bankMargin), limit);
			x = lo(x + r.islandWidth);
			facingLeft = r.rndA & 0x80;
			return;
		}
		x = r.level >> 1;
	} else {
		placeWidth = std::max(r.sectionWidth, r.prevSectionWidth);
		x = placeWidth;
	}
	const uint8_t tooNarrow = island ? 0x0D : 0x0E;
	if (tooNarrow < placeWidth && type != kJet) {
		type = 1;                                       // no room: nothing (a jet flies anyway)
		return;
	}
	// $ACD5: a narrow river (or the bridge section) has helicopters, not ships
	bool narrow;
	if (r.sectionsLeft == 1) {
		r.bankMargin = lo(r.bankMargin + 2);
		narrow = true;
	} else {
		narrow = (island ? 0x0A : 0x0D) < placeWidth;
	}
	if (narrow && type == kShip) {
		wideObject = 0;
		type = kHelicopter;
	}
	// $ACF6: near the left bank facing right, or near the right bank facing left; with an
	// island, on either side of it
	int carry = (placeWidth >> 6) & 1;                // carried out of placeWidth * 4
	x = lo(placeWidth << 2);
	enum class Side { Random, Left, Right, RightOfIsland } side = Side::Random;
	if (x != 0 && island) {
		x = addWithCarry(lo(~x), 0x41, carry);
		islandObjectSide = r.rndA;
		if (!(r.rndA & 0x80)) {
			side = Side::RightOfIsland;
		} else {
			x = addWithCarry(lo(~x), 0x80, carry);
			if (x != 0) side = Side::Left;
		}
	}
	if (side == Side::Random) side = (r.rndA & 0x80) ? Side::Right : Side::Left;
	if (side == Side::Left) {                           // $AD14
		carry = 1;
		x = addWithCarry(x, r.bankMargin, carry);
		if (x != 0) return;                             // facing right
		side = Side::Right;
	}
	if (side == Side::Right) x = addWithCarry(lo(~x), 0x81, carry);   // $AD19
	// $AD1D: measured from the right, facing left
	carry = type >= kFuel;
	x = subtractWithBorrow(x, 8, carry);
	x = subtractWithBorrow(x, r.bankMargin, carry);
	if (wideObject) x = subtractWithBorrow(x, 8, carry);
	facingLeft = true;
}

// $ABF3-$AD8D: every 16 lines a new object goes into the next free slot at the top.
void Objects::place(RiverState &r, const Rom &rom) {
	using namespace object;
	const uint8_t n = --firstSlot;
	enteringSlot = n;
	wideObject = 0;
	slot[n].flags = r.blockFlags;
	uint8_t type = 1;                                   // nothing
	// x: the object's position relative to the bank, less $20. When nothing is placed, the value
	// left over from the decision is stored all the same.
	uint8_t x = 0;
	bool place = false, enemy = false;
	bool facingLeft = false;                            // $AD29
	if (r.blockFlags & kBridgeBlock) {                  // the bridge itself
		r.sectionBankColour = lo((r.blockFlags & kBankShade) | 0xD2);
		wideObject = 7;
		type = kBridge;
		x = lo(r.islandWidth - 6);
	} else if (r.lineInBlock != 0) {                    // mid-block: maybe an enemy, more likely higher up
		x = lo(r.level - 8);
		if (!(x & 0x80)) {
			if (x >= 0x20) enemy = true;
			else { x = lo(x << 3); if (x >= r.rndB) enemy = true; }
		}
	} else {                                            // $AC3E: block start: an enemy or a fuel depot
		type = kEmpty;
		x = lo(r.sectionsLeft + r.blocksLeft);
		if (x < 0x12) {
			// depots get rarer with the level, up to the limit
			const uint8_t level = std::min(r.level, r.depotLevelLimit);
			x = lo((0x3F - level) << 1);
			if (x < r.rndB) enemy = true;
			else if (!(r.rndA & 0x40)) { type = kFuel; place = true; }
		}
	}
	if (enemy) {
		type = chooseEnemy(r, rom);
		place = true;
	}
	if (place) positionObject(r, type, x, facingLeft);
	if (facingLeft && type != kFuel) slot[n].flags |= slotflag::kFacingLeft;

	// $AD37: store the object
	Slot &obj = slot[n];
	uint8_t bank = r.bank;
	if (r.corridor & 0x80) bank = std::min<uint8_t>(std::max<uint8_t>(bank, 0x18), 0x28);
	obj.x = lo(bank + lo(x + 0x20));
	obj.kind = obj.type = type;
	if (type == kHelicopter && shooterSlot >= kNoSlot && r.sectionsLeft < 0x0E && !(r.rndC & 0x70) &&
		r.level >= 0x0D) {
		obj.kind = obj.type = kShooter;                 // one helicopter at a time fires missiles
		missile.x = 0;
		shooterSlot = n;
	}
	enteringLines = lo(rom.objectHeight(type) + 4);
	obj.size = rom.objectSize(type);
	obj.y = lo(0x61 - enteringLines);
	if ((r.lineInBlock | tank.kind) == 0) placeTank(r, rom);
}

// $AD8E-$AE93: at a block start, maybe a tank on a road: on the bridge's road, or on a road
// across the bank (from level 7, with an enemy placed now).
void Objects::placeTank(const RiverState &r, const Rom &rom) {
	if (r.blockFlags & kBridgeBlock) {                  // $ADA6: on the bridge, driving across
		const uint8_t rnd = r.rndA & 0x7F;
		tank.y = 0x0B;
		tank.fires = 0;
		shell.speedFrac = 0;
		shell.speed = 2;
		tank.kind = rnd >= 0x40 ? 6 : 5;
		tank.x = lo(rnd + 0x40);
		return;
	}
	// $ADCD
	const uint8_t width = std::min(r.sectionWidth, r.prevSectionWidth);
	if (slot[enteringSlot].kind < object::kHelicopter || r.level < 7 || r.sectionsLeft < 5 ||
		r.sectionsLeft >= 0x0E || (r.rndC & 0xC0) || width < 2 || width >= 0x0E)
		return;
	// the shell's speed
	uint8_t speed = r.rndC >> 3;
	if (r.level == 0x0A) speed ^= 7;
	speed &= 6;
	if (speed == 0 && !(r.islandFlags & kIsland) && width < 8) speed = 8;
	shell.speed = rom.shellSpeed(speed);
	shell.speedFrac = rom.shellSpeedFrac(speed);
	// where it starts and which way it faces: kind 5 right, 6 left
	uint8_t kind = 5;
	uint8_t x;
	const bool decoLeft = r.decoFlags & kDecoLeft;
	const bool decoRight = r.decoFlags != 0 && !decoLeft;
	if (r.islandFlags & kIsland) {                      // on the island
		if (r.decoFlags != 0 || width >= 0x0B || width < 4) return;
		if (r.rndC & 1) kind++;
		x = 0x7C;
	} else {
		// on the left or the right bank; the road is mirrored when a decoration is in the way
		const bool odd = r.level & 1;
		uint8_t start = odd ? (r.rndC & 0x1F) : rom.tankStartX(width);
		const bool rightBank = start >= (odd ? 0x10 : 0x50);
		if (odd && rightBank) start = lo(start + 0x58);
		bool mirror;
		if (rightBank) {
			mirror = decoRight;
			if (!decoRight) kind++;
		} else {
			mirror = decoLeft;
			if (decoLeft) kind++;
		}
		if (odd) {
			if (mirror) start = lo(0x78 - start);
			x = lo(start + r.bank + 0x20);
		} else {
			if (mirror) start = lo(0x98 - start);
			x = lo(start + 0x30);
		}
	}
	tank.x = x;
	tank.kind = kind;
	tank.y = 0x0C;
	shell.timer = 0xFF;
	tank.fires = 0xFF;
}

BankObstacle Objects::obstacle() const {
	BankObstacle o;
	if (enteringSlot < kNoSlot && slot[enteringSlot].type >= object::kHelicopter) {
		o.present = true;
		o.x = slot[enteringSlot].x;
		o.size = slot[enteringSlot].size;
	}
	o.islandSide = islandObjectSide;
	return o;
}

void Objects::afterLine(RiverState &river, const Rom &rom) {
	scroll();
	// $ABE1: the newest object still takes up the top lines (count 1..$80: above zero, signed)
	if (enteringSlot < kNoSlot) {
		if (enteringLines >= 1 && enteringLines <= 0x80) {
			enteringLines--;
			return;
		}
		enteringSlot = kNoSlot;
	}
	if ((river.lineInBlock & 0x0F) != 0 || firstSlot == 0) return;
	place(river, rom);
	enteringLines--;                                    // $AE94
}

void Objects::restoreCheckpoint(const RiverState &river, const Rom &rom) {
	// $B0F1: $4D-$5F from the reset table
	auto reset = [&](uint16_t addr) { return rom.resetValue(addr - 0x4D); };
	firstSlot = reset(0x4D);
	enteringSlot = reset(0x4E);
	shell.timer = reset(0x50);
	shooterSlot = reset(0x51);
	// $2B-$4C cleared
	enteringLines = slotParity = islandObjectSide = 0;
	tank.kind = tank.x = tank.y = shell.speed = shell.speedFrac = shell.y = tank.fires = 0;
	// $0500-$05FF cleared: object kinds, positions, types, sprite sizes
	for (Slot &s : slot) s.kind = s.x = s.type = s.size = s.flags = 0;
	// $B15E: the bridge we restart from, in the lowest slot
	Slot &bridge = slot[kSlots - 1];
	enteringLines = 0x17;
	bridge.type = object::kBridge;
	bridge.y = 0x4A;
	bridge.flags = river.blockFlags;
	firstSlot = enteringSlot = kSlots - 1;
}

const ObjectKind &objectKind(uint8_t kind) {
	static const Explosion explosion;
	static const Jet jet;
	static const Helicopter helicopter;
	static const Balloon balloon;
	static const Ship ship;
	static const Bridge bridge;
	static const FuelDepot depot;
	if (kind < kJet) return explosion;
	if (kind == kJet) return jet;
	if (kind < kBalloon) return helicopter;
	if (kind == kBalloon) return balloon;
	if (kind == kShip) return ship;
	if (kind == kBridge) return bridge;
	return depot;   // kFuel (no other kind is ever made)
}

void moveObjects(Objects &s, uint8_t level, uint8_t frameCount, bool planeMoving, const Rom &rom) {
	MoveContext c;
	c.frameCount = frameCount;
	c.slotParity = s.slotParity;
	c.moving = planeMoving && (level >> 1) != 0;   // nothing moves before bridge 2
	for (int n = s.firstSlot; n < kSlots; n++) {
		Slot &obj = s.slot[size_t(n)];
		objectKind(obj.kind).animate(obj, frameCount, rom);
		c.slot = n;
		objectKind(obj.kind).move(obj, c);
	}
}

}  // namespace rr
