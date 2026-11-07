// Compare the sound driver (Game::soundDriver, VBI B2A5-B3D8) with the original.
//
// usage: sound_check <rom> <vbi.bin> <pokey.log> <random.log>
//   Files from `rr_oracle ... -vbi`: vbi.bin holds the RAM as each vertical blank interrupt
//   starts. For every frame the driver runs on that state (with the RANDOM values the original
//   read in its VBI) and the AUDF/AUDC values it sets are compared with the POKEY writes the
//   original made in that VBI.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <vector>

#include "../src/game.h"

int main(int argc, char **argv) {
	if (argc < 5) {
		std::fprintf(stderr, "usage: %s <rom> <vbi.bin> <pokey.log> <random.log>\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM\n"); return 2; }
	FILE *vbi = std::fopen(argv[2], "rb");
	FILE *plog = std::fopen(argv[3], "r");
	FILE *rlog = std::fopen(argv[4], "r");
	if (!vbi || !plog || !rlog) { std::fprintf(stderr, "cannot read the trace\n"); return 2; }

	// POKEY writes and RANDOM reads made during the VBI (scanline 240 and later) of each frame
	std::map<long, std::vector<std::pair<int, int>>> writes;
	std::map<long, std::deque<uint8_t>> randoms;
	char line[128];
	while (std::fgets(line, sizeof line, plog)) {
		long f; int sc, xp; unsigned reg, v;
		if (line[0] != '#' && std::sscanf(line, "%ld %d %d %x %x", &f, &sc, &xp, &reg, &v) == 5 && sc >= 240 && reg < 8)
			writes[f].push_back({int(reg), int(v)});
	}
	while (std::fgets(line, sizeof line, rlog)) {
		long f; int sc, xp; unsigned v;
		if (line[0] != '#' && std::sscanf(line, "%ld %d %d %x", &f, &sc, &xp, &v) == 4 && sc >= 240) randoms[f].push_back(uint8_t(v));
	}

	rr::Game game(rom);
	std::vector<uint8_t> mem(0x4000);
	uint8_t regs[8] = {};   // the original's registers, from its writes
	long frames = 0, bad = 0, active = 0, shown = 0;
	int32_t frame = 0;
	while (std::fread(&frame, 4, 1, vbi) == 1 && std::fread(mem.data(), 1, mem.size(), vbi) == mem.size()) {
		for (auto &w : writes[frame]) regs[w.first] = uint8_t(w.second);
		rr::GameState &g = game.g;
		game.gen.loadFromRam(mem.data());
		g.startBeep = mem[0xC6]; g.pauseState = mem[0x24]; g.eventSound = mem[0x7A]; g.eventSoundTime = mem[0x7B];
		g.planeState = mem[0x76]; g.fuel = mem[0x5B]; g.fuelFrac = mem[0x5C]; g.speed = mem[0x59];
		g.crashPitch = mem[0x052C]; g.joystick = mem[0x19]; g.soundExplosion = mem[0x7C];
		g.soundMissile = mem[0x7F]; g.attractFlags = mem[0x29]; g.soundFire = mem[0x7D]; g.soundScore = mem[0x7E];
		// registers not written in this VBI keep the original's values
		for (int c = 0; c < 4; c++) { g.pokey.audf[c] = regs[2 * c]; g.pokey.audc[c] = regs[2 * c + 1]; }
		std::deque<uint8_t> &rq = randoms[frame];
		game.soundDriver([&]() -> uint8_t {
			if (rq.empty()) return 0;
			const uint8_t v = rq.front();
			rq.pop_front();
			return v;
		});
		frames++;
		bool same = true, sounding = false;
		for (int c = 0; c < 4; c++) {
			if (game.g.pokey.audf[c] != regs[2 * c] || game.g.pokey.audc[c] != regs[2 * c + 1]) same = false;
			if (regs[2 * c + 1] & 0x0F) sounding = true;
		}
		if (sounding) active++;
		if (!same) {
			bad++;
			if (shown++ < 8) {
				std::printf("frame %d:", frame);
				for (int c = 0; c < 4; c++)
					std::printf("  ch%d port %02X/%02X orig %02X/%02X", c + 1, game.g.pokey.audf[c], game.g.pokey.audc[c],
					            regs[2 * c], regs[2 * c + 1]);
				std::printf("\n");
			}
		}
	}
	std::printf("%ld frames (%ld with sound), %ld with different registers\n", frames, active, bad);
	return bad ? 1 : 0;
}
