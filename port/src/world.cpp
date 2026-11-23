// The river and the objects run together. See world.h.
#include "world.h"

#include <cstring>

namespace rr {

void World::startGame(int gameNumber, bool pal) {
	objects = Objects{};
	gen.startGame(gameNumber, pal);
	objects.restoreCheckpoint(gen.s, *rom_);
}

void World::restoreCheckpoint() {
	gen.restoreCheckpoint();
	objects.restoreCheckpoint(gen.s, *rom_);
}

void World::nextLine(uint8_t line[kLineBytes], uint8_t *bank) {
	const BankObstacle obstacle = objects.obstacle();
	gen.nextLine(line, bank, obstacle);
	objects.afterLine(gen.s, *rom_);
}

void World::setRiver(uint32_t seed, int bridge) {
	gen.startsFromRom();
	RiverGen::Start starts[4];
	for (int n = 0; n < 4; n++) starts[n] = gen.start(n);
	seed &= 0xFFFFFF;
	if (!seed && bridge <= 1) {
		if (bridge == 1)
			for (int n = 1; n < 4; n++) gen.setStart(n, starts[0]);
		return;
	}
	if (bridge > 99999) bridge = 99999;
	// game 1's river (from the seed, with the cartridge's first bank position), run up to the
	// starting bridges: its checkpoint there is that game's start
	World run(*rom_);
	run.gen.s.depotLevelLimit = 0x2E;
	for (int i = 0; i < 4; i++) run.gen.s.checkpoint[i] = starts[0].checkpoint[i];
	for (int i = 0; i < 3 && seed; i++) run.gen.s.checkpoint[i] = uint8_t(seed >> (8 * i));
	run.gen.s.level = starts[0].level;
	run.restoreCheckpoint();
	int at = 1;   // the bridge the river is flying towards
	uint8_t line[kLineBytes];
	auto runTo = [&](int target, RiverGen::Start &start) {
		for (; at < target; at++) {
			// a new checkpoint at each bridge; the level changes with it (after 80: 79, 80, 79...)
			const uint8_t level = run.gen.s.level;
			while (run.gen.s.level == level) run.nextLine(line);
		}
		std::memcpy(start.checkpoint, run.gen.s.checkpoint, 4);
		start.level = run.gen.s.level;
		start.bridge = target;
	};
	if (bridge > 0) {
		runTo(bridge, starts[0]);
		starts[1] = starts[2] = starts[3] = starts[0];
	} else {
		for (int n = 0; n < 4; n++) runTo(starts[n].bridge, starts[n]);
	}
	for (int n = 0; n < 4; n++) gen.setStart(n, starts[n]);
}

// (const: the map only reads the bytes when saving and comparing)
void World::loadFromRam(const uint8_t *ram) {
	RamMap m;
	mapRam(m);
	m.load(ram);
}

void World::saveToRam(uint8_t *ram) const {
	RamMap m;
	const_cast<World *>(this)->mapRam(m);
	m.save(ram);
}

void World::loadGameOwned(const uint8_t *ram) {
	RamMap m;
	mapRam(m);
	m.load(ram, true);
}

std::vector<std::string> World::diffWithRam(const uint8_t *ram, bool includeGameOwned) const {
	RamMap m;
	const_cast<World *>(this)->mapRam(m);
	std::vector<std::string> out;
	m.diff(ram, includeGameOwned, out);
	return out;
}

}  // namespace rr
