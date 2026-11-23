// The river on the screen: the lines in screen memory (newest first, the river area shows
// 0..159), the same lines without their decoration (houses, roads), which the collisions look
// at, those as runs of one colour, and where the top line is in the original's screen memory.
// The river and its objects (World) make the lines; push() adds the next one at the top.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "world.h"

namespace rr {

struct River {
	using Line = std::array<uint8_t, kLineBytes>;
	// A line as runs of one colour: pixels a..b-1 (0-191) of colour 1 bank, 2 edge, 3 water
	struct Runs {
		int n = 0;
		uint8_t a[48], b[48], colour[48];
	};
	static Runs makeRuns(const uint8_t *line);

	std::vector<Line> lines;       // newest first
	std::vector<Line> bankLines;   // the same without decoration
	std::vector<Runs> runs;        // bankLines as runs
	// $3F04/$3F05: screen memory address of the top line. Screen memory is a ring of two
	// 80-line blocks, $2000-$2EFF and $3000-$3EFF; the address moves up a line with each line.
	uint16_t top = 0x2000;
	// If set: the river's and the objects' state before each line, newest first, so the lines
	// can be made again from the oldest one (small saved states, Game::save)
	bool keepLineStates = false;
	std::vector<World::State> lineStates;

	void clear();
	// The next line goes above the others.
	void push(World &world);
	// The lines again: `count` of them from the state before the oldest.
	void rebuild(World world, const World::State &oldest, int count);
	// The runs of the river's row r (0: its top line), nullptr past the lines made
	const Runs *runsAt(int r) const { return r >= 0 && size_t(r) < runs.size() ? &runs[size_t(r)] : nullptr; }

private:
	void add(const Line &line, const Line &bank);
};

}  // namespace rr
