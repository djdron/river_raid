// Compare the C++ river generator with a trace of the original, frame by frame.
//
// usage: terrain_check <rom> <ram.bin> <first frame in ram.bin> <start frame> <frames> [max errors] [-fresh N]
//   ram.bin: written by oracle/rr_oracle (-ram A:B), 16 KB per frame
//   -fresh N A: do not load the state from the trace; start game N from scratch, generate
//               every line the original generated after frame A (the frame before its first
//               line after START), then compare from <start frame> on. While the screen is
//               being filled the original makes 2 lines per frame and the emulator's frame
//               boundary can fall between them, so frames in that phase are not compared.
//
// The generator state is loaded once from the frame before <start frame>; after that it
// runs on its own. Before each frame only the fields the rest of the game also changes
// are copied from the original (enemy movement, animation). After each frame every other
// generator field and the newest 48-byte screen line must match the original exactly.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "../src/river_gen.h"

int main(int argc, char **argv) {
	if (argc < 6) {
		std::fprintf(stderr, "usage: %s <rom> <ram.bin> <first frame> <start> <frames> [max errors]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
	FILE *f = std::fopen(argv[2], "rb");
	if (!f) { std::fprintf(stderr, "cannot read %s\n", argv[2]); return 2; }
	std::fseek(f, 0, SEEK_END);
	long size = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	std::vector<uint8_t> trace(size);
	if (std::fread(trace.data(), 1, size, f) != size_t(size)) return 2;
	std::fclose(f);
	const int first = std::atoi(argv[3]), start = std::atoi(argv[4]), count = std::atoi(argv[5]);
	const int maxErr = argc > 6 ? std::atoi(argv[6]) : 5;
	int fresh = 0, freshFrom = 0;
	for (int i = 7; i + 2 < argc; i++)
		if (!std::strcmp(argv[i], "-fresh")) { fresh = std::atoi(argv[i + 1]); freshFrom = std::atoi(argv[i + 2]); }
	const int nFrames = int(size / 0x4000);
	auto ram = [&](int frame) { return trace.data() + size_t(frame - first) * 0x4000; };
	if (start - 1 < first || start + count - 1 >= first + nFrames) { std::fprintf(stderr, "range outside trace\n"); return 2; }

	rr::RiverGen gen(rom);
	uint8_t line[rr::kLineBytes];
	if (fresh) {
		gen.startGame(fresh);
		int total = 0;
		for (int fr = freshFrom + 1; fr < start; fr++) total += (ram(fr)[0x2F] - ram(fr - 1)[0x2F]) & 31;
		for (int k = 0; k < total; k++) gen.nextLine(line);
		gen.loadGameOwned(ram(start - 1));
		auto d0 = gen.diffWithRam(ram(start - 1), false);
		std::printf("fresh start of game %d, %d lines generated: %zu fields differ at frame %d\n", fresh, total, d0.size(), start - 1);
		for (auto &d : d0) std::printf("  %s\n", d.c_str());
	} else {
		gen.loadFromRam(ram(start - 1));
	}
	long lines = 0, badFrames = 0, badLines = 0;
	for (int fr = start; fr < start + count; fr++) {
		const uint8_t *prev = ram(fr - 1), *cur = ram(fr);
		int n = (cur[0x2F] - prev[0x2F]) & 31;           // lines generated in this frame
		gen.loadGameOwned(prev);
		for (int k = 0; k < n; k++) gen.nextLine(line);
		lines += n;
		auto diffs = gen.diffWithRam(cur, false);
		bool lineBad = false;
		if (n > 0) {
			uint16_t addr = uint16_t(cur[0x3F04] | (cur[0x3F05] << 8));
			lineBad = std::memcmp(line, cur + addr, rr::kLineBytes) != 0;
			if (lineBad) {
				badLines++;
				if (badFrames < maxErr) {
					std::printf("frame %d line bytes differ at $%04X\n  port:", fr, addr);
					for (int i = 0; i < rr::kLineBytes; i++) std::printf(" %02X", line[i]);
					std::printf("\n  orig:");
					for (int i = 0; i < rr::kLineBytes; i++) std::printf(" %02X", cur[addr + i]);
					std::printf("\n");
				}
			}
		}
		if (!diffs.empty() || lineBad) {
			if (badFrames < maxErr && !diffs.empty()) {
				std::printf("frame %d (%d lines, level %02X, line %d):\n", fr, n, cur[0x60], cur[0x2F]);
				for (auto &d : diffs) std::printf("  %s\n", d.c_str());
			}
			badFrames++;
			if (badFrames == maxErr) std::printf("... stopping detailed output\n");
			gen.loadFromRam(cur);                          // resync to keep finding other bugs
		}
	}
	std::printf("frames %d, lines %ld, frames with differences %ld, line bitmaps differing %ld\n",
				count, lines, badFrames, badLines);
	return badFrames ? 1 : 0;
}
