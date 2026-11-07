// POKEY's RANDOM register ($D20A): the top bits of a 17-bit polynomial counter that advances
// every machine cycle (1.79 MHz), so a read returns whatever state the counter is in at that
// moment. The original reads it at points whose exact cycle depends on the whole machine,
// so the port models the counter and the approximate time of each read; the values are as
// random as on the real machine, but not the same sequence as a particular emulator run.
#pragma once
#include <cstdint>

namespace rr {

class PokeyRandom {
public:
	static constexpr int kCyclesPerLine = 114;
	static constexpr int kLinesPerFrame = 262;   // NTSC

	explicit PokeyRandom(uint32_t seed = 1) : state_(seed % 0x1FFFF ? seed % 0x1FFFF : 1) {}

	// Value read on `scanline` at cycle `cycle` of the current frame.
	uint8_t read(int scanline, int cycle) {
		advanceTo(frameStart_ + uint64_t(scanline) * kCyclesPerLine + uint64_t(cycle));
		return uint8_t(state_ >> 9);
	}
	// Start of the next frame.
	void nextFrame() { frameStart_ += uint64_t(kLinesPerFrame) * kCyclesPerLine; }

private:
	uint32_t state_;           // 17 bits
	uint64_t cycle_ = 0, frameStart_ = 0;

	void advanceTo(uint64_t cycle) {
		for (; cycle_ < cycle; cycle_++) {
			const uint32_t bit = (state_ ^ (state_ >> 5)) & 1;   // x^17 + x^12 + 1
			state_ = (state_ >> 1) | (bit << 16);
		}
	}
};

}  // namespace rr
