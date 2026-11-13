// POKEY's RANDOM register ($D20A): the top bits of a 17-bit polynomial counter that advances
// every machine cycle (1.79 MHz NTSC, 1.77 MHz PAL), so a read returns whatever state the counter is in at that
// moment. The original reads it at points whose exact cycle depends on the whole machine,
// so the port models the counter and the approximate time of each read; the values are as
// random as on the real machine, but not the same sequence as a particular emulator run.
#pragma once
#include <cstdint>
#include <vector>

#include "tv.h"

namespace rr {

class PokeyRandom {
public:
	static constexpr int kCyclesPerLine = TvSystem::kCyclesPerLine;

	explicit PokeyRandom(uint32_t seed = 1, const TvSystem &tv = kNtsc)
		: pos_(table().position[seed % 0x1FFFF ? seed % 0x1FFFF : 1]), cyclesPerFrame_(tv.cyclesPerFrame()) {}

	// Value read on `scanline` at cycle `cycle` of the current frame.
	uint8_t read(int scanline, int cycle) {
		advanceTo(frameStart_ + uint64_t(scanline) * kCyclesPerLine + uint64_t(cycle));
		return uint8_t(table().state[pos_] >> 9);
	}
	// Start of the next frame.
	void nextFrame() { frameStart_ += uint64_t(cyclesPerFrame_); }

private:
	// The counter steps through all 2^17 - 1 non-zero states in a fixed cycle, so it is kept as
	// a position in that cycle, and jumping ahead any number of cycles is one addition.
	static constexpr uint32_t kPeriod = 0x1FFFF;
	struct Table {
		std::vector<uint32_t> state;      // state at each position of the cycle
		std::vector<uint32_t> position;   // position of each state (0 is never reached)
		Table() : state(kPeriod), position(kPeriod + 1) {
			uint32_t s = 1;
			for (uint32_t i = 0; i < kPeriod; i++) {
				state[i] = s;
				position[s] = i;
				const uint32_t bit = (s ^ (s >> 5)) & 1;   // x^17 + x^12 + 1, one step per cycle
				s = (s >> 1) | (bit << 16);
			}
		}
	};
	static const Table &table() {
		static const Table t;
		return t;
	}

	uint32_t pos_;
	long cyclesPerFrame_;
	uint64_t cycle_ = 0, frameStart_ = 0;

	void advanceTo(uint64_t cycle) {
		if (cycle <= cycle_) return;
		pos_ = uint32_t((pos_ + (cycle - cycle_) % kPeriod) % kPeriod);
		cycle_ = cycle;
	}
};

}  // namespace rr
