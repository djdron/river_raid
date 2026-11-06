/*
 * rr_oracle - headless reference runner for River Raid (Atari 8-bit) on libatari800.
 *
 * Runs the original cartridge frame by frame with scripted input and records:
 *   <out>/ram.bin     16 KB ($0000-$3FFF) snapshot after every frame (frame N at offset N*16384)
 *   <out>/pokey.log   every POKEY register write: frame, scanline, cycle-in-line, reg, value
 *   <out>/random.log  every read of POKEY RANDOM: frame, scanline, cycle-in-line, value
 *   <out>/frames.csv  per-frame summary: applied input + CPU registers
 *   <out>/inputs.txt  applied input in script format (replay without -bot gives the same trace)
 *   <out>/fNNNNN.ppm  screenshots for frames listed with -shot
 *
 * Usage:
 *   rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot] [-shot N,N,...]
 *
 * -bot: after the script's frames, an autopilot (bot_decide) drives the plane.
 * -bot-nofire: same autopilot without shooting or dodging (flies over fuel depots).
 *
 * -seed sets POKEY's random counter at power-on (atari800 otherwise seeds it from the
 * wall clock, which makes runs non-reproducible). Default 0.
 *
 * Script format (one line per segment, '#' starts a comment):
 *   <count> <joy> <fire> [start]
 *   joy: bits 1=up 2=down 4=left 8=right (0 = centered); fire/start: 0 or 1
 *   The last line repeats until <frames> is reached.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libatari800.h"

/* internal atari800 symbols (available because we link the static library) */
extern int ANTIC_ypos;
extern int ANTIC_xpos;
extern int Colours_table[256];
void __real_POKEY_PutByte(UWORD addr, UBYTE byte);
UBYTE __real_POKEY_GetByte(UWORD addr, int no_side_effects);
void POKEY_SetRandomCounter(ULONG value);

static FILE *pokey_log, *random_log;
static int cur_frame;

/* Linked with -Wl,--wrap=POKEY_PutByte: every POKEY write goes through here. */
void __wrap_POKEY_PutByte(UWORD addr, UBYTE byte)
{
	if (pokey_log)
		fprintf(pokey_log, "%d %d %d %X %02X\n", cur_frame, ANTIC_ypos, ANTIC_xpos, addr & 0x0f, byte);
	__real_POKEY_PutByte(addr, byte);
}

/* Linked with -Wl,--wrap=POKEY_GetByte: log every read of RANDOM ($D20A). */
UBYTE __wrap_POKEY_GetByte(UWORD addr, int no_side_effects)
{
	UBYTE v = __real_POKEY_GetByte(addr, no_side_effects);
	if (random_log && !no_side_effects && (addr & 0x0f) == 0x0a)
		fprintf(random_log, "%d %d %d %02X\n", cur_frame, ANTIC_ypos, ANTIC_xpos, v);
	return v;
}

typedef struct { int count, joy, fire, start; } seg_t;

static seg_t *load_script(const char *path, int *n)
{
	FILE *f = fopen(path, "r");
	char line[256];
	seg_t *s = NULL;
	int cap = 0;
	*n = 0;
	if (!f) { perror(path); exit(1); }
	while (fgets(line, sizeof line, f)) {
		seg_t t = {0, 0, 0, 0};
		char *c = strchr(line, '#');
		if (c) *c = 0;
		if (sscanf(line, "%d %d %d %d", &t.count, &t.joy, &t.fire, &t.start) < 3)
			continue;
		if (*n == cap) { cap = cap ? cap * 2 : 16; s = realloc(s, cap * sizeof *s); }
		s[(*n)++] = t;
	}
	fclose(f);
	if (*n == 0) { fprintf(stderr, "empty script\n"); exit(1); }
	return s;
}

static void save_ppm(const char *dir, int frame)
{
	char path[1024];
	const UBYTE *scr = libatari800_get_screen_ptr();
	FILE *f;
	int i;
	snprintf(path, sizeof path, "%s/f%05d.ppm", dir, frame);
	f = fopen(path, "wb");
	if (!f) { perror(path); return; }
	fprintf(f, "P6\n384 240\n255\n");
	for (i = 0; i < 384 * 240; i++) {
		int c = Colours_table[scr[i]];
		fputc((c >> 16) & 0xff, f); fputc((c >> 8) & 0xff, f); fputc(c & 0xff, f);
	}
	fclose(f);
}

/* ---- simple autopilot used to produce long reference runs ----
 * Reads the last rendered screen (384x240 palette indices) and RAM.
 * Verified facts it relies on: $57 = plane X (screen centre ~ 2*X-55),
 * plane occupies screen rows 162..175, $76 != 0 while exploding/respawning.
 * Water is any pixel whose RGB is the river blue; everything else counts as obstacle,
 * except objects on the water, which the always-on fire is expected to clear. */
#define WATER_RGB 0x2C2492
#define PF_LEFT   48
#define PF_RIGHT  336

static int is_water(int pix)
{
	return Colours_table[pix] == WATER_RGB;
}

/* water run [l,r] on row y that is closest to x; returns 0 if row has no water */
static int water_run(const UBYTE *scr, int y, int x, int *l, int *r)
{
	int best = -1, bl = 0, br = 0, i = PF_LEFT;
	while (i < PF_RIGHT) {
		int a, d;
		if (!is_water(scr[y * 384 + i])) { i++; continue; }
		a = i;
		while (i < PF_RIGHT && is_water(scr[y * 384 + i])) i++;
		d = x < a ? a - x : x >= i ? x - (i - 1) : 0;
		if (best < 0 || d < best) { best = d; bl = a; br = i - 1; }
	}
	if (best < 0) return 0;
	*l = bl; *r = br;
	return 1;
}

static int bot_fire = 1;   /* -bot-nofire: never shoot, no dodging (used to fly over fuel depots) */

static void bot_decide(input_template_t *in, const UBYTE *ram)
{
	const UBYTE *scr = libatari800_get_screen_ptr();
	int px = 2 * ram[0x57] - 55;
	int y, target = px, have = 0, l, r, lo = PF_LEFT, hi = PF_RIGHT;

	in->joy0 = 0; in->trig0 = (UBYTE)bot_fire; in->start = 0;
	/* $76 != 0: exploding, or waiting at the bridge for the player to move (any stick input launches) */
	if (ram[0x76] != 0) { in->joy0 = 1; in->trig0 = 0; return; }

	/* follow the water corridor from just above the plane to ~70 rows ahead */
	for (y = 158; y >= 90; y -= 4) {
		if (!water_run(scr, y, have ? (lo + hi) / 2 : px, &l, &r)) continue;
		if (!have) { lo = l; hi = r; have = 1; }
		else {
			/* corridor narrows: keep intersection when it overlaps */
			if (l > lo && l < hi) lo = l;
			if (r < hi && r > lo) hi = r;
		}
		if (y == 158 && (r - l) < 24) break;          /* tight spot right ahead: centre on it */
	}
	if (have) target = (lo + hi) / 2;

	/* dodge: objects (not water/bank/plane/shot) close to the plane's rows push the target away */
	if (bot_fire) {
		int x, dl = 0, dr = 0;
		for (y = 128; y <= 176; y += 2)
			for (x = px - 36; x <= px + 36; x++) {
				int c;
				if (x < PF_LEFT || x >= PF_RIGHT) continue;
				if (y >= 160 && x >= px - 9 && x <= px + 9) continue;   /* the plane itself */
				c = Colours_table[scr[y * 384 + x]];
				if (c == WATER_RGB || c == 0x003B00 || c == 0x1C0800 || c == 0xD3E574) continue;
				if (x < px) dl++; else dr++;
			}
		if (dl > 6 && dl >= dr) target = px + 24;
		else if (dr > 6) target = px - 24;
		if (have) {           /* never steer outside the water corridor */
			if (target < lo + 8) target = lo + 8;
			if (target > hi - 8) target = hi - 8;
		}
	}

	if (target < px - 3) in->joy0 = 4;
	else if (target > px + 3) in->joy0 = 8;
}

int main(int argc, char **argv)
{
	const char *rom, *script, *out;
	int frames, nseg, seg = 0, left, i, pal = 0;
	int shots[256], nshots = 0, bot = 0, script_frames = 0;
	unsigned long seed = 0;
	FILE *rec;
	int rec_joy = -1, rec_fire = 0, rec_start = 0, rec_count = 0;
	seg_t *s;
	char path[1024];
	FILE *ram, *csv;
	input_template_t in;
	emulator_state_t st;

	if (argc < 5) {
		fprintf(stderr, "usage: %s <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-shot N,N,...]\n", argv[0]);
		return 1;
	}
	rom = argv[1]; script = argv[2]; frames = atoi(argv[3]); out = argv[4];
	for (i = 5; i < argc; i++) {
		if (!strcmp(argv[i], "-pal")) pal = 1;
		else if (!strcmp(argv[i], "-seed") && i + 1 < argc) seed = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "-bot")) bot = 1;
		else if (!strcmp(argv[i], "-bot-nofire")) { bot = 1; bot_fire = 0; }
		else if (!strcmp(argv[i], "-shot") && i + 1 < argc) {
			char *p = argv[++i];
			while (*p && nshots < 256) { shots[nshots++] = atoi(p); p = strchr(p, ','); if (!p) break; p++; }
		}
	}
	s = load_script(script, &nseg);
	left = s[0].count;
	for (i = 0; i < nseg; i++) script_frames += s[i].count;

	{
		char *eargv[] = { "atari800", "-atari", "-800-rev", "altirra", "-nobasic",
		                  pal ? "-pal" : "-ntsc", "-cart-type", "1", "-cart", (char *)rom, NULL };
		if (!libatari800_init(10, eargv)) {
			fprintf(stderr, "init failed: %s\n", libatari800_error_message());
			return 1;
		}
		POKEY_SetRandomCounter((ULONG)seed);
	}

	snprintf(path, sizeof path, "%s/ram.bin", out);   ram = fopen(path, "wb");
	snprintf(path, sizeof path, "%s/pokey.log", out); pokey_log = fopen(path, "w");
	snprintf(path, sizeof path, "%s/frames.csv", out); csv = fopen(path, "w");
	snprintf(path, sizeof path, "%s/random.log", out); random_log = fopen(path, "w");
	snprintf(path, sizeof path, "%s/inputs.txt", out); rec = fopen(path, "w");
	if (!ram || !pokey_log || !csv || !random_log || !rec) { perror(out); return 1; }
	fprintf(rec, "# input actually applied, replayable as a script: <count> <joy> <fire> <start>\n");
	fprintf(pokey_log, "# frame scanline xpos reg value\n");
	fprintf(random_log, "# frame scanline xpos value\n");
	fprintf(csv, "frame,joy,fire,start,pc,a,x,y,s,p\n");

	libatari800_clear_input_array(&in);
	for (cur_frame = 0; cur_frame < frames; cur_frame++) {
		if (bot && cur_frame >= script_frames) {
			bot_decide(&in, libatari800_get_main_memory_ptr());
		} else {
			const seg_t *t = &s[seg];
			in.joy0 = (UBYTE)t->joy;
			in.trig0 = (UBYTE)t->fire;
			in.start = (UBYTE)t->start;
		}
		if (in.joy0 == rec_joy && in.trig0 == rec_fire && in.start == rec_start) rec_count++;
		else {
			if (rec_count) fprintf(rec, "%d %d %d %d\n", rec_count, rec_joy, rec_fire, rec_start);
			rec_joy = in.joy0; rec_fire = in.trig0; rec_start = in.start; rec_count = 1;
		}
		libatari800_next_frame(&in);

		fwrite(libatari800_get_main_memory_ptr(), 1, 0x4000, ram);
		libatari800_get_current_state(&st);
		{
			cpu_state_t *cpu = (cpu_state_t *)&st.state[st.tags.cpu];
			pc_state_t *pc = (pc_state_t *)&st.state[st.tags.pc];
			fprintf(csv, "%d,%d,%d,%d,%04X,%02X,%02X,%02X,%02X,%02X\n", cur_frame, in.joy0, in.trig0, in.start,
			        pc->PC, cpu->A, cpu->X, cpu->Y, cpu->S, cpu->P);
		}
		for (i = 0; i < nshots; i++)
			if (shots[i] == cur_frame) save_ppm(out, cur_frame);

		if (--left <= 0 && seg + 1 < nseg) left = s[++seg].count;
	}
	if (rec_count) fprintf(rec, "%d %d %d %d\n", rec_count, rec_joy, rec_fire, rec_start);
	fclose(rec);
	fclose(ram); fclose(pokey_log); fclose(csv); fclose(random_log);
	pokey_log = random_log = NULL;
	libatari800_exit();
	return 0;
}
