// Mapping of the game state to the original's RAM ($0000-$3FFF), for the checks against
// traces of the original: loading a state, comparing, and writing it back.
#include <cstdio>
#include <cstring>
#include <tuple>

#include "game.h"

namespace rr {
namespace {

// Game fields compared with RAM (address, reference into the game).
std::vector<std::pair<std::pair<const char *, uint16_t>, uint8_t *>> gameFields(const Game &game) {
	GameState &g = const_cast<GameState &>(game.g);
	return {
	    {{"frameCount", 0x18}, &g.frameCount}, {{"flags29", 0x29}, &g.flags29},
	    {{"attractTimer", 0x2A}, &g.attractTimer}, {{"deathTimer", 0x76}, &g.deathTimer},
	    {{"idle", 0x78}, &g.idle}, {{"speed", 0x59}, &g.speed}, {{"scrollFrac", 0x30}, &g.scrollFrac},
	    {{"shellFrame", 0x3B}, &g.shellFrame}, {{"shellX", 0x3C}, &g.shellX}, {{"shellXLo", 0x3D}, &g.shellXLo},
	    {{"missileDx", 0xC0}, &g.missileDx}, {{"planeX", 0x57}, &g.planeX}, {{"planeFrame", 0x5E}, &g.planeFrame},
	    {{"soundExplosion", 0x7C}, &g.soundExplosion}, {{"soundMissile", 0x7F}, &g.soundMissile},
	    {{"bridgeFlash", 0x31}, &g.bridgeFlash}, {{"fuel", 0x5B}, &g.fuel}, {{"joystick", 0x19}, &g.joystick},
	    {{"joystickPrev", 0xB9}, &g.joystickPrev}, {{"c6", 0xC6}, &g.c6}, {{"state24", 0x24}, &g.state24},
	    {{"optionDelay", 0x25}, &g.optionDelay}, {{"gameNumber", 0x09}, &g.gameNumber}, {{"player", 0x77}, &g.player},
	    {{"fuelLo", 0x5C}, &g.fuelLo}, {{"sound7A", 0x7A}, &g.sound7A}, {{"sound7B", 0x7B}, &g.sound7B},
	    {{"soundFire", 0x7D}, &g.soundFire}, {{"soundScore", 0x7E}, &g.soundScore}, {{"deathPitch", 0x052C}, &g.deathPitch},
	    {{"hit.m2pf", 0x1A}, &g.hit.m2pf}, {{"hit.p2pf", 0x1B}, &g.hit.p2pf}, {{"hit.m2p3", 0x1C}, &g.hit.m2p3},
	    {{"hit.p2p3", 0x1D}, &g.hit.p2p3}, {{"hit.p2p0", 0x1E}, &g.hit.p2p0}, {{"hit.p3p0", 0x1F}, &g.hit.p3p0},
	    {{"hit.p1pf", 0x20}, &g.hit.p1pf}, {{"hit.m0p2", 0x21}, &g.hit.m0p2}, {{"hit.m0pf", 0x22}, &g.hit.m0pf},
	    {{"planeXFrac", 0x58}, &g.planeXFrac}, {{"planeAccel", 0x2B}, &g.planeAccel}, {{"bulletY", 0x56}, &g.bulletY},
	    {{"bridges0", 0x61}, &g.bridges[0]}, {{"bridges1", 0x62}, &g.bridges[1]},
	    {{"score0", 0x63}, &g.score[0]}, {{"score1", 0x64}, &g.score[1]}, {{"score2", 0x65}, &g.score[2]},
	    {{"lives", 0x6A}, &g.lives}, {{"other.level", 0x6B}, &g.other.level},
	    {{"other.bridges0", 0x6C}, &g.other.bridges[0]}, {{"other.bridges1", 0x6D}, &g.other.bridges[1]},
	    {{"other.score0", 0x6E}, &g.other.score[0]}, {{"other.score1", 0x6F}, &g.other.score[1]},
	    {{"other.score2", 0x70}, &g.other.score[2]}, {{"other.save0", 0x71}, &g.other.save[0]},
	    {{"other.save1", 0x72}, &g.other.save[1]}, {{"other.save2", 0x73}, &g.other.save[2]},
	    {{"other.save3", 0x74}, &g.other.save[3]}, {{"other.lives", 0x75}, &g.other.lives},
	    {{"scroller.textLo", 0x3FB3}, &g.scroller.textLo}, {{"scroller.fineScroll", 0x26}, &g.scroller.fineScroll},
	    {{"scroller.logoX", 0x27}, &g.scroller.logoX},
	    {{"respawned", 0x79}, &g.respawned}, {{"powerOnDone", 0x28}, &g.powerOnDone}, {{"screenOn", 0xC7}, &g.screenOn},
	};
}

// Arrays compared with RAM: high scores and their check bytes, the panel.
std::vector<std::tuple<const char *, uint16_t, uint8_t *, int>> gameArrays(const Game &game) {
	GameState &g = const_cast<GameState &>(game.g);
	return {{"highScore", 0x80, g.highScore, 16}, {"checksum", 0x90, g.checksum, 16}, {"panel", 0x1000, g.panel, 60}};
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
	gen.loadFromRam(ram);
	for (auto &f : gameFields(*this)) *f.second = ram[f.first.second];
	for (auto &a : gameArrays(*this)) std::memcpy(std::get<2>(a), ram + std::get<1>(a), size_t(std::get<3>(a)));
	std::memcpy(g.pm.bytes.data(), ram + 0x0B00, g.pm.bytes.size());
	screen = screenFromRam(ram);
	g.screenTop = uint16_t(ram[0x3F04] | (ram[0x3F05] << 8));
	// colours the VBI set (not in RAM): as it computes them
	g.colourWater = (g.state24 < 2 && g.bridgeFlash && !((g.bridgeFlash + 1) & 1)) ? 0x42 : 0x84;
	g.colourEdge = 0x20;
	g.colourBank = bankColour(gen.s);
	if (!g.screenOn) g.colourBank = g.colourEdge = g.colourWater = 0;
}

void Game::saveToRam(uint8_t *ram) const {
	gen.saveToRam(ram);
	for (auto &f : gameFields(*this)) ram[f.first.second] = *f.second;
	for (auto &a : gameArrays(*this)) std::memcpy(ram + std::get<1>(a), std::get<2>(a), size_t(std::get<3>(a)));
	std::memcpy(ram + 0x0B00, g.pm.bytes.data(), g.pm.bytes.size());
	ram[0x3F04] = uint8_t(g.screenTop);
	ram[0x3F05] = uint8_t(g.screenTop >> 8);
}

std::vector<std::string> Game::diffWithRam(const uint8_t *ram) const {
	std::vector<std::string> diffs = gen.diffWithRam(ram, true);
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
	if (g.screenTop != top) {
		std::snprintf(b, sizeof b, "screenTop: port=%04X orig=%04X", g.screenTop, top);
		diffs.push_back(b);
	}
	for (size_t i = 0; i < g.pm.bytes.size(); i++)
		if (g.pm.bytes[i] != ram[0x0B00 + i]) {
			std::snprintf(b, sizeof b, "pm $%04zX: port=%02X orig=%02X", 0x0B00 + i, g.pm.bytes[i], ram[0x0B00 + i]);
			diffs.push_back(b);
		}
	const auto lines = screenFromRam(ram);
	for (size_t i = 0; i < lines.size() && i < screen.size(); i++)
		if (lines[i] != screen[i]) {
			std::snprintf(b, sizeof b, "screen line %zu", i);
			diffs.push_back(b);
			break;
		}
	return diffs;
}

}  // namespace rr
