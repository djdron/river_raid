// Compare Game::frame() with a RAM trace of the original, frame by frame.
//
// usage: game_check <rom> <ram.bin> <random.log> <first frame> <frames> [-ram-base F] [-resync]
//                   [-attract] [-screens screens.bin first]
//   ram.bin from `rr_oracle ... -ram A:B` (frame A at offset 0, give A as -ram-base),
//   random.log from the same run (values the original read from RANDOM, replayed in order).
//   The state is loaded from the RAM of <first frame>; then every frame is run and compared:
//   generator fields, object slots, game fields, collision results, player-missile memory and
//   the river lines on screen. -resync reloads the state from RAM after every frame, so each
//   frame is checked on its own. -attract: start from Game::startAttract() instead of RAM
//   (give 38 as the first frame) and never reload: the whole attract mode from power-on. -screens (rr_oracle -screens first:last, same run): the river
//   area drawn by Game::drawRiver() is compared with the original's screen of each frame.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "../src/game.h"

namespace {

// Game fields compared with RAM (address, reference into the game).
std::vector<std::pair<std::pair<const char *, uint16_t>, uint8_t *>> gameFields(rr::Game &game) {
	rr::GameState &g = game.g;
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
	    {{"hit.m2pf", 0x1A}, &g.hit.m2pf}, {{"hit.p2pf", 0x1B}, &g.hit.p2pf}, {{"hit.m2p3", 0x1C}, &g.hit.m2p3},
	    {{"hit.p2p3", 0x1D}, &g.hit.p2p3}, {{"hit.p2p0", 0x1E}, &g.hit.p2p0}, {{"hit.p3p0", 0x1F}, &g.hit.p3p0},
	    {{"hit.p1pf", 0x20}, &g.hit.p1pf}, {{"hit.m0p2", 0x21}, &g.hit.m0p2}, {{"hit.m0pf", 0x22}, &g.hit.m0pf},
	};
}

// River lines in screen memory as the display list at $3F00 shows them (160 lines).
std::vector<std::array<uint8_t, rr::kLineBytes>> screenFromRam(const uint8_t *ram) {
	std::vector<std::array<uint8_t, rr::kLineBytes>> lines;
	int pc = 0x3F03;   // after 70 F0 30
	uint16_t addr = 0;
	while (lines.size() < 160 && pc < 0x3FFF) {
		const uint8_t op = ram[pc++];
		if ((op & 0x0F) != 0x0E) break;
		if (op & 0x40) { addr = uint16_t(ram[pc] | (ram[pc + 1] << 8)); pc += 2; }
		std::array<uint8_t, rr::kLineBytes> l;
		std::memcpy(l.data(), ram + addr, rr::kLineBytes);
		lines.push_back(l);
		addr = uint16_t(addr + rr::kLineBytes);
	}
	return lines;
}

void load(rr::Game &game, const uint8_t *ram) {
	game.gen.loadFromRam(ram);
	for (auto &f : gameFields(game)) *f.second = ram[f.first.second];
	std::memcpy(game.g.pm.bytes.data(), ram + 0x0B00, game.g.pm.bytes.size());
	game.screen = screenFromRam(ram);
}

}  // namespace

int main(int argc, char **argv) {
	if (argc < 6) {
		std::fprintf(stderr, "usage: %s <rom> <ram.bin> <random.log> <first frame> <frames> [-ram-base F] [-resync] [-attract] [-screens screens.bin first]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM\n"); return 2; }
	FILE *ram = std::fopen(argv[2], "rb");
	FILE *rnd = std::fopen(argv[3], "r");
	if (!ram || !rnd) { std::fprintf(stderr, "cannot read the trace\n"); return 2; }
	const long first = std::atol(argv[4]), count = std::atol(argv[5]);
	long base = 0;
	bool resync = false, fromStart = false;
	FILE *scr = nullptr;
	long scrBase = 0, scrFrames = 0, scrBadFrames = 0, scrBadRows = 0, scrShown = 0;
	for (int i = 6; i < argc; i++) {
		if (!std::strcmp(argv[i], "-ram-base") && i + 1 < argc) base = std::atol(argv[++i]);
		else if (!std::strcmp(argv[i], "-resync")) resync = true;
		else if (!std::strcmp(argv[i], "-attract")) fromStart = true;
		else if (!std::strcmp(argv[i], "-screens") && i + 2 < argc) {
			scr = std::fopen(argv[++i], "rb");
			scrBase = std::atol(argv[++i]);
		}
	}
	std::map<long, std::deque<uint8_t>> randoms;   // frame -> values read
	char lineBuf[128];
	while (std::fgets(lineBuf, sizeof lineBuf, rnd)) {
		if (lineBuf[0] == '#') continue;
		long fr; int sc, xp; unsigned v;
		if (std::sscanf(lineBuf, "%ld %d %d %x", &fr, &sc, &xp, &v) == 4) randoms[fr].push_back(uint8_t(v));
	}
	std::vector<uint8_t> mem(0x4000);
	auto readRam = [&](long f) {
		std::fseek(ram, (f - base) * 0x4000L, SEEK_SET);
		return std::fread(mem.data(), 1, 0x4000, ram) == 0x4000;
	};

	rr::Game game(rom);
	if (!readRam(first)) { std::fprintf(stderr, "frame %ld not in the trace\n", first); return 2; }
	if (fromStart) game.startAttract();
	else load(game, mem.data());
	std::map<std::string, long> diffCount;
	long framesBad = 0, randomBad = 0, shown = 0;
	for (long f = first + 1; f < first + 1 + count; f++) {
		std::deque<uint8_t> &rq = randoms[f];
		bool underflow = false;
		auto random = [&]() -> uint8_t {
			if (rq.empty()) { underflow = true; return 0; }
			const uint8_t v = rq.front();
			rq.pop_front();
			return v;
		};
		game.prepare();
		game.display();
		if (scr && f >= scrBase) {
			static uint8_t got[240][rr::kRowWidth], want[240][rr::kRowWidth];
			std::fseek(scr, (f - scrBase) * long(sizeof want), SEEK_SET);
			if (std::fread(want, 1, sizeof want, scr) == sizeof want) {
				game.drawRiver(got);
				scrFrames++;
				int bad = 0;
				for (int r = 20; r < 180; r++)
					if (std::memcmp(got[r], want[r], sizeof got[r])) bad++;
				if (bad) {
					scrBadFrames++;
					scrBadRows += bad;
					if (scrShown < 5) {
						scrShown++;
						for (int r = 20; r < 180; r++)
							if (std::memcmp(got[r], want[r], sizeof got[r])) {
								int x = 0;
								while (got[r][x] == want[r][x]) x++;
								std::printf("screen frame %ld: %d rows differ, first row %d column %d: port %02X orig %02X\n", f, bad, r, x, got[r][x], want[r][x]);
								break;
							}
					}
				}
			}
		}
		game.logic(random);
		game.vbi(random);
		if (!rq.empty() || underflow) {
			randomBad++;
			rq.clear();
		}
		if (!readRam(f)) break;
		std::vector<std::string> diffs = game.gen.diffWithRam(mem.data(), true);
		for (auto &fd : gameFields(game))
			if (*fd.second != mem[fd.first.second]) {
				char b[96];
				std::snprintf(b, sizeof b, "%s $%02X: port=%02X orig=%02X", fd.first.first, fd.first.second, *fd.second, mem[fd.first.second]);
				diffs.push_back(b);
			}
		for (size_t i = 0; i < game.g.pm.bytes.size(); i++)
			if (game.g.pm.bytes[i] != mem[0x0B00 + i]) {
				char b[96];
				std::snprintf(b, sizeof b, "pm $%04zX: port=%02X orig=%02X", 0x0B00 + i, game.g.pm.bytes[i], mem[0x0B00 + i]);
				diffs.push_back(b);
			}
		const auto scr = screenFromRam(mem.data());
		for (size_t i = 0; i < scr.size() && i < game.screen.size(); i++)
			if (scr[i] != game.screen[i]) {
				char b[64];
				std::snprintf(b, sizeof b, "screen line %zu", i);
				diffs.push_back(b);
				break;
			}
		if (!diffs.empty()) {
			framesBad++;
			for (auto &d : diffs) diffCount[d.substr(0, d.find(':'))]++;
			if (shown < 5) {
				shown++;
				std::printf("frame %ld:", f);
				for (size_t i = 0; i < diffs.size() && i < 12; i++) std::printf("  %s", diffs[i].c_str());
				std::printf("%s\n", diffs.size() > 12 ? "  ..." : "");
			}
			if (!resync && !fromStart) load(game, mem.data());
		}
		if (resync) load(game, mem.data());
	}
	std::printf("%ld frames, %ld with differences%s, %ld with a different number of RANDOM reads\n",
	            count, framesBad, fromStart ? "" : " (state reloaded from RAM after each)", randomBad);
	if (scr) std::printf("screens: %ld frames compared, %ld with differences (%ld rows of the river area)\n", scrFrames, scrBadFrames, scrBadRows);
	for (auto &d : diffCount) std::printf("  %-40s %ld\n", d.first.c_str(), d.second);
	return framesBad ? 1 : 0;
}
