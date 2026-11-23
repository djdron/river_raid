// Writes one line per frame with what the game is doing, in names of the game rather than of
// the original's RAM: the plane, its bullet, the tank, its shell, the helicopter's missile, the
// objects in their slots, score, bridges, lives, the river generator, the sound registers and a
// hash of the picture (river and panel). Two versions of the port that play the same give the
// same lines, however they keep their state: tools/trace/compare.sh runs this on two git refs
// and compares (the object-oriented rewrite against dli_removed, docs/OOP.md).
//
// usage: game_trace <rom> <frames> [-pal] [-game N] [-seed N] [-bridge N] [-wild] [-ghost] [-attract]
//   The autopilot of display_bench: in the attract mode it chooses game N with SELECT and
//   starts it; in a game it steers to the middle of the water ahead and keeps firing (-wild:
//   at random, over the banks too). Without -ghost the plane crashes and runs out of fuel as
//   in a game, so lives, respawns and game overs are in the run; with -ghost it never does.
//   -attract: no game is started, the attract mode runs on (SELECT still chooses game N).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "game.h"
#include "os_font.h"
#include "pokey_random.h"
#include "rom.h"
#include "status_bar.h"
#include "tv.h"

using namespace rr;

namespace {

uint32_t fnv(const void *p, size_t n, uint32_t h = 2166136261u) {
	const uint8_t *b = static_cast<const uint8_t *>(p);
	for (size_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
	return h;
}

std::string hex(const uint8_t *b, int n) {
	std::string s;
	char t[4];
	for (int i = 0; i < n; i++) { std::snprintf(t, sizeof t, "%02X", b[i]); s += t; }
	return s;
}

}  // namespace

int main(int argc, char **argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s <rom> <frames> [-pal] [-game N] [-seed N] [-bridge N] [-wild] [-ghost] [-attract]\n", argv[0]);
		return 2;
	}
	Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM\n"); return 2; }
	const long frames = std::atol(argv[2]);
	bool pal = false, wild = false, ghost = false, attract = false;
	int game = 1, bridge = 0;
	uint32_t seed = 1;
	for (int i = 3; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-game") && i + 1 < argc) game = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) seed = uint32_t(std::atol(argv[++i]));
		else if (!std::strcmp(argv[i], "-bridge") && i + 1 < argc) bridge = std::atoi(argv[++i]);
		else if (!std::strcmp(argv[i], "-wild")) wild = true;
		else if (!std::strcmp(argv[i], "-ghost")) ghost = true;
		else if (!std::strcmp(argv[i], "-attract")) attract = true;
	}
	const TvSystem tv = tvSystem(pal);
	PokeyRandom random(seed, tv);
	uint8_t font[kOsFontBytes];
	builtinOsFont(font);
	const Charsets cs = buildCharsets(rom, font);
	static uint8_t screen[kScreenHeight][kScreenWidth];

	Game g(rom);
	if (bridge) g.gen.setRiver(0, bridge);
	g.startAttract(pal);
	g.afterDisplay = [&] {
		if (ghost) {
			g.g.hit.planeHitBank = g.g.hit.planeHitObject = g.g.hit.planeHitShell = g.g.hit.missileHitPlane = 0xFF;
			g.g.fuel = 0xFF;
		}
		std::memset(screen, 0, sizeof screen);
		g.drawRiver(screen);
	};
	bool menu = true;
	for (long f = 0; f < frames; f++) {
		Input in;
		if ((g.g.attractFlags & 0x80) != 0) menu = true;
		if (menu) {
			if (g.g.gameNumber != (game - 1) % 8) in.select = true;
			else if (!attract && !(f & 1)) { in.start = true; menu = false; }
		} else {
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
			if (wild) {
				uint32_t h = uint32_t(f >> 4) * 2654435761u ^ seed;
				h ^= h >> 15;
				target = px + int(h % 3) * 40 - 40;
			}
			uint8_t stick = 0x0F;
			if (target < px - 3) stick &= uint8_t(~0x04);
			else if (target > px + 3) stick &= uint8_t(~0x08);
			in.stick[0] = stick;
			in.trigger[0] = (f & 1) != 0;
		}
		int reads = 0;
		g.frame(in, [&] { return reads++ ? random.read(249, 13) : pal ? random.read(191, 24) : random.read(192, 13); });
		random.nextFrame();
		const GameState &s = g.g;
		const RiverState &r = g.gen.s;
		if (s.idleTime & 0x80) std::memset(screen, 0, sizeof screen);
		else drawStatusBar(rom, cs, s.panel, s.fuel, s.scroller, s.logoShown, screen, pal);

		std::printf("%ld fc%02X at%02X/%02X ps%02X pz%02X game%u pl%u busy%d", f, s.frameCount, s.attractFlags,
			s.attractTimer, s.pauseState, s.planeState, s.gameNumber, s.player, g.busy() ? 1 : 0);
		std::printf(" | plane %02X.%02X a%02X f%u sp%02X.%02X fuel%02X.%02X bul%02X", s.planeX, s.planeXFrac,
			s.planeAccel, s.planeFrame, s.speed, s.scrollFrac, s.fuel, s.fuelFrac, s.bulletY);
		std::printf(" | tank %02X %02X,%02X fire%02X shell t%02X f%u %02X.%02X,%02X v%02X.%02X | missile %02X,%02X st%02X",
			r.tankKind, r.tankX, r.tankY, r.tankFires, r.shellTimer, s.shellFrame, s.shellX, s.shellXFrac, r.shellY,
			r.shellSpeed, r.shellSpeedFrac, r.missileX, r.missileY, s.missileStep);
		std::printf(" | score %s br %s lives %u other %s/%s/%u hi %s", hex(s.score, 3).c_str(), hex(s.bridges, 2).c_str(),
			s.lives, hex(s.other.score, 3).c_str(), hex(s.other.bridges, 2).c_str(), s.other.lives, hex(s.highScore, 16).c_str());
		std::printf(" | slots %u..", r.firstSlot);
		for (int i = r.firstSlot; i < kSlots; i++) {
			const Slot &o = r.slot[size_t(i)];
			std::printf(" %02X/%02X %02X,%02X %02X s%u", o.kind, o.type, o.x, o.y, o.flags, o.size);
		}
		std::printf(" ent%02X/%u shooter%02X par%u", r.enteringSlot, r.enteringLines, r.shooterSlot, r.slotParity);
		std::printf(" | river lv%02X lfsr%02X%02X%02X bank%02X w%02X isl%02X cp%s", r.level, r.rndA, r.rndB, r.rndC,
			r.bank, r.waterWidth, r.islandWidth, hex(r.checkpoint, 4).c_str());
		std::printf(" | snd %s %s ev%u/%02X | hit %02X%02X%02X%02X%02X%02X%02X%02X%02X",
			hex(s.pokey.audf, 4).c_str(), hex(s.pokey.audc, 4).c_str(), s.eventSound, s.eventSoundTime, s.hit.bulletHitBank,
			s.hit.planeHitBank, s.hit.bulletHitObject, s.hit.planeHitObject, s.hit.planeHitShell, s.hit.shellHitObject,
			s.hit.tankAtEdge, s.hit.missileHitPlane, s.hit.missileHitBank);
		std::printf(" | pic %08X\n", fnv(screen, sizeof screen));
	}
	return 0;
}
