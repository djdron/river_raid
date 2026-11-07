// River Raid (Atari 8-bit) river generator, C++ port.
//
// The original generates the level one screen line at a time (code $A711-$AE96 in the
// cartridge). The generator depends only on its own state: not on the player, not on
// POKEY RANDOM (checked against the original on 120 000 lines). Each call of nextLine()
// produces one new line at the top of the screen:
//   - 48 bytes of 2-bit-per-pixel bitmap (ANTIC mode E, 192 pixels),
//   - updated river geometry (bank, water, island),
//   - objects placed into the slot table every 16 lines.
//
// Field comments give the original RAM address, so the state can be compared byte for
// byte with traces of the original (see tools/terrain_check.cpp).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rr {

// Read-only access to the cartridge image ($A000-$BFFF); the port never ships game data.
class Rom {
public:
	bool load(const std::string &path);
	uint8_t at(uint16_t addr) const { return data_[addr - 0xA000]; }
	uint16_t word(uint16_t addr) const { return uint16_t(at(addr) | (at(addr + 1) << 8)); }
private:
	std::array<uint8_t, 0x2000> data_{};
};

constexpr int kSlots = 11;         // object slots, one horizontal band each
constexpr int kLineBytes = 48;     // one screen line: 192 pixels, 4 per byte

struct Slot {
	uint8_t y = 0;        // $0C+i  vertical position, +1 per generated line
	uint8_t x = 0;        // $0516+i horizontal position
	uint8_t flags = 0;    // $42+i
	uint8_t kind = 0;     // $0500+i current kind / animation (game logic changes it)
	uint8_t type = 0;     // $050B+i type as placed (index into the points table)
	uint8_t size = 0;     // $0521+i sprite width (SIZEP3): 0 normal, 1 double (ship), 3 quad (bridge)
};

struct RiverState {
	// level structure: a block is 32 lines, a section 2 blocks, 16 sections lead to a bridge
	uint8_t lineInBlock = 0;        // $2F  0..31
	uint8_t blocksLeft = 0;         // $5A  blocks left in the section
	uint8_t sectionsLeft = 0;       // $5D  sections left until the bridge
	uint8_t level = 0;              // $60  bridge number, 1..$50 (stops growing at $50)
	uint8_t blockFlags = 0;         // $23  kBridgeBlock, kBankShade, $10; copied into new slots
	uint8_t islandFlags = 0;        // $2D  kIsland: this section has an island; $40: next one may
	uint8_t corridor = 0;           // $052D non-zero: the bank is pulled into $18..$28 (bit 7: also
	                                //       for object positions)
	uint8_t bridgeHit = 0;          // $2E  the bridge ahead was hit (restart point after a crash)
	// pseudo-random generator: 24-bit LFSR (sub $B183); the river is the same in every game
	uint8_t rndA = 0, rndB = 0, rndC = 0;     // $AA $AB $AC
	uint8_t checkpoint[4] = {};     // $66-$69 restart point: LFSR A, B, C and bank at the last bridge
	uint8_t prevCheckpoint[4] = {}; // $AD-$B0 the one before
	// geometry of the current line (horizontal values in pixels of the 192-pixel line / 2)
	uint8_t bank = 0;               // $52  left bank: where the water starts
	uint8_t bankDir = 0;            // $C3  bank walk direction: +1, $FF (-1) or 0
	uint8_t bankMin = 0;            // $C4  walk limits
	uint8_t bankMax = 0;            // $C5
	uint8_t waterWidth = 0;         // $53
	uint8_t islandWidth = 0;        // $54  island (or, without an island, extra bank) width
	uint8_t islandTargetWidth = 0;  // $34
	uint8_t islandStep = 0;         // $35  +1/+2/$FF/$FE per line while changing, or 0
	uint8_t sectionWidth = 0;       // $5F  width code of the section (x4 = island target width)
	uint8_t prevSectionWidth = 0;   // $2C
	uint8_t bankMargin = 0;         // $B5  6 below level 9: objects keep away from the banks
	// bank decoration (house, mountain) being drawn
	uint8_t decoFlags = 0;          // $36  0: none; bit 7: on the left bank
	uint8_t islandObjectSide = 0;   // $37  bit 7: the newest object is right of the island
	uint16_t decoBitmap = 0;        // $BA/$BB ROM address of the next bitmap row
	uint8_t decoWidth = 0;          // $BC  bytes per row
	uint8_t decoOffset = 0;         // $BD  bytes from the line edge
	uint8_t decoRows = 0;           // $BE  rows
	// objects
	std::array<Slot, kSlots> slot{};
	uint8_t firstSlot = 0;          // $4D  first slot in use (slots fill from 10 down)
	uint8_t enteringSlot = 0;       // $4E  slot of the object still being drawn in, kNoSlot = none
	uint8_t shooterSlot = 0;        // $51  slot of the helicopter that fires missiles, kNoSlot = none
	uint8_t enteringLines = 0;      // $32  lines left until the newest object is fully drawn in
	uint8_t slotParity = 0;         // $33  flips when the slots shift (objects move on alternate frames)
	uint8_t sectionBankColour = 0;  // $0B  bank colour of the section ($D2/$D6)
	uint8_t wideObject = 0;         // $B8  non-zero while placing a bridge (7) or a ship (5)
	uint8_t placeWidth = 0;         // $B3  river width code used to place the object
	// the tank on the bank road, its shell, and the helicopter's missile
	uint8_t tankKind = 0;           // $38  0 none; 5/6 facing right/left (animated); bit 7: destroyed
	uint8_t tankX = 0;              // $39
	uint8_t tankY = 0;              // $3A
	uint8_t tankFires = 0;          // $41  bit 7: drives to the river and fires
	uint8_t shellSpeed = 0;         // $3E  shell's sideways speed per frame, whole part
	uint8_t shellSpeedFrac = 0;     // $3F  and fraction (/256)
	uint8_t shellY = 0;             // $40
	uint8_t shellTimer = 0;         // $50  bit 7: no shell; 0 launch; 1-$10 flight; $11-$38 explosion
	uint8_t missileX = 0;           // $C1  0: no missile
	uint8_t missileY = 0;           // $C2
	uint8_t flashLines = 0;         // $4F  a hit bridge: the bank flashes on this many new lines
	uint8_t depotLevelLimit = 0x2E; // $CB  levels past it don't make fuel depots rarer ($2E NTSC, $2F PAL)
	// scratch values the original keeps in RAM (kept for byte-exact comparison)
	uint8_t vB1 = 0, vB7 = 0, vC8 = 0, vC9 = 0, vCA = 0;
};

class RiverGen {
public:
	explicit RiverGen(const Rom &rom) : rom_(rom) {}
	RiverState s;

	// Game start as the original does it after START: reset (sub $B0F1), starting point of
	// the selected game ($AF9B: games 1-2 start at bridge 1, 3-4 at 5, 5-6 at 20, 7-8 at 50)
	// and the respawn restore ($A3C8). gameNumber is 1..8.
	// pal: the original reads GTIA's PAL bit at power-on ($A06A); the river differs slightly.
	void startGame(int gameNumber, bool pal = false);
	// Respawn at the last checkpoint ($A3C8): the river continues from the saved bridge.
	void restoreCheckpoint();

	// sub $B194: remember the current river position as the restart point, next level.
	void sectionCheckpoint();

	// Generate one line; writes the bitmap into line[0..47].
	void nextLine(uint8_t line[kLineBytes]);

	// Load / compare with a 16 KB RAM image of the original ($0000-$3FFF).
	void loadFromRam(const uint8_t *ram);
	void saveToRam(uint8_t *ram) const;
	// Load only the fields the rest of the game also changes (enemy movement, animation).
	void loadGameOwned(const uint8_t *ram);
	// Returns the list of fields that differ ("name $addr: port=.. orig=..").
	std::vector<std::string> diffWithRam(const uint8_t *ram, bool includeGameOwned) const;

private:
	const Rom &rom_;
	void stepLfsr();
	void structureAndWidth();   // $A711-$A7FA
	void smoothIsland();        // $A7FB-$A825
	void moveBank();            // $A826-$A96B
	void startDecoration();     // $A96C-$A9D6
	void drawLine(uint8_t line[kLineBytes]);   // $AA8C-$AB75
	void scrollObjects();       // $AB76-$ABF2
	void placeObject();         // $ABF3-$AE96
};

}  // namespace rr
