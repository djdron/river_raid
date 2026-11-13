// POKEY sound: the four channels of the Atari's sound chip, generated cycle by cycle.
//
// Each channel divides a base clock (64 kHz by default) by AUDF+1; every time its counter runs
// out it produces a pulse. AUDC selects what a pulse does to the channel's output bit
// (the "distortion") and the volume:
//   bit 7 = 0: the pulse only counts when the 5-bit polynomial counter's output is 1
//   bit 5 = 1: the output toggles (a square wave)
//   bit 5 = 0: the output takes the 4-bit (bit 6 = 1) or 17-bit (bit 6 = 0) polynomial's bit
//   bit 4 = 1: volume only: the output is the volume, whatever the counter does
//   bits 3-0: volume
// The polynomial counters are shift registers advancing every machine cycle; they make the
// noise. Only their state at the 64 kHz ticks matters, so the model keeps each counter as a
// position in its (precomputed) sequence and moves it on by whole stretches of cycles. River Raid leaves AUDCTL at 0 (64 kHz clock, 17-bit noise, no joined channels or
// filters), which is what this model covers.
#pragma once
#include <cstdint>
#include <vector>

#include "tv.h"

namespace rr {

class PokeySound {
public:
	// sampleRate: samples per second; clock: machine cycles per second (NTSC or PAL)
	explicit PokeySound(int sampleRate = 48000, double clock = kNtsc.clock);
	double clock() const { return clock_; }
	// Samples per second of machine time; a little more or less than the output's rate makes
	// the sound keep pace when the game runs a little faster or slower than the Atari.
	void setSampleRate(double rate) { sampleRate_ = rate; }

	void write(int reg, uint8_t value);           // $D200-$D207 (AUDF1, AUDC1, ... AUDC4)
	// Runs `cycles` machine cycles and appends the resulting samples to `out`.
	void run(long cycles, std::vector<int16_t> &out);

private:
	double sampleRate_;
	double clock_;
	uint8_t audf_[4] = {}, audc_[4] = {};
	int counter_[4] = {};
	uint8_t output_[4] = {};
	int tick_ = 0;                                // cycles to the next 64 kHz tick
	uint32_t poly4Pos_ = 0, poly5Pos_ = 0, poly17Pos_ = 0;   // steps since 0x0F, 0x1F, 0x1FFFF
	int level_ = 0;                               // output level of the four channels, 0..60
	double samplePos_ = 0;                        // in cycles, towards the next sample
	long sum_ = 0, sumCycles_ = 0;
	double dcLevel_ = 0;

	int level() const;
	void tick();                                  // the 64 kHz clock: the channels' counters
	void count(long cycles, std::vector<int16_t> &out);   // cycles at a constant level
};

}  // namespace rr
