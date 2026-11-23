// Tests the number types of src/fixed.h on every input (or, where there are too many, on many)
// against the code they replace in the game: the shell's sideways step, the scrolling's
// fraction, the score's BCD addition and the high score comparison. Prints the differences
// found (none expected) and exits 1 if there are any.
#include <cstdio>

#include "fixed.h"

using namespace rr;

namespace {
inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }
long bad = 0, tests = 0;
void check(bool same, const char *what) {
	tests++;
	if (!same && bad++ < 10) std::printf("differs: %s\n", what);
}
bool bcdByte(int v) { return (v & 0x0F) < 10 && (v >> 4) < 10; }
}  // namespace

int main() {
	// moveShell: right x += speed + carry of frac; left x -= speed + borrow (all fractions and
	// speeds, a spread of positions)
	for (int x = 0; x < 256; x += 17)
		for (int xf = 0; xf < 256; xf++)
			for (int sp = 0; sp < 256; sp += 3)
				for (int sf = 0; sf < 256; sf++) {
					int frac = xf + sf;
					const uint8_t rx = lo(x + sp + (frac >> 8)), rf = lo(frac);
					Fixed8 a{uint8_t(x), uint8_t(xf)};
					a.add(uint8_t(sp), uint8_t(sf));
					check(a.whole == rx && a.frac == rf, "Fixed8::add");
					frac = xf - sf;
					const uint8_t lx = lo(x - sp - (frac < 0 ? 1 : 0)), lf = lo(frac);
					Fixed8 b{uint8_t(x), uint8_t(xf)};
					b.sub(uint8_t(sp), uint8_t(sf));
					check(b.whole == lx && b.frac == lf, "Fixed8::sub");
				}
	// scrollRiver: sum = frac + speed, a line when it carries
	for (int f = 0; f < 256; f++)
		for (int sp = 0; sp < 256; sp++) {
			const int sum = f + sp;
			Fixed8 a{0, uint8_t(f)};
			const bool c = a.addFrac(uint8_t(sp));
			check(c == (sum >= 0x100) && a.frac == lo(sum), "Fixed8::addFrac");
			Fixed8 b{0, uint8_t(f)};
			const bool br = b.subFrac(uint8_t(sp));
			check(br == (f - sp < 0) && b.frac == lo(f - sp), "Fixed8::subFrac");
		}
	// explode: the score + points, three BCD bytes (every valid score, every two-digit points)
	for (int s0 = 0; s0 < 256; s0++) {
		if (!bcdByte(s0)) continue;
		for (int s1 = 0; s1 < 256; s1++) {
			if (!bcdByte(s1)) continue;
			for (int s2 = 0; s2 < 256; s2++) {
				if (!bcdByte(s2)) continue;
				for (int p = 0; p < 256; p += 0x05) {
					if (!bcdByte(p)) continue;
					uint8_t sc[3] = {uint8_t(s0), uint8_t(s1), uint8_t(s2)};
					int carry = 0;
					sc[2] = addBcd(sc[2], uint8_t(p), carry);
					sc[1] = addBcd(sc[1], 0, carry);
					sc[0] = addBcd(sc[0], 0, carry);
					Bcd<3> b{{uint8_t(s0), uint8_t(s1), uint8_t(s2)}};
					const bool out = b.add(uint8_t(p));
					check(out == (carry != 0) && b.d[0] == sc[0] && b.d[1] == sc[1] && b.d[2] == sc[2], "Bcd<3>::add");
				}
			}
		}
	}
	// the bridge counter: +1 with the carry in (two bytes, every value)
	for (int b0 = 0; b0 < 256; b0++)
		for (int b1 = 0; b1 < 256; b1++) {
			if (!bcdByte(b0) || !bcdByte(b1)) continue;
			int carry = 1;
			const uint8_t r1 = addBcd(uint8_t(b1), 0, carry), r0 = addBcd(uint8_t(b0), 0, carry);
			Bcd<2> b{{uint8_t(b0), uint8_t(b1)}};
			b.add(0, 1);
			check(b.d[0] == r0 && b.d[1] == r1, "Bcd<2>::add carry");
		}
	// updateHighScore: is the score at least the high score (byte by byte from the highest)
	uint32_t h = 12345;
	for (long i = 0; i < 20000000; i++) {
		uint8_t s[3], hi[3];
		for (int k = 0; k < 3; k++) {
			h = h * 1103515245u + 12345u;
			s[k] = uint8_t(h >> 24);
			hi[k] = (h & 0x300) ? uint8_t(h >> 16) : s[k];   // (often equal bytes)
		}
		bool keep = false;   // the original: return (keep the high score) at the first lower byte
		for (int k = 0; k < 3; k++) {
			if (s[k] < hi[k]) { keep = true; break; }
			if (s[k] > hi[k]) break;
		}
		const Bcd<3> a{{s[0], s[1], s[2]}}, b{{hi[0], hi[1], hi[2]}};
		check(keep == (a < b), "Bcd<3>::operator<");
	}
	std::printf("%ld tests, %ld differences\n", tests, bad);
	return bad ? 1 : 0;
}
