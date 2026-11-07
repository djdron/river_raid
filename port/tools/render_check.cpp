// Compare drawScreenLine() with the screen of the original, row by row.
//
// usage: render_check <rom> <strip.bin> <frames.csv> [-game N] [-pal] [-ram ram.bin] [-from F] [-to F] [-first F]
//   strip.bin and frames.csv come from `rr_oracle <rom> oracle/scripts/ghost_film.txt N out -ghost
//   -strip 20` (screen row 20, where new lines appear: 384 colour codes per frame); the C++
//   generator runs alone from the game start.
//   Every frame where a new line has entered, that row must equal the line generated two
//   lines earlier (the screen shows the bitmap one frame later, the generator runs ahead).
//   -first F: last frame before the river scrolls (145 for the ghost film, 37 for
//   oracle/scripts/attract_idle.txt).
//   -ram ram.bin (`rr_oracle -ram 0:N`, same run): take the positions the running game gives
//   the objects (slot x, flags, kind) from RAM instead of the generator's. Rows that still
//   differ are counted separately when the tank on the road ($38) is active, since the game
//   logic draws it.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "../src/river_draw.h"
#include "../src/river_gen.h"

int main(int argc, char **argv) {
	if (argc < 4) {
		std::fprintf(stderr, "usage: %s <rom> <strip.bin> <frames.csv> [-game N] [-pal] [-ram ram.bin] [-from F] [-to F] [-first F]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
	int game = 1, from = 400, to = -1;
	int first = 145;   // frame before the river starts scrolling: 145 after START at 120 (ghost film), 37 in the attract mode
	bool pal = false;
	const char *ramPath = nullptr;
	for (int i = 4; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-game") && i + 1 < argc) game = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-ram") && i + 1 < argc) ramPath = argv[++i];
		else if (!std::strcmp(argv[i], "-from") && i + 1 < argc) from = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-to") && i + 1 < argc) to = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-first") && i + 1 < argc) first = std::atoi(argv[++i]);
	}

	// $2F (line within the block, 0..31) per frame
	std::ifstream csv(argv[3]);
	std::string text;
	std::getline(csv, text);
	int col = -1, n = 0;
	{
		std::stringstream hs(text);
		for (std::string h; std::getline(hs, h, ','); n++)
			if (h == "r2F") col = n;
	}
	if (col < 0) { std::fprintf(stderr, "no r2F column in %s\n", argv[3]); return 2; }
	std::vector<int> r2f;
	while (std::getline(csv, text)) {
		std::stringstream ls(text);
		std::string v;
		for (int c = 0; c <= col; c++) std::getline(ls, v, ',');
		r2f.push_back(int(std::strtol(v.c_str(), nullptr, 16)));
	}

	FILE *strip = std::fopen(argv[2], "rb");
	if (!strip) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
	std::fseek(strip, 0, SEEK_END);
	const long stripFrames = std::ftell(strip) / rr::kRowWidth;
	FILE *ram = ramPath ? std::fopen(ramPath, "rb") : nullptr;
	if (ramPath && !ram) { std::fprintf(stderr, "cannot read %s\n", ramPath); return 2; }
	long ramFrames = 0;
	if (ram) { std::fseek(ram, 0, SEEK_END); ramFrames = std::ftell(ram) / 0x4000; }

	long last = std::min<long>(long(r2f.size()), stripFrames) - 1;
	if (ram) last = std::min(last, ramFrames - 1);
	if (to >= 0) last = std::min<long>(last, to);

	const int kFirstFrame = first;
	rr::RiverGen gen(rom);
	gen.startGame(game, pal);
	std::vector<std::vector<uint8_t>> lines;
	std::vector<rr::RiverState> states;
	long lineCount = 0, compared = 0, exact = 0, tank = 0, firstBad = -1;
	std::vector<uint8_t> want(rr::kRowWidth), mem(0x4000);
	for (long f = kFirstFrame + 1; f <= last; f++) {
		const int step = (r2f[f] - r2f[f - 1] + 32) % 32;
		lineCount += step;
		if (!step || f < from) continue;
		const long shown = lineCount - 2;
		while (long(states.size()) <= shown) {
			lines.emplace_back(rr::kLineBytes);
			gen.nextLine(lines.back().data());
			states.push_back(gen.s);
		}
		rr::RiverState s = states[shown];
		if (ram) {
			std::fseek(ram, (f - 1) * 0x4000L, SEEK_SET);
			if (std::fread(mem.data(), 1, 0x4000, ram) != 0x4000) break;
			for (int k = 0; k < rr::kSlots; k++) {
				s.slot[k].x = mem[0x0516 + k];
				s.slot[k].flags = mem[0x42 + k];
				s.slot[k].kind = mem[0x0500 + k];
			}
		}
		uint8_t got[rr::kRowWidth];
		rr::drawScreenLine(rom, s, lines[shown].data(), got);
		std::fseek(strip, f * long(rr::kRowWidth), SEEK_SET);
		if (std::fread(want.data(), 1, rr::kRowWidth, strip) != size_t(rr::kRowWidth)) break;
		compared++;
		if (!std::memcmp(got, want.data(), rr::kRowWidth)) { exact++; continue; }
		if (ram && mem[0x38]) { tank++; continue; }
		if (firstBad < 0) firstBad = f;
	}
	std::printf("frames %d..%ld: %ld rows compared, %ld exact (%.2f%%)", from, last, compared, exact,
	            compared ? 100.0 * exact / compared : 0.0);
	if (ram) std::printf(", %ld differ while the road tank is active, %ld other", tank, compared - exact - tank);
	std::printf("\n");
	if (firstBad >= 0) std::printf("first unexplained difference at frame %ld\n", firstBad);
	return ram && compared - exact - tank ? 1 : 0;
}
