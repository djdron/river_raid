// Dump the generated river as text, for the pictures in docs/HOW_IT_WORKS.md
// (tools/doc_figures.py draws them).
//
// usage: river_dump <rom> <game 1..8> <lines> <out.txt> [-seed N]
//   One line per generated line, in generation order:
//     L <level> <sections left> <blocks left> <line in block> <192 digits 0..3>
//   the digits being the 2-bit colour of each dot (0 black, 1 bank, 2 bank edge, 3 water),
//   and after a line where an object was placed:
//     O <type> <dot of the object's middle>
//   with type as in rom.h (object::kShip...).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../src/river_gen.h"

int main(int argc, char **argv) {
	if (argc < 5) {
		std::fprintf(stderr, "usage: %s <rom> <game 1..8> <lines> <out.txt> [-seed N]\n", argv[0]);
		return 2;
	}
	rr::Rom rom;
	if (!rom.load(argv[1])) { std::fprintf(stderr, "cannot read ROM %s\n", argv[1]); return 2; }
	const int game = std::atoi(argv[2]), lines = std::atoi(argv[3]);
	FILE *f = std::fopen(argv[4], "w");
	if (!f) { std::fprintf(stderr, "cannot write %s\n", argv[4]); return 1; }
	rr::RiverGen gen(rom);
	if (argc > 6 && !std::strcmp(argv[5], "-seed")) gen.setRiver(uint32_t(std::strtoul(argv[6], nullptr, 0)), 0);
	gen.startGame(game, false);
	for (int i = 0; i < lines; i++) {
		const uint8_t enteringLines = gen.s.enteringLines;
		uint8_t line[rr::kLineBytes];
		gen.nextLine(line);
		std::fprintf(f, "L %d %d %d %d ", gen.s.level, gen.s.sectionsLeft, gen.s.blocksLeft, gen.s.lineInBlock);
		for (int p = 0; p < rr::kLineBytes * 4; p++) std::fputc('0' + ((line[p >> 2] >> (6 - 2 * (p & 3))) & 3), f);
		std::fputc('\n', f);
		if (gen.s.enteringLines > enteringLines && gen.s.enteringSlot < rr::kSlots) {
			// a new object (or nothing) entered at the top: the count of lines until it is fully
			// drawn in starts again (every 16 lines, or 32 after a tall one). Slot positions are
			// sprite positions: the screen's left edge is 32, a sprite is 8 colour clocks wide
			// times its size.
			const rr::Slot &sl = gen.s.slot[gen.s.enteringSlot];
			const int width = (sl.size & 3) == 3 ? 4 : (sl.size & 3) == 1 ? 2 : 1;
			std::fprintf(f, "O %d %d\n", sl.type, sl.x - 32 + 4 * width);
		}
	}
	std::fclose(f);
	return 0;
}
