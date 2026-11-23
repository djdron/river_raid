// Mapping of the game state to the original's RAM ($0000-$3FFF), for the checks against
// traces of the original: loading a state, comparing, and writing it back.
#include <cstdio>
#include <cstring>
#include <tuple>

#include "game.h"
#include "objects.h"

namespace rr {
namespace {

// Game fields compared with RAM (address, reference into the game).
std::vector<std::pair<std::pair<const char *, uint16_t>, uint8_t *>> gameFields(const Game &game) {
	Game &g = const_cast<Game &>(game);
	World &world = const_cast<World &>(game.world);
	return {
		{{"frameCount", 0x18}, &g.frameCount}, {{"attractFlags", 0x29}, &g.attractFlags},
		{{"attractTimer", 0x2A}, &g.attractTimer}, {{"planeState", 0x76}, &g.plane.state},
		{{"idleTime", 0x78}, &g.idleTime}, {{"speed", 0x59}, &g.plane.speed}, {{"scrollFrac", 0x30}, &g.scrollFrac},
		{{"shellFrame", 0x3B}, &world.objects.shell.frame}, {{"shellX", 0x3C}, &world.objects.shell.x}, {{"shellXFrac", 0x3D}, &world.objects.shell.xFrac},
		{{"missileStep", 0xC0}, &world.objects.missile.step}, {{"planeX", 0x57}, &g.plane.x}, {{"planeFrame", 0x5E}, &g.plane.frame},
		{{"soundExplosion", 0x7C}, &g.sound.explosion}, {{"soundMissile", 0x7F}, &g.sound.missile},
		{{"bridgeFlash", 0x31}, &g.bridgeFlash}, {{"fuel", 0x5B}, &g.plane.fuel}, {{"joystick", 0x19}, &g.plane.stick},
		{{"joystickPrev", 0xB9}, &g.plane.stickPrev}, {{"startBeep", 0xC6}, &g.sound.startBeep}, {{"pauseState", 0x24}, &g.pauseState},
		{{"selectDelay", 0x25}, &g.selectDelay}, {{"gameNumber", 0x09}, &g.gameNumber}, {{"player", 0x77}, &g.player},
		{{"fuelFrac", 0x5C}, &g.plane.fuelFrac}, {{"eventSound", 0x7A}, &g.sound.event}, {{"eventSoundTime", 0x7B}, &g.sound.eventTime},
		{{"soundFire", 0x7D}, &g.sound.fire}, {{"soundBonus", 0x7E}, &g.sound.bonus}, {{"crashPitch", 0x052C}, &g.sound.crashPitch},
		{{"hit.bulletHitBank", 0x1A}, &g.hit.bulletHitBank}, {{"hit.planeHitBank", 0x1B}, &g.hit.planeHitBank}, {{"hit.bulletHitObject", 0x1C}, &g.hit.bulletHitObject},
		{{"hit.planeHitObject", 0x1D}, &g.hit.planeHitObject}, {{"hit.planeHitShell", 0x1E}, &g.hit.planeHitShell}, {{"hit.shellHitObject", 0x1F}, &g.hit.shellHitObject},
		{{"hit.tankAtEdge", 0x20}, &g.hit.tankAtEdge}, {{"hit.missileHitPlane", 0x21}, &g.hit.missileHitPlane}, {{"hit.missileHitBank", 0x22}, &g.hit.missileHitBank},
		{{"planeXFrac", 0x58}, &g.plane.xFrac}, {{"planeAccel", 0x2B}, &g.plane.accel}, {{"bulletY", 0x56}, &g.bullet.y},
		{{"bridges0", 0x61}, &g.bridges.d[0]}, {{"bridges1", 0x62}, &g.bridges.d[1]},
		{{"score0", 0x63}, &g.score.d[0]}, {{"score1", 0x64}, &g.score.d[1]}, {{"score2", 0x65}, &g.score.d[2]},
		{{"lives", 0x6A}, &g.lives}, {{"other.level", 0x6B}, &g.other.level},
		{{"other.bridges0", 0x6C}, &g.other.bridges.d[0]}, {{"other.bridges1", 0x6D}, &g.other.bridges.d[1]},
		{{"other.score0", 0x6E}, &g.other.score.d[0]}, {{"other.score1", 0x6F}, &g.other.score.d[1]},
		{{"other.score2", 0x70}, &g.other.score.d[2]}, {{"other.checkpoint0", 0x71}, &g.other.checkpoint[0]},
		{{"other.checkpoint1", 0x72}, &g.other.checkpoint[1]}, {{"other.checkpoint2", 0x73}, &g.other.checkpoint[2]},
		{{"other.checkpoint3", 0x74}, &g.other.checkpoint[3]}, {{"other.lives", 0x75}, &g.other.lives},
		{{"scroller.textLo", 0x3FB3}, &g.scroller.textLo}, {{"scroller.fineScroll", 0x26}, &g.scroller.fineScroll},
		{{"scroller.logoX", 0x27}, &g.scroller.logoX},
		{{"respawned", 0x79}, &g.respawned}, {{"powerOnDone", 0x28}, &g.powerOnDone}, {{"screenOn", 0xC7}, &g.screenOn},
	};
}

// Arrays compared with RAM: high scores and their check bytes, the panel.
std::vector<std::tuple<const char *, uint16_t, uint8_t *, int>> gameArrays(const Game &game) {
	Game &g = const_cast<Game &>(game);
	return {{"highScore", 0x80, g.highScores.table, 16}, {"highScoreCheck", 0x90, g.highScores.check, 16}, {"panel", 0x1000, g.panel.chars, 60}};
}

// River lines in screen memory as the display list at $3F00 shows them (160 lines).
std::vector<std::array<uint8_t, kLineBytes>> screenFromRam(const uint8_t *ram) {
	std::vector<std::array<uint8_t, kLineBytes>> lines;
	int pc = 0x3F03;   // after 70 F0 30
	uint16_t addr = 0;
	while (lines.size() < 160 && pc < 0x3FFF) {
		const uint8_t op = ram[pc++];
		if ((op & 0x0F) != 0x0E) break;
		if (op & 0x40) { addr = uint16_t(ram[pc] | (ram[pc + 1] << 8)); pc += 2; }
		std::array<uint8_t, kLineBytes> l;
		std::memcpy(l.data(), ram + addr, kLineBytes);
		lines.push_back(l);
		addr = uint16_t(addr + kLineBytes);
	}
	return lines;
}

}  // namespace

void Game::loadFromRam(const uint8_t *ram) {
	world.loadFromRam(ram);
	for (auto &f : gameFields(*this)) *f.second = ram[f.first.second];
	for (auto &a : gameArrays(*this)) std::memcpy(std::get<2>(a), ram + std::get<1>(a), size_t(std::get<3>(a)));
	pal_ = ram[0xBF] == 0;   // A06F: GTIA's PAL bits, 0 on a PAL machine
	river.lines = screenFromRam(ram);
	river.top = uint16_t(ram[0x3F04] | (ram[0x3F05] << 8));
	// colours the VBI set (not in RAM): as it computes them
	colourWater = (pauseState < 2 && bridgeFlash && !((bridgeFlash + 1) & 1)) ? 0x42 : 0x84;
	colourEdge = 0x20;
	colourBank = bankColour(world.gen.s, world.objects);
	if (!screenOn) colourBank = colourEdge = colourWater = 0;
}

void Game::saveToRam(uint8_t *ram) const {
	world.saveToRam(ram);
	for (auto &f : gameFields(*this)) ram[f.first.second] = *f.second;
	for (auto &a : gameArrays(*this)) std::memcpy(ram + std::get<1>(a), std::get<2>(a), size_t(std::get<3>(a)));
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
	ram[0x3F04] = uint8_t(river.top);
	ram[0x3F05] = uint8_t(river.top >> 8);
}

std::vector<std::string> Game::diffWithRam(const uint8_t *orig) const {
	// The collisions do not see the decoration (houses, roads), which only jets fly over; the
	// "touching the edge" bit is read only for helicopters, balloons and ships (moveObjects):
	// the display tests only theirs, and the others' is not compared
	std::vector<uint8_t> copy(orig, orig + 0x4000);
	for (int i = 0; i < kSlots; i++)
		if (!objectKind(world.objects.slot[i].kind).turnsAtEdge())
			copy[size_t(0x42 + i)] = uint8_t((copy[size_t(0x42 + i)] & ~slotflag::kTouchingEdge) | (world.objects.slot[i].flags & slotflag::kTouchingEdge));
	const uint8_t *ram = copy.data();
	std::vector<std::string> diffs = world.diffWithRam(ram, true);
	char b[96];
	for (auto &fd : gameFields(*this))
		if (*fd.second != ram[fd.first.second]) {
			std::snprintf(b, sizeof b, "%s $%02X: port=%02X orig=%02X", fd.first.first, fd.first.second, *fd.second, ram[fd.first.second]);
			diffs.push_back(b);
		}
	for (auto &a : gameArrays(*this))
		for (int i = 0; i < std::get<3>(a); i++)
			if (std::get<2>(a)[i] != ram[std::get<1>(a) + i]) {
				std::snprintf(b, sizeof b, "%s[%d] $%04X: port=%02X orig=%02X", std::get<0>(a), i, std::get<1>(a) + i,
					std::get<2>(a)[i], ram[std::get<1>(a) + i]);
				diffs.push_back(b);
			}
	const uint16_t top = uint16_t(ram[0x3F04] | (ram[0x3F05] << 8));
	if (river.top != top) {
		std::snprintf(b, sizeof b, "screenTop: port=%04X orig=%04X", river.top, top);
		diffs.push_back(b);
	}
	const auto lines = screenFromRam(ram);
	for (size_t i = 0; i < lines.size() && i < river.lines.size(); i++)
		if (lines[i] != river.lines[i]) {
			std::snprintf(b, sizeof b, "screen line %zu", i);
			diffs.push_back(b);
			break;
		}
	return diffs;
}

}  // namespace rr
