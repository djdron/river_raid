// Refactoring check: an older version of the port (built from a git commit, namespace rr_old)
// and the current one must behave identically. Compared after every frame or line: the state
// as the original's RAM, the screen lines, the drawn river area, the POKEY registers.
//   1. the river generator alone, 150 000 lines of every game, NTSC and PAL;
//   2. from power-on, the attract mode and random play (START, SELECT, pause included);
//   3. from states of the original's traces (oracle/out*/ram.bin), random play.
// usage: build.sh <commit>, then eq [frames] (run from the repository root)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "side.h"

static const char *kRom = "rom/River_Raid.rom";
static bool scratch(int a) { return a == 0xB1 || a == 0xB7 || a == 0xC8 || a == 0xC9 || a == 0xCA; }
static Snapshot A, B;
static long failures = 0;

static bool compare(const char *what, long step) {
	oldSnapshot(A); newSnapshot(B);
	std::string d;
	for (int a = 0; a < 0x4000; a++)
		if (!scratch(a) && A.ram[a] != B.ram[a]) { char b[64]; std::snprintf(b, 64, " ram $%04X %02X/%02X", a, A.ram[a], B.ram[a]); d += b; if (d.size() > 300) break; }
	if (A.lines != B.lines) d += " lines";
	else for (int k = 0; k < A.lines && k < 256; k++) if (std::memcmp(A.screen[k], B.screen[k], 48)) { d += " screen"; break; }
	if (std::memcmp(A.picture, B.picture, sizeof A.picture)) d += " picture";
	if (std::memcmp(A.audf, B.audf, 4) || std::memcmp(A.audc, B.audc, 4)) d += " pokey";
	if (A.busy != B.busy) d += " busy";
	if (std::memcmp(A.colours, B.colours, 5)) d += " colours";
	if (!d.empty()) { if (failures++ < 10) std::printf("%s step %ld:%s\n", what, step, d.c_str()); return false; }
	return true;
}

struct Player {
	uint32_t s;
	explicit Player(uint32_t seed) : s(seed) {}
	uint32_t r() { s = s * 22695477u + 1u; return s >> 16; }
	int hold = 0; SideInput cur{};
	SideInput next(bool allowConsole) {
		if (hold-- <= 0) {
			hold = int(r() % 40);
			cur = SideInput{};
			uint8_t st = 0x0F;
			int m = int(r() % 10);
			if (m < 3) st &= ~0x01;            // up
			else if (m < 4) st &= ~0x02;       // down
			int h = int(r() % 6);
			if (h == 0) st &= ~0x04; else if (h == 1) st &= ~0x08;
			cur.stick[0] = cur.stick[1] = st;
			cur.trigger[0] = cur.trigger[1] = (r() % 3) != 0;
			if (allowConsole) {
				int c = int(r() % 200);
				cur.start = c == 0; cur.select = c == 1; cur.option = c == 2; cur.key = c == 3;
			}
		}
		SideInput in = cur;
		if (in.start || in.select || in.option || in.key) { if (hold > 2) hold = 2; }
		return in;
	}
};

static void run(const char *what, long frames, uint32_t seed, bool console) {
	Player p(seed);
	uint32_t r1 = seed ^ 0x55AA, r2 = r1;
	for (long f = 0; f < frames; f++) {
		SideInput in = p.next(console);
		int d1 = oldFrame(in, r1), d2 = newFrame(in, r2);
		if (d1 != d2) { if (failures++ < 10) std::printf("%s frame %ld: RANDOM reads %d/%d\n", what, f, d1, d2); return; }
		if (!compare(what, f)) return;
	}
}

int main(int argc, char **argv) {
	const long frames = argc > 1 ? std::atol(argv[1]) : 20000;
	// 1. river generator alone, every game, NTSC and PAL
	for (int pal = 0; pal < 2; pal++)
		for (int gn = 1; gn <= 8; gn++) {
			oldInit(kRom, 2, nullptr, gn, pal); newInit(kRom, 2, nullptr, gn, pal);
			static uint8_t ra[0x4000], rb[0x4000];
			uint8_t la[48], lb[48];
			for (long n = 0; n < 150000; n++) {
				oldGenLine(la, ra); newGenLine(lb, rb);
				bool bad = std::memcmp(la, lb, 48) != 0;
				for (int a = 0; a < 0x4000 && !bad; a++) if (!scratch(a) && ra[a] != rb[a]) bad = true;
				if (bad) { if (failures++ < 10) std::printf("generator game %d pal %d line %ld differs\n", gn, pal, n); break; }
			}
		}
	std::printf("generator done, failures %ld\n", failures);
	// 2. from power-on attract with random play
	for (uint32_t seed = 1; seed <= 6; seed++) {
		oldInit(kRom, 0, nullptr, 0, seed & 1); newInit(kRom, 0, nullptr, 0, seed & 1);
		run("attract+play", frames, seed, true);
	}
	std::printf("play done, failures %ld\n", failures);
	// 3. from states of the original's traces, random play
	const char *traces[] = {"out_long", "out_2p", "out_b", "out_nf", "out_idle", "out_scr", "out_r", "out_g"};
	long starts = 0;
	for (const char *t : traces) {
		std::string path = std::string("oracle/") + t + "/ram.bin";
		FILE *f = std::fopen(path.c_str(), "rb");
		if (!f) continue;
		static uint8_t ram[0x4000];
		for (long fr = 300; ; fr += 397) {
			if (std::fseek(f, fr * 0x4000L, SEEK_SET) || std::fread(ram, 1, 0x4000, f) != 0x4000) break;
			oldInit(kRom, 1, ram, 0, false); newInit(kRom, 1, ram, 0, false);
			if (!compare("load", fr)) break;
			run(t, 400, uint32_t(fr), false);
			starts++;
		}
		std::fclose(f);
	}
	std::printf("trace starts %ld, failures %ld\n", starts, failures);
	return failures ? 1 : 0;
}
