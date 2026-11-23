// The objects in the river's slots (RiverState::slot): what each kind of object does each frame.
//
// A slot keeps the original's bytes (Slot in river_gen.h): its place, flags, the type it was
// placed as and the kind it is now. The kind is also the animation frame (a helicopter's
// rotor, an explosion's frames) and changes when the object is hit, so the behaviour is looked
// up from it each frame: objectKind(slot.kind) gives the one object of the class that knows
// what that kind does (Explosion, Jet, Helicopter, Balloon, Ship, FuelDepot, Bridge).
#pragma once
#include <cstdint>

#include "river_gen.h"

namespace rr {

class Rom;

// What an object's move needs from the rest of the game this frame.
struct MoveContext {
	uint8_t frameCount = 0;
	uint8_t slotParity = 0;   // flips when the slots shift: odd and even slots move in turn
	int slot = 0;
	bool moving = false;      // the plane flies (or the attract mode), from bridge 2 on
};

class ObjectKind {
public:
	virtual ~ObjectKind() = default;
	virtual const char *name() const = 0;
	// A26C-A2C9: the animation, every 8 frames (every 2 for kinds 7 and up); some kinds have none
	virtual void animate(Slot &, uint8_t /*frameCount*/, const Rom &) const {}
	// A2CA-A2ED: the move, a step left or right
	virtual void move(Slot &, const MoveContext &) const {}
	// It turns round at the river edge: the display tests its rows against the edge
	virtual bool turnsAtEdge() const { return false; }
};

// The class of the object of this kind
const ObjectKind &objectKind(uint8_t kind);

// A26C-A2ED: every slot's object animates, then moves.
void moveObjects(RiverState &s, uint8_t frameCount, bool planeMoving, const Rom &rom);

}  // namespace rr
