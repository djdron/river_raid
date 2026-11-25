// The river and the game's objects on it, run together a line at a time: the river makes its
// line (keeping its bank clear of the object being drawn in at the top), then the objects
// move down with it and a new one is placed every 16 lines. The river's generator knows
// nothing of the objects (RiverGen, river_gen.h); the objects read the river where they are
// placed (Objects, objects.h).
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "objects.h"
#include "ram_map.h"
#include "river_gen.h"

namespace rr {

class World {
public:
	explicit World(const Rom &rom) : gen(rom), rom_(&rom) {}

	RiverGen gen;
	Objects objects;
	const Rom &rom() const { return *rom_; }

	// Both together, for keeping (saved states, the river before each line) and going back
	struct State {
		RiverState river;
		Objects objects;
		uint8_t unused_ = 0;   // (so that there is no padding: the state is saved as bytes)
	};
	State state() const { return {gen.s, objects, 0}; }
	void setState(const State &s) {
		gen.s = s.river;
		objects = s.objects;
	}

	// Game start ($B0F1, $AF9B, $A3C8; gameNumber 1..8): the river from the game's starting
	// point, the bridge to restart from in the lowest slot
	void startGame(int gameNumber, bool pal = false);
	// Respawn at the last checkpoint ($A3C8)
	void restoreCheckpoint();
	// One line: the river's, then the objects'
	void nextLine(uint8_t line[kLineBytes], uint8_t *bank = nullptr);

	// Another river and/or another starting bridge, worked out by running it. seed: game 1's
	// river starts from this state of the LFSR (1..$FFFFFF: A low byte, B, C high byte)
	// instead of the cartridge's; the other games at its bridges 5, 20 and 50. bridge: every
	// game starts at this bridge (1..99999) instead. 0 for both: the cartridge's table.
	void setRiver(uint32_t seed, int bridge);

	// The river's bytes and the objects' in the original's RAM
	void mapRam(RamMap &m) {
		gen.s.mapRam(m);
		objects.mapRam(m);
	}
	// Load / compare with a 16 KB RAM image of the original ($0000-$3FFF)
	void loadFromRam(const uint8_t *ram);
	void saveToRam(uint8_t *ram) const;
	// Load only the fields the rest of the game also changes (enemy movement, animation).
	void loadGameOwned(const uint8_t *ram);
	// Returns the list of fields that differ ("name $addr: port=.. orig=..").
	std::vector<std::string> diffWithRam(const uint8_t *ram, bool includeGameOwned) const;

private:
	const Rom *rom_;
};

}  // namespace rr
