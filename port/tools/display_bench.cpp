// Runs the game for many frames with a simple autopilot, for timing display() (RR_TIME=1,
// RR_DISPLAY=beam / simple; docs/DLI_REMOVAL.md).
//
// usage: display_bench <rom> <frames> [-pal] [-game N] [-seed N]
//   -game N: game N of the menu (7: one player from bridge 50), chosen with SELECT in the
//   attract mode; after a game over the next one starts the same way.
// The autopilot steers to the middle of the water ahead of the plane and keeps firing; the
// plane cannot crash and never runs out of fuel.
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>

#include "game.h"
#include "pokey_random.h"
#include "rom.h"
#include "tv.h"

using namespace rr;

int main(int argc, char **argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s <rom> <frames> [-pal] [-game N] [-seed N]\n", argv[0]);
		return 2;
	}
	Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM\n"); return 2; }
	const long frames = std::atol(argv[2]);
	bool pal = false;
	int game = 1;
	uint32_t seed = 1;
	for (int i = 3; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-game") && i + 1 < argc) game = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) seed = uint32_t(std::atol(argv[++i]));
	}
	const TvSystem tv = tvSystem(pal);
	PokeyRandom random(seed, tv);
	Game g(rom);
	g.startAttract(pal);
	// the plane does not crash and never runs out of fuel (as rr_oracle -ghost): the collision
	// results that would end it are dropped after the display, so the game goes on for long
	g.afterDisplay = [&g] {
		g.g.hit.planeHitBank = g.g.hit.planeHitObject = g.g.hit.planeHitShell = g.g.hit.missileHitPlane = 0xFF;
		g.g.fuel = 0xFF;
	};
	long inGame = 0, attractFrames = 0, games = 0;
	bool menu = true;          // choosing the game: SELECT until it is the one asked for, then START
	int maxBridge = 0;
	for (long f = 0; f < frames; f++) {
		Input in;
		if ((g.g.attractFlags & 0x80) != 0) menu = true;   // the attract mode (after a game over too)
		if (menu) {
			attractFrames++;
			if (g.g.gameNumber != (game - 1) % 8) in.select = true;   // (held: the next game every 30 frames)
			else if (!(f & 1)) { in.start = true; menu = false; games++; }
		} else {
			inGame++;
			// steer to the middle of the water run nearest the plane, 30 lines above it
			const int px = g.g.planeX - 32 + 4;
			const int row = 0xAA - 28 - 30;
			int target = px;
			if (row >= 0 && size_t(row) < g.runLines.size()) {
				const Game::Runs &r = g.runLines[size_t(row)];
				int best = 1 << 30;
				for (int i = 0; i < r.n; i++) {
					if (r.colour[i] != 3) continue;
					const int a = r.a[i], b = r.b[i] - 1;
					const int d = px < a ? a - px : px > b ? px - b : 0;
					if (d < best) { best = d; target = (a + b) / 2; }
				}
			}
			uint8_t stick = 0x0F;
			if (target < px - 3) stick &= uint8_t(~0x04);
			else if (target > px + 3) stick &= uint8_t(~0x08);
			in.stick[0] = stick;
			in.trigger[0] = (f & 1) != 0;
		}
		if (!menu) maxBridge = std::max(maxBridge, (g.g.bridges[0] >> 4) * 1000 + (g.g.bridges[0] & 15) * 100 + (g.g.bridges[1] >> 4) * 10 + (g.g.bridges[1] & 15));
		int reads = 0;
		g.frame(in, [&] { return reads++ ? random.read(249, 13) : pal ? random.read(191, 24) : random.read(192, 13); });
		random.nextFrame();
	}
	std::printf("%ld frames: %ld in a game (%ld games started, up to bridge %d), %ld choosing the game\n", frames, inGame, games, maxBridge, attractFrames);
	return 0;
}
