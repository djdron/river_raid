#include "pokey_sound.h"

#include <algorithm>
#include <cmath>

namespace rr {

namespace {

constexpr int kCyclesPer64k = 28;   // 64 kHz base clock

// The output bit of a polynomial counter (a shift register with feedback) cycle by cycle, from
// the given start until it repeats.
std::vector<uint8_t> polySequence(uint32_t start, uint32_t mask, int tap1, int tap2) {
	std::vector<uint8_t> bits;   // bit 0 after 0, 1, 2... steps (the only bit used)
	uint32_t p = start;
	do {
		bits.push_back(uint8_t(p & 1));
		p = ((p << 1) | (((p >> tap1) ^ (p >> tap2)) & 1)) & mask;
	} while (p != start);
	return bits;
}
const std::vector<uint8_t> &poly4() { static const auto s = polySequence(0x0F, 0x0F, 3, 2); return s; }
const std::vector<uint8_t> &poly5() { static const auto s = polySequence(0x1F, 0x1F, 4, 2); return s; }
const std::vector<uint8_t> &poly17() { static const auto s = polySequence(0x1FFFF, 0x1FFFF, 16, 11); return s; }

}  // namespace

PokeySound::PokeySound(int sampleRate, double clock) : sampleRate_(sampleRate), clock_(clock) {}

void PokeySound::write(int reg, uint8_t value) {
	if (reg < 0 || reg > 7) return;
	if (reg & 1) audc_[reg >> 1] = value;
	else audf_[reg >> 1] = value;
	level_ = level();
}

int PokeySound::level() const {
	int sum = 0;
	for (int c = 0; c < 4; c++) {
		const int volume = audc_[c] & 0x0F;
		if (audc_[c] & 0x10) sum += volume;               // volume only
		else if (output_[c]) sum += volume;
	}
	return sum;   // 0..60
}

// One tick of the 64 kHz clock, with the polynomial counters at this cycle.
void PokeySound::tick() {
	const uint8_t p4 = poly4()[poly4Pos_], p5 = poly5()[poly5Pos_], p17 = poly17()[poly17Pos_];
	for (int c = 0; c < 4; c++) {
		if (--counter_[c] >= 0) continue;
		counter_[c] = audf_[c];                   // period AUDF + 1 ticks
		const uint8_t v = audc_[c];
		if (!(v & 0x80) && !p5) continue;
		if (v & 0x20) output_[c] ^= 1;
		else if (v & 0x40) output_[c] = p4;
		else output_[c] = p17;
	}
	level_ = level();
}

// Cycles with the output level unchanged: averaged into samples. A whole stretch at once: up to
// each sample's end, then the rest.
void PokeySound::count(long cycles, std::vector<int16_t> &out) {
	const double cyclesPerSample = clock_ / sampleRate_;
	if (samplePos_ + double(cycles) < cyclesPerSample) {   // within the sample (most calls)
		sum_ += long(level_) * cycles;
		sumCycles_ += cycles;
		samplePos_ += double(cycles);
		return;
	}
	while (cycles > 0) {
		// the sample ends on the cycle that takes samplePos_ to cyclesPerSample
		long toSample = long(std::ceil(cyclesPerSample - samplePos_));
		if (toSample < 1) toSample = 1;
		if (toSample > cycles) {
			sum_ += long(level_) * cycles;
			sumCycles_ += cycles;
			samplePos_ += double(cycles);
			return;
		}
		sum_ += long(level_) * toSample;
		sumCycles_ += toSample;
		samplePos_ += double(toSample) - cyclesPerSample;
		const double v = double(sum_) / double(sumCycles_);   // average over the sample: 0..60
		sum_ = sumCycles_ = 0;
		dcLevel_ += (v - dcLevel_) * 0.0005;                 // slow DC removal, as a speaker does
		const double s = (v - dcLevel_) * 800.0;
		out.push_back(int16_t(std::clamp(s, -32767.0, 32767.0)));
		cycles -= toSample;
	}
}

// From pulse to pulse: between them the 64 kHz ticks only count the channels down and the level
// does not change, so the cycles up to the next pulse are counted at once (as the ZX Spectrum
// Next's port jumps from pulse to pulse).
void PokeySound::run(long cycles, std::vector<int16_t> &out) {
	auto wrap = [](uint32_t &pos, long n, long size) {   // (a division only for long stretches)
		pos = uint32_t(n < size ? (pos + n >= uint32_t(size) ? pos + n - size : pos + n) : (pos + n) % size);
	};
	auto advancePolys = [&](long n) {
		wrap(poly4Pos_, n, long(poly4().size()));
		wrap(poly5Pos_, n, long(poly5().size()));
		wrap(poly17Pos_, n, long(poly17().size()));
	};
	while (cycles > 0) {
		const long toTick = std::max(tick_, 1);     // the next tick is on this cycle
		int ticks = counter_[0];                    // the next pulse: on tick counter + 1
		for (int c = 1; c < 4; c++) ticks = std::min(ticks, counter_[c]);
		ticks++;
		const long toPulse = toTick + long(ticks - 1) * kCyclesPer64k;
		if (toPulse > cycles) {                     // no pulse in what is left: ticks only count down
			if (cycles >= toTick) {
				const long passed = 1 + (cycles - toTick) / kCyclesPer64k;
				for (int c = 0; c < 4; c++) counter_[c] -= int(passed);
				tick_ = int(kCyclesPer64k - (cycles - toTick - (passed - 1) * kCyclesPer64k));
			} else {
				tick_ -= int(cycles);
			}
			advancePolys(cycles);
			count(cycles, out);
			return;
		}
		for (int c = 0; c < 4; c++) counter_[c] -= ticks - 1;
		count(toPulse - 1, out);
		advancePolys(toPulse);
		tick_ = kCyclesPer64k;
		tick();
		count(1, out);
		cycles -= toPulse;
	}
}

}  // namespace rr
