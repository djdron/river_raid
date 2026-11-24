// POKEY's RANDOM register ($D20A): the top bits of a 17-bit polynomial counter that advances
// every machine cycle (1.79 MHz NTSC, 1.77 MHz PAL), so a read returns whatever state the counter is in at that
// moment. The original reads it at points whose exact cycle depends on the whole machine,
// so the port models the counter and the approximate time of each read; the values are as
// random as on the real machine, but not the same sequence as a particular emulator run.
#pragma once
#include <array>
#include <cstdint>

#include "tv.h"

namespace rr {

class PokeyRandom {
public:
	static constexpr int kCyclesPerLine = TvSystem::kCyclesPerLine;

	explicit PokeyRandom(uint32_t seed = 1, const TvSystem &tv = kNtsc)
		: frameState_(seed % kPeriod ? seed % kPeriod : 1), cyclesPerFrame_(tv.cyclesPerFrame()) {}

	// Value read on `scanline` at cycle `cycle` of the current frame: the counter's state at
	// the frame's start, a jump on to that cycle. (A read at or before this frame's last read
	// returns that read's state again: the counter does not go back.)
	uint8_t read(int scanline, int cycle) {
		const uint64_t at = uint64_t(scanline) * kCyclesPerLine + uint64_t(cycle);
		if (!readThisFrame_ || at > lastAt_) {
			lastState_ = jump(at, frameState_);
			lastAt_ = at;
			readThisFrame_ = true;
		}
		return uint8_t(lastState_ >> 9);
	}
	// Start of the next frame: the counter a whole frame on.
	void nextFrame() {
		frameState_ = jump(uint64_t(cyclesPerFrame_), frameState_);
		readThisFrame_ = false;
	}

	// The counter `steps` cycles on. Each step is linear over GF(2) (a shift and an XOR), so
	// any number of steps is too: the new state is the XOR of the images of the old state's set
	// bits. jumps()[k] holds the images of the 17 bits after 2^k steps, and a jump of n steps
	// applies those of n's set bits: at most 17 x 17 XORs, with 1.2 KB of tables made at
	// start. (Before, the counter was kept as its position in the cycle, with ~1 MB of tables.)
	static uint32_t advance(uint32_t s, uint64_t steps) {
		uint32_t n = uint32_t(steps % kPeriod);
		for (int k = 0; n; k++, n >>= 1)
			if (n & 1) s = apply(jumps()[size_t(k)], s);
		return s;
	}
	// One cycle: x^17 + x^12 + 1.
	static uint32_t step(uint32_t s) { return (s >> 1) | (((s ^ (s >> 5)) & 1) << 16); }

private:
	// The counter steps through all 2^17 - 1 non-zero states in a fixed cycle.
	static constexpr uint32_t kPeriod = 0x1FFFF;
	static constexpr int kBits = 17;
	using Map = std::array<uint32_t, kBits>;   // the image of each bit

	static uint32_t apply(const Map &m, uint32_t s) {   // s: 17 bits
		uint32_t r = 0;
		for (int j = 0; s; j++, s >>= 1)
			if (s & 1) r ^= m[size_t(j)];
		return r;
	}
	static const std::array<Map, kBits> &jumps() {
		static const std::array<Map, kBits> t = [] {
			std::array<Map, kBits> m{};
			for (int j = 0; j < kBits; j++) m[0][size_t(j)] = step(1u << j);
			for (int k = 1; k < kBits; k++)   // 2^k steps: twice 2^(k-1)
				for (int j = 0; j < kBits; j++) m[size_t(k)][size_t(j)] = apply(m[size_t(k - 1)], m[size_t(k - 1)][size_t(j)]);
			return m;
		}();
		return t;
	}

	uint32_t frameState_;           // at the start of this frame
	long cyclesPerFrame_;
	uint32_t lastState_ = 0;        // at this frame's last read
	uint64_t lastAt_ = 0;           // its cycle in the frame
	bool readThisFrame_ = false;

	// The jumps are always the same few lengths: a whole frame (nextFrame), and from a frame's
	// start to the places the game reads RANDOM at (two or three). Each is kept as a map read 4
	// bits at a time (as the ZX Spectrum ports keep their frame's map): 5 lookups and XORs.
	struct Jump {
		uint64_t steps = 0;
		uint32_t nibble[5][16] = {};
	};
	std::array<Jump, 6> cache_{};
	size_t nextJump_ = 0;

	uint32_t jump(uint64_t cycles, uint32_t s) {
		const uint64_t steps = cycles % kPeriod;
		if (!steps) return s;   // a whole number of the counter's cycles
		const Jump *j = nullptr;
		for (const Jump &c : cache_)
			if (c.steps == steps) j = &c;
		if (!j) {
			Jump &c = cache_[nextJump_];
			nextJump_ = (nextJump_ + 1) % cache_.size();
			c.steps = steps;
			for (int k = 0; k < 5; k++)   // (the fifth nibble is bit 16 alone: values 0 and 1)
				for (uint32_t v = 0; v < (k < 4 ? 16u : 2u); v++) c.nibble[k][v] = advance(v << (4 * k), steps);
			j = &c;
		}
		return j->nibble[0][s & 15] ^ j->nibble[1][(s >> 4) & 15] ^ j->nibble[2][(s >> 8) & 15] ^
			j->nibble[3][(s >> 12) & 15] ^ j->nibble[4][s >> 16];
	}
};

}  // namespace rr
