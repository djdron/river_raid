// Generate the river with the C++ generator alone and save it as a picture.
//
// usage: river_map <rom> <game 1..8> <lines> <out.png> [-pal] [-flat] [-raw lines.bin]
//   The picture has the start at the bottom, like the game scrolls. Each generated line is
//   one row, drawn as the game shows it (river_draw.h): bank shades, roads at bridges and the
//   objects, in the visible 336 columns of the screen.
//   -flat draws only the bitmap with a fixed colour per 2-bit value (no road, no objects).
//   -raw writes the 48-byte lines in generation order, for comparing with the original.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "../src/river_draw.h"
#include "../src/river_gen.h"

namespace {

uint32_t crcTable[256];
void initCrc() {
	for (uint32_t n = 0; n < 256; n++) {
		uint32_t c = n;
		for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		crcTable[n] = c;
	}
}
uint32_t crc(const uint8_t *p, size_t n, uint32_t c = 0xFFFFFFFFu) {
	for (size_t i = 0; i < n; i++) c = crcTable[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c;
}
void put32(std::vector<uint8_t> &v, uint32_t x) {
	for (int i = 3; i >= 0; i--) v.push_back(uint8_t(x >> (8 * i)));
}
void chunk(FILE *f, const char *type, const std::vector<uint8_t> &data) {
	std::vector<uint8_t> v;
	put32(v, uint32_t(data.size()));
	std::vector<uint8_t> td(type, type + 4);
	td.insert(td.end(), data.begin(), data.end());
	v.insert(v.end(), td.begin(), td.end());
	put32(v, crc(td.data(), td.size()) ^ 0xFFFFFFFFu);
	std::fwrite(v.data(), 1, v.size(), f);
}
// Minimal PNG writer: 8-bit RGB, deflate "stored" blocks (no compression library needed).
bool writePng(const std::string &path, int w, int h, const std::vector<uint8_t> &rgb) {
	FILE *f = std::fopen(path.c_str(), "wb");
	if (!f) return false;
	static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
	std::fwrite(sig, 1, 8, f);
	std::vector<uint8_t> ihdr;
	put32(ihdr, uint32_t(w)); put32(ihdr, uint32_t(h));
	ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
	chunk(f, "IHDR", ihdr);
	std::vector<uint8_t> raw;
	for (int y = 0; y < h; y++) {
		raw.push_back(0);
		raw.insert(raw.end(), rgb.begin() + size_t(y) * w * 3, rgb.begin() + size_t(y + 1) * w * 3);
	}
	std::vector<uint8_t> z = {0x78, 0x01};
	uint32_t a = 1, b = 0;
	for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
	for (size_t pos = 0; pos < raw.size(); pos += 65535) {
		size_t n = raw.size() - pos < 65535 ? raw.size() - pos : 65535;
		z.push_back(pos + n == raw.size() ? 1 : 0);
		z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8));
		z.push_back(uint8_t(~n)); z.push_back(uint8_t(~n >> 8));
		z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
	}
	put32(z, (b << 16) | a);
	chunk(f, "IDAT", z);
	chunk(f, "IEND", {});
	std::fclose(f);
	return true;
}

}  // namespace

int main(int argc, char **argv) {
	if (argc < 5) {
		std::fprintf(stderr, "usage: %s <rom> <game 1..8> <lines> <out.png> [-pal] [-flat] [-raw lines.bin]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
	const int game = std::atoi(argv[2]), lines = std::atoi(argv[3]);
	bool pal = false, flat = false;
	const char *rawPath = nullptr;
	for (int i = 5; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-flat")) flat = true;
		else if (!std::strcmp(argv[i], "-raw") && i + 1 < argc) rawPath = argv[++i];
	}
	rr::RiverGen gen(rom);
	gen.startGame(game, pal);
	std::vector<uint8_t> all(size_t(lines) * rr::kLineBytes);
	const int w = rr::kVisibleLast + 1 - rr::kVisibleFirst;
	std::vector<uint8_t> rgb(size_t(w) * lines * 3);
	// flat colours: 0 background, 1 bank, 2 bank edge / island, 3 water
	static const uint8_t flatColours[4] = {0x00, 0xD2, 0x20, 0x84};
	for (int i = 0; i < lines; i++) {
		uint8_t *src = &all[size_t(i) * rr::kLineBytes];
		gen.nextLine(src);
		uint8_t screen[rr::kRowWidth];
		if (flat) {
			for (int p = 0; p < rr::kRowWidth; p++) screen[p] = flatColours[(src[p >> 3] >> (6 - 2 * ((p >> 1) & 3))) & 3];
		} else {
			rr::drawScreenLine(rom, gen.s, src, screen);
		}
		uint8_t *row = &rgb[size_t(lines - 1 - i) * w * 3];   // newest line at the top
		for (int x = 0; x < w; x++) rr::atariRgb(screen[rr::kVisibleFirst + x], row + x * 3);
	}
	if (rawPath) {
		FILE *f = std::fopen(rawPath, "wb");
		if (f) { std::fwrite(all.data(), 1, all.size(), f); std::fclose(f); }
	}
	initCrc();
	if (!writePng(argv[4], w, lines, rgb)) { std::fprintf(stderr, "cannot write %s\n", argv[4]); return 1; }
	std::printf("game %d, %d lines, level reached %d -> %s\n", game, lines, gen.s.level, argv[4]);
	return 0;
}
