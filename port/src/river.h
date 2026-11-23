// The river on the screen: the lines in screen memory (newest first, the river area shows
// 0..159), the same lines without their decoration (houses, roads), which the collisions look
// at, those as runs of one colour, and where the top line is in the original's screen memory.
// The river and its objects (World) make the lines; push() adds the next one at the top.
#pragma once
#include <array>
#include <cstdint>
#include <vector>

#include "ram_map.h"
#include "world.h"

namespace rr {

// The newest N items, newest first ([0]), in a ring: a new one goes in without moving the
// others, and past N the oldest goes. The storage (N items) is made by the first push and kept
// by clear().
template <class T, size_t N> class Recent {
	static_assert((N & (N - 1)) == 0, "a power of two");

public:
	size_t size() const { return size_; }
	bool empty() const { return size_ == 0; }
	const T &operator[](size_t i) const { return items_[(head_ + i) & (N - 1)]; }
	const T &front() const { return (*this)[0]; }
	const T &back() const { return (*this)[size_ - 1]; }
	void clear() { head_ = size_ = 0; }
	void push(const T &item) {
		if (items_.empty()) items_.resize(N);
		head_ = (head_ + N - 1) & (N - 1);
		items_[head_] = item;
		if (size_ < N) size_++;
	}

private:
	std::vector<T> items_;
	size_t head_ = 0, size_ = 0;
};

struct River {
	using Line = std::array<uint8_t, kLineBytes>;
	// A line as runs of one colour: pixels a..b-1 (0-191) of colour 1 bank, 2 edge, 3 water
	struct Runs {
		int n = 0;
		uint8_t a[48], b[48], colour[48];
	};
	static Runs makeRuns(const uint8_t *line);

	static constexpr size_t kKept = 256;   // lines kept: screen memory (2 x 80 lines) and more
	Recent<Line, kKept> lines;       // newest first
	Recent<Line, kKept> bankLines;   // the same without decoration
	Recent<Runs, kKept> runs;        // bankLines as runs
	// $3F04/$3F05: screen memory address of the top line. Screen memory is a ring of two
	// 80-line blocks, $2000-$2EFF and $3000-$3EFF; the address moves up a line with each line.
	uint16_t top = 0x2000;
	// If set: the river's and the objects' state before each line, newest first, so the lines
	// can be made again from the oldest one (small saved states, Game::save)
	bool keepLineStates = false;
	Recent<World::State, kKept> lineStates;

	void clear();
	// The next line goes above the others.
	void push(World &world);
	// The lines again: `count` of them from the state before the oldest.
	void rebuild(World world, const World::State &oldest, int count);
	// The runs of the river's row r (0: its top line), nullptr past the lines made
	const Runs *runsAt(int r) const { return r >= 0 && size_t(r) < runs.size() ? &runs[size_t(r)] : nullptr; }

	// The original's screen in its RAM: where the top line is, and the lines its display list at
	// $3F00 shows (160 of them, from the top)
	void mapRam(RamMap &m) { m.word("screenTop", 0x3F04, 0x3F05, top); }
	void linesFromRam(const uint8_t *ram);
	// The first of those lines that differs from the river's, -1 if none
	int lineDifferingFromRam(const uint8_t *ram) const;

private:
	void add(const Line &line, const Line &bank);
};

}  // namespace rr
