// River Raid (Atari 8-bit) river generator, C++ port.
//
// The original generates the level one screen line at a time (code $A711-$AE96 in the
// cartridge). The generator depends only on its own state: not on the player, not on
// POKEY RANDOM (checked against the original on 120 000 lines). Each call of nextLine()
// produces one new line at the top of the screen:
//   - 48 bytes of 2-bit-per-pixel bitmap (ANTIC mode E, 192 pixels),
//   - updated river geometry (bank, water, island),
//   - where its objects go (Objects, objects.h, places them as the lines come: World, world.h).
//
// Field comments give the original RAM address, so the state can be compared byte for
// byte with traces of the original (see tools/terrain_check.cpp).
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ram_map.h"
#include "rom.h"

namespace rr {

constexpr int kLineBytes = 48;     // one screen line: 192 pixels, 4 per byte

// RiverState::blockFlags (bits also copied into the flags of a new slot)
constexpr uint8_t kBridgeBlock = 0x80;   // the straight run-up to a bridge (the bridge's slot)
constexpr uint8_t kNewBlock = 0x10;      // set at each block start
constexpr uint8_t kBankShade = 0x04;     // which of the two bank greens ($D6/$D2) this level has


// RiverState::islandFlags: 0 no island; $40 the island opens in the next section (this one has
// width 0); $C0 island; $80 the island closes (width 0 again).
constexpr uint8_t kIsland = 0x80;
constexpr uint8_t kIslandOpening = 0x40;
// RiverState::decoFlags
constexpr uint8_t kDecoLeft = 0x80;


// The object still being drawn in at the top, which the bank keeps clear of (Objects::obstacle)
struct BankObstacle {
	bool present = false;          // an enemy, bridge or depot (type 8 and up) is entering
	uint8_t x = 0, size = 0;
	uint8_t islandSide = 0;        // bit 7: it is right of the island
};

struct RiverState {
	// level structure: a block is 32 lines, a section 2 blocks, 16 sections lead to a bridge
	uint8_t lineInBlock = 0;        // $2F  0..31
	uint8_t blocksLeft = 0;         // $5A  blocks left in the section
	uint8_t sectionsLeft = 0;       // $5D  sections left until the bridge
	uint8_t level = 0;              // $60  bridge number, 1..$50 (stops growing at $50)
	uint8_t blockFlags = 0;         // $23  kBridgeBlock, kBankShade, $10; copied into new slots
	uint8_t islandFlags = 0;        // $2D  kIsland: this section has an island; $40: next one may
	// $052D non-zero: the bank is pulled into $18..$28 (bit 7: object positions too)
	uint8_t corridor = 0;
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
	uint8_t unused_ = 0;            // (so that there is no padding: the state is saved as bytes)
	uint16_t decoBitmap = 0;        // $BA/$BB ROM address of the next bitmap row
	uint8_t decoWidth = 0;          // $BC  bytes per row
	uint8_t decoOffset = 0;         // $BD  bytes from the line edge
	uint8_t decoRows = 0;           // $BE  rows
	uint8_t sectionBankColour = 0;  // $0B  bank colour of the section ($D2/$D6)
	uint8_t flashLines = 0;         // $4F  a hit bridge: the bank flashes on this many new lines
	uint8_t depotLevelLimit = 0x2E; // $CB  levels past it don't make fuel depots rarer ($2E NTSC, $2F PAL)

	void mapRam(RamMap &m) {
		const uint8_t owned = RamMap::kGameOwned;
		m.byte("lineInBlock", 0x2F, lineInBlock).byte("blocksLeft", 0x5A, blocksLeft).byte("sectionsLeft", 0x5D, sectionsLeft);
		m.byte("level", 0x60, level).byte("blockFlags", 0x23, blockFlags).byte("islandFlags", 0x2D, islandFlags);
		m.byte("corridor", 0x052D, corridor).byte("bridgeHit", 0x2E, bridgeHit, owned);
		m.byte("rndA", 0xAA, rndA).byte("rndB", 0xAB, rndB).byte("rndC", 0xAC, rndC);
		m.bytes("checkpoint", 0x66, checkpoint, 4).bytes("prevCheckpoint", 0xAD, prevCheckpoint, 4);
		m.byte("bank", 0x52, bank).byte("bankDir", 0xC3, bankDir).byte("bankMin", 0xC4, bankMin).byte("bankMax", 0xC5, bankMax);
		m.byte("waterWidth", 0x53, waterWidth).byte("islandWidth", 0x54, islandWidth);
		m.byte("islandTargetWidth", 0x34, islandTargetWidth).byte("islandStep", 0x35, islandStep);
		m.byte("sectionWidth", 0x5F, sectionWidth).byte("prevSectionWidth", 0x2C, prevSectionWidth).byte("bankMargin", 0xB5, bankMargin);
		m.byte("decoFlags", 0x36, decoFlags).word("decoBitmap", 0xBA, 0xBB, decoBitmap).byte("decoWidth", 0xBC, decoWidth);
		m.byte("decoOffset", 0xBD, decoOffset).byte("decoRows", 0xBE, decoRows);
		m.byte("sectionBankColour", 0x0B, sectionBankColour, owned).byte("flashLines", 0x4F, flashLines, owned);
		m.byte("depotLevelLimit", 0xCB, depotLevelLimit);
	}
};

class RiverGen {
public:
	explicit RiverGen(const Rom &rom) : rom_(rom) { startsFromRom(); }
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
	// `bank`, if given, gets the line without its decoration (houses, roads): the bank fill
	// where the decoration is drawn (for the collisions with the bank, display.cpp)
	// `obstacle`: the object entering at the top, which the bank keeps clear of
	void nextLine(uint8_t line[kLineBytes], uint8_t *bank, const BankObstacle &obstacle);

	// Where the games start. Games 1-8 have 4 starting points, 2 games each (one and two
	// players): bridges 1, 5, 20 and 50 of the same river. The cartridge stores, for each, the
	// generator's checkpoint there, the level and the bridge counter, so that the game does not
	// have to run the river up to them.
	struct Start {
		uint8_t checkpoint[4];   // LFSR A, B, C and bank
		uint8_t level;
		int bridge;              // the bridge, 1..99999 (the counter shows its last 4 digits)
	};
	// The cartridge's starts (World::setRiver works out others)
	void startsFromRom();
	const Start &start(int n) const { return starts_[n]; }
	void setStart(int n, const Start &start) { starts_[n] = start; }

private:
	const Rom &rom_;
	Start starts_[4];
	const BankObstacle *obstacle_ = nullptr;   // this line's (nextLine)
	void stepLfsr();                          // sub $B183
	void countLines();                        // $A711-$A7FA
	uint8_t chooseSectionWidth();             // $A781-$A7DB (even levels)
	void changeIsland();                      // $A7FB-$A825
	void moveBank();                          // $A826-$A96B
	bool bankWouldHitObject(bool movingLeft) const;   // $A8D2-$A95E
	void startDecoration();                   // $A96C-$A9D6
	void drawLine(uint8_t line[kLineBytes], uint8_t *bank);  // $AA8C-$AB75
};

}  // namespace rr
