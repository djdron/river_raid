// Compare the status panel and the scrolling line with screens of the original.
//
// usage: status_check <rom> <screens.bin> <ram.bin>
//   Both files from one run of `rr_oracle <rom> oracle/scripts/attract_idle.txt N out
//   -screens A:B -ram A:B` (same A and B): full 384 x 240 screens and RAM of each frame.
// Checks:
//   - the character sets built by buildCharsets() equal RAM $0600-$09FF (the OS part of the
//     font is taken from that RAM, since the emulator's OS font is not in the cartridge);
//   - Scroller::step() reproduces $3FB3/$26/$27 frame by frame;
//   - rows 180..239 drawn by drawStatusBar() equal the screen, with panel memory and fuel
//     taken from RAM.
#include <cstdio>
#include <cstring>
#include <vector>

#include "../src/status_bar.h"

int main(int argc, char **argv) {
	if (argc < 4) {
		std::fprintf(stderr, "usage: %s <rom> <screens.bin> <ram.bin>\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
	FILE *scr = std::fopen(argv[2], "rb");
	FILE *ram = std::fopen(argv[3], "rb");
	if (!scr || !ram) { std::fprintf(stderr, "cannot read the trace files\n"); return 2; }
	const long frameBytes = rr::kScreenWidth * rr::kScreenHeight;
	std::fseek(scr, 0, SEEK_END);
	std::fseek(ram, 0, SEEK_END);
	const long frames = std::min(std::ftell(scr) / frameBytes, std::ftell(ram) / 0x4000L);
	auto readRam = [&](long f, std::vector<uint8_t> &m) {
		std::fseek(ram, f * 0x4000L, SEEK_SET);
		return std::fread(m.data(), 1, 0x4000, ram) == 0x4000;
	};

	std::vector<uint8_t> mem(0x4000), prev(0x4000);
	readRam(0, mem);
	uint8_t os[rr::kOsFontBytes] = {};
	std::memcpy(os, &mem[0x0800], 256);
	std::memcpy(os + 0x100, &mem[0x0900], 256);
	std::memcpy(os + 0x300, &mem[0x0700], 256);
	const rr::Charsets cs = rr::buildCharsets(rom, os);
	const bool charsetsOk = !std::memcmp(cs.scroller, &mem[0x0600], 512) && !std::memcmp(cs.panel, &mem[0x0800], 512);
	std::printf("character sets: %s\n", charsetsOk ? "equal to RAM" : "DIFFERENT");

	rr::Scroller sc;
	sc.textLo = mem[0x3FB3];
	sc.fineScroll = mem[0x26];
	sc.logoX = mem[0x27];
	long stateBad = 0, compared = 0, exactFrames = 0, panelBad = 0, scrollerBad = 0, edgeOnly = 0;
	static uint8_t got[rr::kScreenHeight][rr::kScreenWidth], want[rr::kScreenHeight][rr::kScreenWidth];
	for (long f = 1; f < frames; f++) {
		prev.swap(mem);
		if (!readRam(f, mem)) break;
		sc.step(mem[0x2A] != 0);
		if (sc.textLo != mem[0x3FB3] || sc.fineScroll != mem[0x26] || sc.logoX != mem[0x27]) {
			if (!stateBad) std::printf("scroller state differs first at frame %ld\n", f);
			stateBad++;
			sc.textLo = mem[0x3FB3];
			sc.fineScroll = mem[0x26];
			sc.logoX = mem[0x27];
		}
		std::fseek(scr, f * frameBytes, SEEK_SET);
		if (std::fread(want, 1, size_t(frameBytes), scr) != size_t(frameBytes)) break;
		rr::drawStatusBar(rom, cs, &mem[0x1000], mem[0x5B], sc, prev[0x27], got);
		compared++;
		bool panelSame = true, scrollerSame = true, edges = true;
		for (int y = rr::kRiverTop + rr::kRiverRows; y < rr::kScreenHeight; y++) {
			for (int x = 0; x < rr::kScreenWidth; x++) {
				if (got[y][x] == want[y][x]) continue;
				if (y < 210) panelSame = false;
				else scrollerSame = false;
				if (x >= 24 && x < 360) edges = false;
			}
		}
		if (!panelSame) panelBad++;
		if (!scrollerSame) {
			scrollerBad++;
			if (panelSame && edges) edgeOnly++;
		}
		if (panelSame && scrollerSame) exactFrames++;
	}
	std::printf("%ld frames: %ld identical; panel differs in %ld; scrolling line differs in %ld (%ld of them only in "
		"columns <24 or >=360); scroller state differs in %ld\n",
		compared, exactFrames, panelBad, scrollerBad, edgeOnly, stateBad);
	return charsetsOk && !panelBad && !stateBad ? 0 : 1;
}
