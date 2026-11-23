// Small number types of the game, kept as the original keeps them: bytes that wrap round, a
// fraction byte whose carry moves a whole byte on, BCD counters added as the 6502 adds in
// decimal mode. They only name what the bytes are; the arithmetic is the original's.
// tools/fixed_check tests them on every input against the code they replaced.
#pragma once
#include <cstdint>

namespace rr {

// A byte with a fraction byte below it: x + frac / 256. add() and sub() return the carry (or
// borrow) out of the fraction, which moves the whole byte by one, as ADC / SBC do.
struct Fixed8 {
	uint8_t whole = 0, frac = 0;
	bool addFrac(uint8_t f) {               // fraction only; true if it carried
		const unsigned s = unsigned(frac) + f;
		frac = uint8_t(s);
		return s > 0xFF;
	}
	bool subFrac(uint8_t f) {               // fraction only; true if it borrowed
		const bool borrow = frac < f;
		frac = uint8_t(frac - f);
		return borrow;
	}
	void add(uint8_t w, uint8_t f) { const bool c = addFrac(f); whole = uint8_t(whole + w + c); }
	void sub(uint8_t w, uint8_t f) { const bool b = subFrac(f); whole = uint8_t(whole - w - b); }
};

// One byte of BCD addition with carry in and out (ADC in decimal mode, for valid BCD).
inline uint8_t addBcd(uint8_t a, uint8_t b, int &carry) {
	int low = (a & 0x0F) + (b & 0x0F) + carry;
	int high = (a >> 4) + (b >> 4);
	if (low > 9) { low -= 10; high++; }
	carry = 0;
	if (high > 9) { high -= 10; carry = 1; }
	return uint8_t((high << 4) | low);
}

// N BCD bytes, the highest first (as the original keeps score, bridges and high scores).
// add() adds a value of up to two digits to the lowest byte and carries up; it returns the
// carry out of the highest byte (the counter went past all nines).
template <int N> struct Bcd {
	uint8_t d[N] = {};
	bool add(uint8_t low, int carry = 0) {
		d[N - 1] = addBcd(d[N - 1], low, carry);
		for (int i = N - 2; i >= 0; i--) d[i] = addBcd(d[i], 0, carry);
		return carry != 0;
	}
	// The original compares byte by byte from the highest
	bool operator<(const Bcd &o) const {
		for (int i = 0; i < N; i++)
			if (d[i] != o.d[i]) return d[i] < o.d[i];
		return false;
	}
	bool operator==(const Bcd &o) const {
		for (int i = 0; i < N; i++)
			if (d[i] != o.d[i]) return false;
		return true;
	}
	int digit(int k) const { return k & 1 ? d[k >> 1] & 0x0F : d[k >> 1] >> 4; }   // k = 0: the highest
};

}  // namespace rr
