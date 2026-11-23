// The game's objects on the river: the ones in the slots (ships, helicopters, jets, balloons,
// fuel depots, bridges, explosions), the tank on the bank road, its shell and the helicopter's
// missile. Objects keeps them and places new ones as the river comes (World runs the two
// together, world.h); the rest of this file is what each kind of object does each frame.
//
// A slot keeps the original's bytes (Slot): its place, flags, the type it was
// placed as and the kind it is now. The kind is also the animation frame (a helicopter's
// rotor, an explosion's frames) and changes when the object is hit, so the behaviour is looked
// up from it each frame: objectKind(slot.kind) gives the one object of the class that knows
// what that kind does (Explosion, Jet, Helicopter, Balloon, Ship, FuelDepot, Bridge).
#pragma once
#include <array>
#include <cstdint>

#include "river_gen.h"
#include "tank.h"

namespace rr {

class Rom;

constexpr int kSlots = 11;         // object slots, one horizontal band each
constexpr uint8_t kNoSlot = 11;    // "no slot" in slot numbers

// Slot::flags
namespace slotflag {
constexpr uint8_t kBridge = 0x80;        // the slot holds a bridge (road colours next to it)
constexpr uint8_t kAwake = 0x40;         // the object may move
constexpr uint8_t kTurned = 0x10;        // turned round at the bank edge, until it leaves it
constexpr uint8_t kFacingLeft = 0x08;    // moves left (and its shape is mirrored)
constexpr uint8_t kTouchingEdge = 0x01;  // set by the display: the sprite touched the bank edge
}  // namespace slotflag

struct Slot {
	uint8_t y = 0;        // $0C+i  vertical position, +1 per generated line
	uint8_t x = 0;        // $0516+i horizontal position
	uint8_t flags = 0;    // $42+i
	uint8_t kind = 0;     // $0500+i current kind / animation (game logic changes it)
	uint8_t type = 0;     // $050B+i type as placed (index into the points table)
	uint8_t size = 0;     // $0521+i sprite width (SIZEP3): 0 normal, 1 double (ship), 3 quad (bridge)
};

struct Objects {
	std::array<Slot, kSlots> slot{};
	uint8_t firstSlot = 0;          // $4D  first slot in use (slots fill from 10 down)
	uint8_t enteringSlot = 0;       // $4E  slot of the object still being drawn in, kNoSlot = none
	uint8_t shooterSlot = 0;        // $51  slot of the helicopter that fires missiles, kNoSlot = none
	uint8_t enteringLines = 0;      // $32  lines left until the newest object is fully drawn in
	uint8_t slotParity = 0;         // $33  flips when the slots shift (objects move on alternate frames)

	uint8_t wideObject = 0;         // $B8  non-zero while placing a bridge (7) or a ship (5)
	uint8_t placeWidth = 0;         // $B3  river width code used to place the object
	// the tank on the bank road, its shell, and the helicopter's missile
	Tank tank;
	Shell shell;
	Missile missile;
	uint8_t islandObjectSide = 0;   // $37  bit 7: the newest object is right of the island

	void mapRam(RamMap &m) {
		const uint8_t owned = RamMap::kGameOwned, inSlot = RamMap::kSlot;
		for (int i = 0; i < kSlots; i++) {
			Slot &s = slot[size_t(i)];
			m.byte("slot.y", uint16_t(0x0C + i), s.y, inSlot, i).byte("slot.x", uint16_t(0x0516 + i), s.x, owned | inSlot, i);
			m.byte("slot.flags", uint16_t(0x42 + i), s.flags, owned | inSlot, i).byte("slot.kind", uint16_t(0x0500 + i), s.kind, owned | inSlot, i);
			m.byte("slot.type", uint16_t(0x050B + i), s.type, inSlot, i).byte("slot.size", uint16_t(0x0521 + i), s.size, inSlot, i);
		}
		m.byte("firstSlot", 0x4D, firstSlot).byte("enteringSlot", 0x4E, enteringSlot).byte("shooterSlot", 0x51, shooterSlot, owned);
		m.byte("enteringLines", 0x32, enteringLines).byte("slotParity", 0x33, slotParity, owned);
		m.byte("wideObject", 0xB8, wideObject).byte("placeWidth", 0xB3, placeWidth).byte("islandObjectSide", 0x37, islandObjectSide);
		tank.mapRam(m);
		shell.mapRam(m);
		missile.mapRam(m);
	}

	// The entering object, which the bank keeps clear of
	BankObstacle obstacle() const;
	// After the river's line ($AB76-$AE96): everything moves down a line ($AB76), and every 16
	// lines a new object goes into the next free slot at the top ($ABF3).
	void afterLine(RiverState &river, const Rom &rom);
	// Respawn ($B0F1, $B15E): the objects gone, the bridge to restart from in the lowest slot
	// (after the river's own restoreCheckpoint, whose block flags it takes)
	void restoreCheckpoint(const RiverState &river, const Rom &rom);

private:
	void scroll();                                                                // $AB76-$ABF2
	void place(RiverState &r, const Rom &rom);                                    // $ABF3-$AD8D
	uint8_t chooseEnemy(const RiverState &r, const Rom &rom);                     // $AC62-$AC8B
	void positionObject(RiverState &r, uint8_t &type, uint8_t &x, bool &facingLeft);   // $AC8C-$AD28
	void placeTank(const RiverState &r, const Rom &rom);                          // $AD8E-$AE93
};


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
void moveObjects(Objects &objects, uint8_t level, uint8_t frameCount, bool planeMoving, const Rom &rom);

}  // namespace rr
