// Compare Game::frame() with a RAM trace of the original, frame by frame.
//
// usage: game_check <rom> <ram.bin> <random.log> <first frame> <frames> [-ram-base F] [-resync]
//                   [-attract | -poweron] [-screens screens.bin first]
//   ram.bin from `rr_oracle ... -ram A:B` (frame A at offset 0, give A as -ram-base),
//   random.log from the same run (values the original read from RANDOM, replayed in order).
//   The state is loaded from the RAM of <first frame>; then every frame is run and compared:
//   generator fields, object slots, game fields, collision results, player-missile memory and
//   the river lines on screen. -resync reloads the state from RAM after every frame, so each
//   frame is checked on its own. -attract: start from Game::startAttract() instead of RAM
//   (give 38 as the first frame) and never reload: the whole attract mode from power-on.
//   -poweron: start from Game::powerOn() (give 0 as the first frame), never reload. -screens (rr_oracle -screens first:last, same run): the river
//   area drawn by Game::drawRiver() is compared with the original's screen of each frame.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <tuple>
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
std::vector<std::tuple<const char *, uint16_t, uint8_t *, int>> gameArrays(rr::Game &game) {
	rr::GameState &g = game.g;
	return {{"highScore", 0x80, g.highScore, 16}, {"checksum", 0x90, g.checksum, 16}, {"panel", 0x1000, g.panel, 60}};
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
	for (auto &a : gameArrays(game)) std::memcpy(std::get<2>(a), ram + std::get<1>(a), size_t(std::get<3>(a)));
	std::memcpy(game.g.pm.bytes.data(), ram + 0x0B00, game.g.pm.bytes.size());
	game.screen = screenFromRam(ram);
	// colours the VBI set (not in RAM): as it computes them
	rr::GameState &g = game.g;
	g.colourWater = (g.state24 < 2 && g.bridgeFlash && !((g.bridgeFlash + 1) & 1)) ? 0x42 : 0x84;
	g.colourEdge = 0x20;
	g.colourBank = rr::bankColour(game.gen.s);
	if (!g.screenOn) g.colourBank = g.colourEdge = g.colourWater = 0;
}

}  // namespace

int main(int argc, char **argv) {
	if (argc < 6) {
		std::fprintf(stderr, "usage: %s <rom> <ram.bin> <random.log> <first frame> <frames> [-ram-base F] [-resync] [-attract | -poweron] [-screens screens.bin first]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM\n"); return 2; }
	FILE *ram = std::fopen(argv[2], "rb");
	FILE *rnd = std::fopen(argv[3], "r");
	if (!ram || !rnd) { std::fprintf(stderr, "cannot read the trace\n"); return 2; }
	const long first = std::atol(argv[4]), count = std::atol(argv[5]);
	long base = 0;
	bool resync = false, fromStart = false, powerOn = false;
	FILE *scr = nullptr;
	long scrBase = 0, scrFrames = 0, scrBadFrames = 0, scrBadRows = 0, scrShown = 0;
	for (int i = 6; i < argc; i++) {
		if (!std::strcmp(argv[i], "-ram-base") && i + 1 < argc) base = std::atol(argv[++i]);
		else if (!std::strcmp(argv[i], "-resync")) resync = true;
		else if (!std::strcmp(argv[i], "-attract")) fromStart = true;
		else if (!std::strcmp(argv[i], "-poweron")) powerOn = fromStart = true;
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
	// input of each frame from frames.csv next to ram.bin (joy: 1 up 2 down 4 left 8 right)
	std::map<long, rr::Input> inputs;
	{
		std::string dir = argv[2];
		const size_t slash = dir.find_last_of('/');
		dir = slash == std::string::npos ? std::string(".") : dir.substr(0, slash);
		if (FILE *csv = std::fopen((dir + "/frames.csv").c_str(), "r")) {
			char row[512];
			if (!std::fgets(row, sizeof row, csv)) row[0] = 0;
			while (std::fgets(row, sizeof row, csv)) {
				// frame,joy,fire,start,(18 CPU/RAM columns),select,option,joy2,fire2,key
				long fr; int joy, fire, start;
				if (std::sscanf(row, "%ld,%d,%d,%d", &fr, &joy, &fire, &start) == 4) {
					rr::Input in;
					in.stick[0] = uint8_t(~joy & 0x0F);
					in.trigger[0] = fire != 0;
					in.start = start != 0;
					int col = 0, extra[5] = {}, n = 0;
					for (char *c = row; *c; c++)
						if (*c == ',' && ++col >= 22 && n < 5) extra[n++] = std::atoi(c + 1);
					in.select = extra[0] != 0;
					in.option = extra[1] != 0;
					in.stick[1] = uint8_t(~extra[2] & 0x0F);
					in.trigger[1] = extra[3] != 0;
					in.key = extra[4] != 0;
					inputs[fr] = in;
				}
			}
			std::fclose(csv);
		}
	}
	std::vector<uint8_t> mem(0x4000);
	auto readRam = [&](long f) {
		std::fseek(ram, (f - base) * 0x4000L, SEEK_SET);
		return std::fread(mem.data(), 1, 0x4000, ram) == 0x4000;
	};

	rr::Game game(rom);
	if (!readRam(first)) { std::fprintf(stderr, "frame %ld not in the trace\n", first); return 2; }
	if (powerOn) game.powerOn();
	else if (fromStart) game.startAttract();
	else load(game, mem.data());
	std::map<std::string, long> diffCount;
	long framesBad = 0, randomBad = 0, shown = 0, busyFrames = 0;
	for (long f = first + 1; f < first + 1 + count; f++) {
		std::deque<uint8_t> &rq = randoms[f];
		bool underflow = false;
		auto random = [&]() -> uint8_t {
			if (rq.empty()) { underflow = true; return 0; }
			const uint8_t v = rq.front();
			rq.pop_front();
			return v;
		};
		game.afterDisplay = [&]() {
		if (scr && f >= scrBase && !game.busy()) {
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
		};
		game.frame(inputs.count(f) ? inputs[f] : rr::Input(), random);
		if (!rq.empty() || underflow) {
			randomBad++;
			rq.clear();
		}
		if (!readRam(f)) break;
		if (game.busy()) { busyFrames++; continue; }   // memory being cleared: nothing to compare
		std::vector<std::string> diffs = game.gen.diffWithRam(mem.data(), true);
		for (auto &fd : gameFields(game))
			if (*fd.second != mem[fd.first.second]) {
				char b[96];
				std::snprintf(b, sizeof b, "%s $%02X: port=%02X orig=%02X", fd.first.first, fd.first.second, *fd.second, mem[fd.first.second]);
				diffs.push_back(b);
			}
		for (auto &a : gameArrays(game))
			for (int i = 0; i < std::get<3>(a); i++)
				if (std::get<2>(a)[i] != mem[std::get<1>(a) + i]) {
					char b[96];
					std::snprintf(b, sizeof b, "%s[%d] $%04X: port=%02X orig=%02X", std::get<0>(a), i, std::get<1>(a) + i,
					              std::get<2>(a)[i], mem[std::get<1>(a) + i]);
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
	std::printf("%ld frames (%ld of them during resets, not compared), %ld with differences%s, %ld with a different number of RANDOM reads\n",
	            count, busyFrames, framesBad, fromStart ? "" : " (state reloaded from RAM after each)", randomBad);
	if (scr) std::printf("screens: %ld frames compared, %ld with differences (%ld rows of the river area)\n", scrFrames, scrBadFrames, scrBadRows);
	for (auto &d : diffCount) std::printf("  %-40s %ld\n", d.first.c_str(), d.second);
	return framesBad ? 1 : 0;
}
