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
	std::map<long, unsigned> pcs;   // where the original's CPU was at the end of each frame
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
					unsigned pc = 0;
					if (std::sscanf(row, "%*d,%*d,%*d,%*d,%x", &pc) == 1) pcs[fr] = pc;
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
	else game.loadFromRam(mem.data());
	std::map<std::string, long> diffCount;
	long framesBad = 0, randomBad = 0, shown = 0, busyFrames = 0, midFrames = 0;
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
		std::vector<std::string> diffs = game.diffWithRam(mem.data());
		// The RAM snapshot is taken at the end of the frame. Normally the original's main loop
		// is then waiting for the next frame (AE99-AE9E); if it is still working (the river
		// refill after a reset draws 2 lines per frame and can run past the end), the snapshot
		// holds a half-done frame and is not compared.
		const bool waiting = !pcs.count(f) || pcs[f] == 0xAE99 || pcs[f] == 0xAE9C || pcs[f] == 0xAE9E;
		if (!diffs.empty() && !waiting) {
			midFrames++;
			std::printf("frame %ld: the original is still working at the end of the frame (pc $%04X), not compared\n", f, pcs[f]);
			continue;
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
			if (!resync && !fromStart) game.loadFromRam(mem.data());
		}
		if (resync) game.loadFromRam(mem.data());
	}
	std::printf("%ld frames (%ld of them during resets and %ld snapshots of a half-done frame, not compared), %ld with differences%s, %ld with a different number of RANDOM reads\n",
		count, busyFrames, midFrames, framesBad, fromStart ? "" : " (state reloaded from RAM after each)", randomBad);
	if (scr) std::printf("screens: %ld frames compared, %ld with differences (%ld rows of the river area)\n", scrFrames, scrBadFrames, scrBadRows);
	for (auto &d : diffCount) std::printf("  %-40s %ld\n", d.first.c_str(), d.second);
	return framesBad ? 1 : 0;
}
