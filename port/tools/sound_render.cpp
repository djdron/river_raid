// Render a POKEY write log to a WAV file with PokeySound, to compare with the emulator's sound.
//
// usage: sound_render <pokey.log> <out.wav> [rate]
//   pokey.log from rr_oracle: every POKEY write with its frame, scanline and cycle.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "../src/pokey_sound.h"

int main(int argc, char **argv) {
	if (argc < 3) {
		std::fprintf(stderr, "usage: %s <pokey.log> <out.wav> [rate]\n", argv[0]);
		return 2;
	}
	const int rate = argc > 3 ? std::atoi(argv[3]) : 44100;
	FILE *in = std::fopen(argv[1], "r");
	if (!in) { std::fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
	rr::PokeySound pokey(rate);
	std::vector<int16_t> samples;
	long now = 0;
	char line[128];
	while (std::fgets(line, sizeof line, in)) {
		long f; int sc, xp; unsigned reg, v;
		if (line[0] == '#' || std::sscanf(line, "%ld %d %d %x %x", &f, &sc, &xp, &reg, &v) != 5) continue;
		const long t = f * 262L * 114 + sc * 114L + xp;
		if (t > now) { pokey.run(t - now, samples); now = t; }
		pokey.write(int(reg), uint8_t(v));
	}
	FILE *out = std::fopen(argv[2], "wb");
	if (!out) return 1;
	auto u32 = [&](uint32_t x) { std::fwrite(&x, 4, 1, out); };
	auto u16 = [&](uint16_t x) { std::fwrite(&x, 2, 1, out); };
	const uint32_t bytes = uint32_t(samples.size() * 2);
	std::fwrite("RIFF", 1, 4, out); u32(36 + bytes); std::fwrite("WAVEfmt ", 1, 8, out);
	u32(16); u16(1); u16(1); u32(uint32_t(rate)); u32(uint32_t(rate * 2)); u16(2); u16(16);
	std::fwrite("data", 1, 4, out); u32(bytes);
	std::fwrite(samples.data(), 2, samples.size(), out);
	std::fclose(out);
	std::printf("%zu samples\n", samples.size());
	return 0;
}
