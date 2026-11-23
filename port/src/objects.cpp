// The objects in the river's slots. See objects.h.
#include "objects.h"

#include "rom.h"

namespace rr {
namespace {

using namespace object;

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

void moveObjects(RiverState &s, uint8_t frameCount, bool planeMoving, const Rom &rom) {
	MoveContext c;
	c.frameCount = frameCount;
	c.slotParity = s.slotParity;
	c.moving = planeMoving && (s.level >> 1) != 0;   // nothing moves before bridge 2
	for (int n = s.firstSlot; n < kSlots; n++) {
		Slot &obj = s.slot[size_t(n)];
		objectKind(obj.kind).animate(obj, frameCount, rom);
		c.slot = n;
		objectKind(obj.kind).move(obj, c);
	}
}

}  // namespace rr
