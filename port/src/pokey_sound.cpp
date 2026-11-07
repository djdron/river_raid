#include "pokey_sound.h"

#include <algorithm>
#include <cmath>

namespace rr {

namespace {
constexpr int kCyclesPer64k = 28;   // 64 kHz base clock
}

PokeySound::PokeySound(int sampleRate) : sampleRate_(sampleRate) {}

void PokeySound::write(int reg, uint8_t value) {
	if (reg < 0 || reg > 7) return;
	if (reg & 1) audc_[reg >> 1] = value;
	else audf_[reg >> 1] = value;
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

void PokeySound::run(long cycles, std::vector<int16_t> &out) {
	const double cyclesPerSample = kClock / sampleRate_;
	for (long i = 0; i < cycles; i++) {
		// polynomial counters, one step per cycle
		poly4_ = ((poly4_ << 1) | (((poly4_ >> 3) ^ (poly4_ >> 2)) & 1)) & 0x0F;
		poly5_ = ((poly5_ << 1) | (((poly5_ >> 4) ^ (poly5_ >> 2)) & 1)) & 0x1F;
		poly17_ = ((poly17_ << 1) | (((poly17_ >> 16) ^ (poly17_ >> 11)) & 1)) & 0x1FFFF;
		if (--tick_ <= 0) {
			tick_ = kCyclesPer64k;
			for (int c = 0; c < 4; c++) {
				if (--counter_[c] >= 0) continue;
				counter_[c] = audf_[c];                   // period AUDF + 1 ticks
				const uint8_t v = audc_[c];
				if (!(v & 0x80) && !(poly5_ & 1)) continue;
				if (v & 0x20) output_[c] ^= 1;
				else if (v & 0x40) output_[c] = poly4_ & 1;
				else output_[c] = poly17_ & 1;
			}
		}
		sum_ += level();
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

}  // namespace rr
