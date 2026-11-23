// The game's state as the original's RAM ($0000-$3FFF), for the checks against traces of the
// original: loading a state, comparing, and writing it back. The bytes come from the classes'
// own lists (mapRam); the player-missile memory and the screen are worked out here.
#include <cstring>
#include <string>

#include "game.h"
#include "objects.h"

namespace rr {

void Game::mapRam(RamMap &m) {
	world_.mapRam(m);
	river_.mapRam(m);
	m.byte("frameCount", 0x18, frameCount_).byte("attractFlags", 0x29, attractFlags_).byte("attractTimer", 0x2A, attractTimer_);
	m.byte("idleTime", 0x78, idleTime_).byte("pauseState", 0x24, pauseState_).byte("selectDelay", 0x25, selectDelay_);
	m.byte("powerOnDone", 0x28, powerOnDone_).byte("screenOn", 0xC7, screenOn_);
	m.byte("scrollFrac", 0x30, scrollFrac_).byte("bridgeFlash", 0x31, bridgeFlash_);
	plane_.mapRam(m);
	bullet_.mapRam(m);
	sound_.mapRam(m);
	hit_.mapRam(m);
	match_.mapRam(m);
	panel_.mapRam(m);
	scroller_.mapRam(m);
}

void Game::loadFromRam(const uint8_t *ram) {
	RamMap m;
	mapRam(m);
	m.load(ram);
	pal_ = ram[0xBF] == 0;   // A06F: GTIA's PAL bits, 0 on a PAL machine
	river_.linesFromRam(ram);
	// colours the VBI set (not in RAM): as it computes them
	colourWater_ = (pauseState_ < 2 && bridgeFlash_ && !((bridgeFlash_ + 1) & 1)) ? 0x42 : 0x84;
	colourEdge_ = 0x20;
	colourBank_ = bankColour(world_.gen.s, world_.objects);
	if (!screenOn_) colourBank_ = colourEdge_ = colourWater_ = 0;
}

void Game::saveToRam(uint8_t *ram) const {
	RamMap m;
	const_cast<Game *>(this)->mapRam(m);   // (only read)
	m.save(ram);
	// player-missile memory as the original has it at the top of the frame (AE99): the sprites'
	// rows (sub $B0F1 leaves the logo's rainbow and the fuel needle below the river, rows
	// $DB-$E1 and $CC-$D5; on PAL 12 and 7 lower)
	std::memset(ram + 0x0B00, 0, 0x500);
	for (int l = 0; l < 0x100; l++) {
		ram[0x0C00 + l] = spriteRow(0, l);
		ram[0x0D00 + l] = spriteRow(1, l);
		ram[0x0E00 + l] = spriteRow(2, l);
		ram[0x0B00 + l] = uint8_t(spriteRow(4, l) | spriteRow(5, l) << 4);
	}
	const int logo = pal_ ? 0xE7 : 0xDB, needle = pal_ ? 0xD3 : 0xCC;
	for (int i = 0; i < 7; i++) ram[0x0E00 + logo + i] = 0xFF, ram[0x0F00 + logo + i] = 0xFC;
	for (int i = 0; i < 10; i++) ram[0x0B00 + needle + i] |= 0x0C;
	ram[0xBF] = pal_ ? 0 : 0x0E;
}

std::vector<std::string> Game::diffWithRam(const uint8_t *orig) const {
	// The collisions do not see the decoration (houses, roads), which only jets fly over; the
	// "touching the edge" bit is read only for helicopters, balloons and ships (moveObjects):
	// the display tests only theirs, and the others' is not compared
	std::vector<uint8_t> copy(orig, orig + 0x4000);
	for (int i = 0; i < kSlots; i++)
		if (!objectKind(world_.objects.slot[i].kind).turnsAtEdge())
			copy[size_t(0x42 + i)] = uint8_t((copy[size_t(0x42 + i)] & ~slotflag::kTouchingEdge) | (world_.objects.slot[i].flags & slotflag::kTouchingEdge));
	const uint8_t *ram = copy.data();
	RamMap m;
	const_cast<Game *>(this)->mapRam(m);   // (only read)
	std::vector<std::string> diffs;
	m.diff(ram, true, diffs);
	const int line = river_.lineDifferingFromRam(ram);
	if (line >= 0) diffs.push_back("screen line " + std::to_string(line));
	return diffs;
}

}  // namespace rr
