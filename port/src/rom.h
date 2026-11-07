// The cartridge image, and the game data the port reads from it.
//
// The port contains no graphics or tables of the original: they are read from the user's
// ROM file ($A000-$BFFF) through the named accessors below. Each says what the table holds;
// the address is the table's place in the cartridge.
#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace rr {

// Object types and kinds (index of the ROM tables below). A slot's type is what was placed;
// its kind is what it is now (animation frames, explosions).
namespace object {
constexpr uint8_t kEmpty = 0;           // also 1: nothing, or an object that has gone
constexpr uint8_t kShot = 2;            // explosion frames 2-6 (2: shot, 3: rammed), then 0
constexpr uint8_t kRammed = 3;
constexpr uint8_t kJet = 7;             // first live kind: everything from here on can be hit
constexpr uint8_t kHelicopter = 8;      // 8, 9: rotor frames
constexpr uint8_t kShooter = 0x0A;      // $0A, $0B: helicopter that fires missiles
constexpr uint8_t kBalloon = 0x0C;
constexpr uint8_t kShip = 0x0D;
constexpr uint8_t kBridge = 0x0E;
constexpr uint8_t kFuel = 0x0F;         // fuel depot
constexpr uint8_t kBridgeWithTank = 0x10;   // only for the points table
}  // namespace object

class Rom {
public:
	bool load(const std::string &path);
	uint8_t at(uint16_t addr) const { return data_[addr - 0xA000]; }
	uint16_t word(uint16_t addr) const { return uint16_t(at(addr) | (at(uint16_t(addr + 1)) << 8)); }

	// --- objects -------------------------------------------------------------------------
	// Points for destroying an object of this type, in tens (BCD).
	uint8_t points(uint8_t type) const { return at(uint16_t(0xBB0B + type)); }
	// The object type the generator picks for an enemy (index: 3 random bits).
	uint8_t enemyChoice(int i) const { return at(uint16_t(0xBB1C + (i & 7))); }
	// The next animation frame of a kind (explosions run 2..6 to 0, rotors 8<->9...).
	uint8_t nextKind(uint8_t kind) const { return at(uint16_t(0xBB24 + kind)); }
	// The sprite's last row (rows 0..height, drawn from the bottom up).
	uint8_t objectHeight(uint8_t type) const { return at(uint16_t(0xBB60 + type)); }
	// Sprite width (SIZEP3): 0 normal, 1 double (ship), 3 quadruple (bridge).
	uint8_t objectSize(uint8_t type) const { return at(uint16_t(0xBBA2 + type)); }
	// ROM address of the shape of a kind, one byte per row from the bottom; frame +16 for
	// objects facing left.
	uint16_t shapeAddress(int shapeIndex) const { return uint16_t(0xB900 + at(uint16_t(0xBB40 + shapeIndex))); }
	// ROM address of the colours of a type, one per row.
	uint16_t coloursAddress(uint8_t type) const { return uint16_t(0xB700 + at(uint16_t(0xBB30 + type))); }
	// Bank colour per row next to a bridge: the grey road with its yellow line.
	static constexpr uint16_t kRoadColours = 0xB4E5;
	uint8_t roadColour(int row) const { return at(uint16_t(kRoadColours + row)); }

	// --- river generator ---------------------------------------------------------------------
	// Starting points of the games (2 games each: bridge 1, 5, 20, 50): LFSR A, B, C, bank.
	uint8_t startCheckpoint(int start, int i) const { return at(uint16_t(0xB494 + 4 * i + start)); }
	uint8_t startLevel(int start) const { return at(uint16_t(0xB4A4 + start)); }
	uint8_t startBridges(int start) const { return at(uint16_t(0xB4A8 + start)); }   // BCD
	// Bank step per line of the straight run up to a bridge.
	uint8_t bridgeBankStep(int line) const { return at(uint16_t(0xBA8F + line)); }
	// The 2-byte pieces where water meets the bank (and the scrolling text, same page).
	uint8_t edgePiece(int i) const { return at(uint16_t(0xB800 + (i & 0xFF))); }
	// Bank decorations (houses, mountains): offset from the edge for odd levels by 3 random
	// bits (bit 7: none) and by the river width; width in bytes; rows; bitmap address.
	uint8_t decoOffsetOddLevel(int i) const { return at(uint16_t(0xBBCC + i)); }
	uint8_t decoOffsetForWidth(int width) const { return at(uint16_t(0xBBD4 + width)); }
	uint8_t decoWidth(int i) const { return at(uint16_t(0xBBB2 + i)); }
	uint8_t decoRows(int width) const { return at(uint16_t(0xBBEB + width)); }
	uint16_t decoBitmap(int width) const { return word(uint16_t(0xBBF0 + ((width << 1) & 0xFF))); }
	// The tank shell's sideways speed: whole part and fraction (pairs).
	uint8_t shellSpeed(int i) const { return at(uint16_t(0xB7F4 + i)); }
	uint8_t shellSpeedFrac(int i) const { return at(uint16_t(0xB7F5 + i)); }
	// Where a tank on the bank road starts, by river width (even levels).
	uint8_t tankStartX(int width) const { return at(uint16_t(0xBFE4 + width)); }

	// The values sub $B0F1 loads at a reset into $4D..$76 (i = address - $4D).
	uint8_t resetValue(int i) const { return at(uint16_t(0xBAE1 + i)); }

	// --- game ----------------------------------------------------------------------------------
	// Next frame of the road tank's animation.
	uint8_t nextTankKind(uint8_t kind) const { return at(uint16_t(0xBAAF + kind)); }
	// Colour of the tank shell, changing every 8 frames.
	uint8_t shellColour(int i) const { return at(uint16_t(0xBAC5 + (i & 7))); }
	// The shell's rise per frame of its flight.
	uint8_t shellRise(int t) const { return at(uint16_t(0xB4AF + t)); }
	// Player-missile shapes: addresses of the shapes of the tank, the shell and the plane.
	uint16_t tankShape(int kind) const { return word(uint16_t(0xB4D1 + ((2 * kind) & 0xFF))); }
	uint16_t shellShape(int frame) const { return word(uint16_t(0xBAB7 + ((2 * frame) & 0xFF))); }
	uint16_t planeShape(int frame) const { return word(uint16_t(0xBACD + ((2 * frame) & 0xFF))); }

	// --- sound ---------------------------------------------------------------------------------
	uint8_t engineTone(int stick) const { return at(uint16_t(0xB4AC + (stick & 3))); }   // AUDC by up/down
	uint8_t shellPitch(int t) const { return at(uint16_t(0xB4C0 + t)); }               // shell whistle
	uint8_t scorePitch(int i) const { return at(uint16_t(0xB4E1 + i)); }               // bonus life sound

	// --- status panel ------------------------------------------------------------------------
	// The 60 characters of the panel at power-on, and the word for the second player's line.
	uint8_t panelText(int i) const { return at(uint16_t(0xBB70 + i)); }
	uint8_t playerTwoText(int i) const { return at(uint16_t(0xBB8F + i)); }
	// Character graphics copied into the panel's character set (8 bytes per character): the
	// Activision lettering, the panel symbols and "0", "P1"/"P2", the digits of "1982, 1983".
	uint8_t logoLettering(int i) const { return at(uint16_t(0xBEEE + i)); }
	uint8_t panelSymbols(int i) const { return at(uint16_t(0xBF2E + i)); }
	uint8_t playerLabels(int i) const { return at(uint16_t(0xBF9E + i)); }
	uint8_t yearDigits(int i) const { return at(uint16_t(0xBFB6 + i)); }
	// Colours of the rainbow under the Activision logo.
	uint8_t logoColour(int i) const { return at(uint16_t(0xB48D + i)); }
	// The scrolling line's text (character codes) from $B800 + i.
	uint8_t scrollerText(unsigned i) const { return 0xB800u + i < 0xC000u ? at(uint16_t(0xB800 + i)) : 0; }

private:
	std::array<uint8_t, 0x2000> data_{};
};

}  // namespace rr
