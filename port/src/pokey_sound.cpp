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

PokeySound::PokeySound(int sampleRate) : sampleRate_(sampleRate) {}

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

// Cycles with the output level unchanged: averaged into samples.
void PokeySound::count(long cycles, std::vector<int16_t> &out) {
	const double cyclesPerSample = kClock / sampleRate_;
	for (long i = 0; i < cycles; i++) {
		sum_ += level_;
		sumCycles_++;
		samplePos_ += 1;
		if (samplePos_ >= cyclesPerSample) {
			samplePos_ -= cyclesPerSample;
			const double v = double(sum_) / double(sumCycles_);   // average over the sample: 0..60
			sum_ = sumCycles_ = 0;
			dcLevel_ += (v - dcLevel_) * 0.0005;                 // slow DC removal, as a speaker does
			const double s = (v - dcLevel_) * 800.0;
			out.push_back(int16_t(std::clamp(s, -32767.0, 32767.0)));
		}
	}
}

void PokeySound::run(long cycles, std::vector<int16_t> &out) {
	auto advancePolys = [&](long n) {
		poly4Pos_ = uint32_t((poly4Pos_ + n) % long(poly4().size()));
		poly5Pos_ = uint32_t((poly5Pos_ + n) % long(poly5().size()));
		poly17Pos_ = uint32_t((poly17Pos_ + n) % long(poly17().size()));
	};
	while (cycles > 0) {
		const long toTick = std::max(tick_, 1);   // the tick is on this cycle
		if (toTick > cycles) {                    // no tick in what is left
			advancePolys(cycles);
			tick_ -= int(cycles);
			count(cycles, out);
			return;
		}
		count(toTick - 1, out);
		advancePolys(toTick);
		tick_ = kCyclesPer64k;
		tick();
		count(1, out);
		cycles -= toTick;
	}
}

}  // namespace rr
