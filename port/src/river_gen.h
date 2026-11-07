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
	// level structure
	uint8_t lineInBlock = 0;    // $2F  0..31
	uint8_t blocksLeft = 0;     // $5A  blocks left in the section (2 per section)
	uint8_t sectionsLeft = 0;   // $5D  sections left until the bridge (16)
	uint8_t level = 0;          // $60  bridge number, 1..$50
	uint8_t mode = 0;           // $23  bit 7: straight bridge approach, bit 4, bit 2
	uint8_t shape = 0;          // $2D  bit 7: island, bit 6
	uint8_t corridor = 0;       // $052D non-zero: banks forced into $18..$28
	uint8_t bridgeHit = 0;      // $2E
	// pseudo-random generator, 24-bit LFSR (sub $B183)
	uint8_t rndA = 0, rndB = 0, rndC = 0;      // $AA $AB $AC
	uint8_t save[4] = {};       // $66-$69 checkpoint: $AA $AB $AC $52 at the last bridge
	uint8_t savePrev[4] = {};   // $AD-$B0 previous checkpoint
	// geometry of the current line
	uint8_t bank = 0;           // $52  bank position
	uint8_t bankDir = 0;        // $C3  +1, $FF or 0
	uint8_t bankMin = 0;        // $C4
	uint8_t bankMax = 0;        // $C5
	uint8_t water = 0;          // $53  water width
	uint8_t islandW = 0;        // $54  island / inner width
	uint8_t islandTarget = 0;   // $34
	uint8_t islandStep = 0;     // $35  +1/+2/$FF/$FE or 0
	uint8_t widthCode = 0;      // $5F  target width code of the section (x4 = $34)
	uint8_t widthCodePrev = 0;  // $2C
	uint8_t margin = 0;         // $B5  0 or 6 (levels below 9 keep objects away from banks)
	// bank decoration (house, mountain) being drawn
	uint8_t deco = 0;           // $36  0 none, bit 7 left side, low bits
	uint8_t deco37 = 0;         // $37
	uint16_t decoPtr = 0;       // $BA/$BB bitmap rows
	uint8_t decoW = 0;          // $BC  bytes per row
	uint8_t decoOfs = 0;        // $BD  offset in bytes from the line edge
	uint8_t decoRows = 0;       // $BE  rows
	// objects
	std::array<Slot, kSlots> slot{};
	uint8_t firstSlot = 0;      // $4D  first slot in use (slots fill from 10 down)
	uint8_t newSlot = 0;        // $4E  slot of the object still being drawn in, $0B = none
	uint8_t jetSlot = 0;        // $51
	uint8_t objLines = 0;       // $32  lines left to draw the newest object
	uint8_t toggle33 = 0;       // $33
	uint8_t bridgeFlags = 0;    // $0B
	uint8_t objFlag = 0;        // $B8
	uint8_t objWidth = 0;       // $B3
	// object on the bank road (tank) and others the generator starts
	uint8_t roadKind = 0;       // $38
	uint8_t roadX = 0;          // $39
	uint8_t roadY = 0;          // $3A
	uint8_t v3E = 0, v3F = 0, v40 = 0, v41 = 0, v50 = 0, vC1 = 0, vC2 = 0;
	uint8_t flash = 0;          // $4F  bridge flash: colours switch while non-zero
	uint8_t vCB = 0x2E;         // $CB  set at power-on: $2E on NTSC, $2F on PAL (fuel depot odds)
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

	// Generate one line; writes the bitmap into line[0..47].
	void nextLine(uint8_t line[kLineBytes]);

	// Load / compare with a 16 KB RAM image of the original ($0000-$3FFF).
	void loadFromRam(const uint8_t *ram);
	// Load only the fields the rest of the game also changes (enemy movement, animation).
	void loadGameOwned(const uint8_t *ram);
	// Returns the list of fields that differ ("name $addr: port=.. orig=..").
	std::vector<std::string> diffWithRam(const uint8_t *ram, bool includeGameOwned) const;

private:
	const Rom &rom_;
	void stepLfsr();
	void sectionCheckpoint();   // sub $B194
	void structureAndWidth();   // $A711-$A7FA
	void smoothIsland();        // $A7FB-$A825
	void moveBank();            // $A826-$A96B
	void startDecoration();     // $A96C-$A9D6
	void drawLine(uint8_t line[kLineBytes]);   // $AA8C-$AB75
	void scrollObjects();       // $AB76-$ABF2
	void placeObject();         // $ABF3-$AE96
};

}  // namespace rr
