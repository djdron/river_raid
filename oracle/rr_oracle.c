/*
 * rr_oracle - headless reference runner for River Raid (Atari 8-bit) on libatari800.
 *
 * Runs the original cartridge frame by frame with scripted input and records:
 *   <out>/ram.bin     16 KB ($0000-$3FFF) snapshot after every frame (frame N at offset N*16384)
 *   <out>/pokey.log   every POKEY register write: frame, scanline, cycle-in-line, reg, value
 *   <out>/random.log  every read of POKEY RANDOM: frame, scanline, cycle-in-line, value
 *   <out>/frames.csv  per-frame summary: input + CPU registers
 *   <out>/fNNNNN.ppm  screenshots for frames listed with -shot
 *
 * Usage:
 *   rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-shot N,N,...]
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

int main(int argc, char **argv)
{
	const char *rom, *script, *out;
	int frames, nseg, seg = 0, left, i, pal = 0;
	int shots[256], nshots = 0;
	unsigned long seed = 0;
	seg_t *s;
	char path[1024];
	FILE *ram, *csv;
	input_template_t in;
	emulator_state_t st;

	if (argc < 5) {
		fprintf(stderr, "usage: %s <rom> <script> <frames> <outdir> [-pal] [-shot N,N,...]\n", argv[0]);
		return 1;
	}
	rom = argv[1]; script = argv[2]; frames = atoi(argv[3]); out = argv[4];
	for (i = 5; i < argc; i++) {
		if (!strcmp(argv[i], "-pal")) pal = 1;
		else if (!strcmp(argv[i], "-seed") && i + 1 < argc) seed = strtoul(argv[++i], NULL, 0);
		else if (!strcmp(argv[i], "-shot") && i + 1 < argc) {
			char *p = argv[++i];
			while (*p && nshots < 256) { shots[nshots++] = atoi(p); p = strchr(p, ','); if (!p) break; p++; }
		}
	}
	s = load_script(script, &nseg);
	left = s[0].count;

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
	if (!ram || !pokey_log || !csv || !random_log) { perror(out); return 1; }
	fprintf(pokey_log, "# frame scanline xpos reg value\n");
	fprintf(random_log, "# frame scanline xpos value\n");
	fprintf(csv, "frame,joy,fire,start,pc,a,x,y,s,p\n");

	libatari800_clear_input_array(&in);
	for (cur_frame = 0; cur_frame < frames; cur_frame++) {
		const seg_t *t = &s[seg];
		in.joy0 = (UBYTE)t->joy;
		in.trig0 = (UBYTE)t->fire;
		in.start = (UBYTE)t->start;
		libatari800_next_frame(&in);

		fwrite(libatari800_get_main_memory_ptr(), 1, 0x4000, ram);
		libatari800_get_current_state(&st);
		{
			cpu_state_t *cpu = (cpu_state_t *)&st.state[st.tags.cpu];
			pc_state_t *pc = (pc_state_t *)&st.state[st.tags.pc];
			fprintf(csv, "%d,%d,%d,%d,%04X,%02X,%02X,%02X,%02X,%02X\n", cur_frame, t->joy, t->fire, t->start,
			        pc->PC, cpu->A, cpu->X, cpu->Y, cpu->S, cpu->P);
		}
		for (i = 0; i < nshots; i++)
			if (shots[i] == cur_frame) save_ppm(out, cur_frame);

		if (--left <= 0 && seg + 1 < nseg) left = s[++seg].count;
	}
	fclose(ram); fclose(pokey_log); fclose(csv); fclose(random_log);
	pokey_log = random_log = NULL;
	libatari800_exit();
	return 0;
}
