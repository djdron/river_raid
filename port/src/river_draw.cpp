#include "river_draw.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rr {
namespace {

constexpr uint8_t kBackground = 0x00;
constexpr uint8_t kBankEdge = 0x20;     // COLPF1
constexpr uint8_t kWater = 0x84;        // COLPF2

// Scanline counter value of the row where new lines enter; the DLI counts it down per scanline.
constexpr uint8_t kEntryScanline = 0x5D;
// Sprite horizontal position of the left edge of the screen buffer (HPOSP3 = 32).
constexpr int kSpriteLeft = 32;

// Tables in the cartridge:
constexpr uint16_t kObjectHeight = 0xBB60;   // [type] last row of the object's sprite
constexpr uint16_t kShapeIndex = 0xBB40;     // [kind (+16 when flags bit 3)] offset into $B900
constexpr uint16_t kShapes = 0xB900;         // sprite rows, one byte each
constexpr uint16_t kColourIndex = 0xBB30;    // [type] offset into $B700
constexpr uint16_t kColours = 0xB700;        // sprite colour per row
constexpr uint16_t kRoadColours = 0xB4E5;    // bank colour per row next to a bridge

}  // namespace

uint8_t bankColour(const RiverState &s) {
	uint8_t c = uint8_t((s.mode & 4) ^ 0xD6);
	// While a bridge object is still being drawn in near the top, the shade is switched.
	if (s.newSlot < kSlots) {
		const Slot &sl = s.slot[s.newSlot];
		if ((sl.flags & 0x80) && sl.y < 0x5C) c ^= 4;
	}
	return c;
}

void drawScreenLine(const Rom &rom, const RiverState &s, const uint8_t line[kLineBytes], uint8_t out[kRowWidth]) {
	uint8_t bank = bankColour(s);

	// The first slot (from firstSlot up) whose sprite covers this scanline is drawn;
	// row = how far up from the object's bottom row the scanline is.
	bool haveSprite = false;
	uint8_t shape = 0, spriteColour = 0, spriteX = 0, spriteSize = 0;
	for (int k = s.firstSlot; k < kSlots; k++) {
		const Slot &sl = s.slot[k];
		const uint8_t height = rom.at(uint16_t(kObjectHeight + sl.type));
		const uint8_t row = uint8_t(sl.y + height - kEntryScanline);
		if (row > height || row >= 0x80) continue;
		if (sl.flags & 0x80) bank = rom.at(uint16_t(kRoadColours + row));   // bridge: road
		const int kind = sl.kind + ((sl.flags & 8) ? 16 : 0);
		shape = rom.at(uint16_t(kShapes + rom.at(uint16_t(kShapeIndex + kind)) + row));
		spriteColour = rom.at(uint16_t(kColours + rom.at(uint16_t(kColourIndex + sl.type)) + row));
		spriteX = sl.x;
		spriteSize = sl.size;
		haveSprite = true;
		break;
	}

	const uint8_t colours[4] = {kBackground, bank, kBankEdge, kWater};
	for (int p = 0; p < kLineBytes * 4; p++) {
		const int v = (line[p >> 2] >> (6 - 2 * (p & 3))) & 3;
		out[2 * p] = out[2 * p + 1] = colours[v];
	}

	if (haveSprite) {
		const int width = (spriteSize & 3) == 3 ? 4 : (spriteSize & 3) == 1 ? 2 : 1;   // SIZEP3
		for (int b = 0; b < 8; b++) {
			if (!(shape & (0x80 >> b))) continue;
			const int x0 = 2 * (spriteX + b * width - kSpriteLeft);
			for (int x = std::max(0, x0); x < std::min(kRowWidth, x0 + 2 * width); x++) out[x] = spriteColour;
		}
	}

	std::memset(out, 0, kVisibleFirst);
	std::memset(out + kVisibleLast + 1, 0, kRowWidth - kVisibleLast - 1);
}

namespace {

// GTIA output level of the 16 luminances relative to the highest (atari800, from GTIA data).
constexpr double kLuma[16] = {0.6941, 0.7091, 0.7241, 0.7401, 0.7560, 0.7741, 0.7931, 0.8121,
	0.8260, 0.8470, 0.8700, 0.8930, 0.9160, 0.9420, 0.9690, 1.0000};
constexpr double kPi = 3.14159265358979323846;
constexpr double kChroma = 0.175;       // colour amplitude
constexpr double kCrtGamma = 2.35;

// Video signal value (gamma-encoded for a CRT) to an sRGB byte.
uint8_t toSrgb(double c) {
	double linear = c >= 0.0 ? std::pow(c, kCrtGamma) : c / 12.92;
	double s = linear <= 0.0031308 ? linear * 12.92 : 1.055 * std::pow(linear, 1.0 / 2.4) - 0.055;
	return uint8_t(std::clamp(int(s * 255.0), 0, 255));
}

// NTSC: hue 1 is at the colour burst phase (303 degrees), each next hue 26.8 degrees later.
void ntscColour(int hue, double &i, double &q) {
	const double angle = (303.0 + (hue - 1) * 26.8) * kPi / 180.0;
	i = std::cos(angle) * kChroma;
	q = std::sin(angle) * kChroma;
}

// PAL: each hue is the colour signal delayed by a base delay, optionally one extra delay
// element and a number of steps of the adjustable delay; the phase alternates between even
// and odd lines and the TV averages the two (atari800's model from oscillograms).
void palColour(int hue, double &u, double &v) {
	struct Delay { int extra, steps; };
	static constexpr Delay even[15] = {{1, 5}, {1, 6}, {1, 7}, {0, 0}, {0, 1}, {0, 2}, {0, 4}, {0, 5},
		{0, 6}, {0, 7}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}};
	static constexpr Delay odd[15] = {{1, 1}, {1, 0}, {0, 7}, {0, 6}, {0, 5}, {0, 4}, {0, 2}, {0, 1},
		{0, 0}, {1, 7}, {1, 5}, {1, 4}, {1, 3}, {1, 2}, {1, 1}};
	const double base = 0.421894970414201, extra = 0.446563064859117, step = 23.2 / 360.0;   // in cycles
	auto delay = [&](const Delay &d) { return base + extra * d.extra + step * d.steps; };
	const double evenBurst = delay(even[0]), oddBurst = delay(odd[0]);
	const double subcarrier = (evenBurst + oddBurst) / 2.0;
	double burstDiff = evenBurst - oddBurst;
	burstDiff -= std::floor(burstDiff);
	const double amplitude = std::sqrt(2.0) / std::sqrt(2.0 * std::cos(burstDiff * 2.0 * kPi) + 2.0);
	const double evenAngle = (0.5 - (delay(even[hue - 1]) - subcarrier)) * 2.0 * kPi;
	const double oddAngle = (0.5 + (delay(odd[hue - 1]) - subcarrier)) * 2.0 * kPi;
	const double sat = kChroma * amplitude;
	u = (std::cos(evenAngle) + std::cos(oddAngle)) * sat / 2.0;
	v = (std::sin(evenAngle) + std::sin(oddAngle)) * sat / 2.0;
}

}  // namespace

void atariRgb(uint8_t colour, uint8_t rgb[3], bool pal) {
	const int hue = colour >> 4, lum = colour & 0x0F;
	const double y = (kLuma[lum] - kLuma[0]) / (kLuma[15] - kLuma[0]);
	double r = y, g = y, b = y;
	if (hue && pal) {
		double u, v;
		palColour(hue, u, v);
		r = y + 1.13983 * v;
		g = y - 0.39465 * u - 0.58060 * v;
		b = y + 2.03211 * u;
	} else if (hue) {
		double i, q;
		ntscColour(hue, i, q);
		r = y + 0.9563 * i + 0.6210 * q;
		g = y - 0.2721 * i - 0.6474 * q;
		b = y - 1.1070 * i + 1.7046 * q;
	}
	rgb[0] = toSrgb(r);
	rgb[1] = toSrgb(g);
	rgb[2] = toSrgb(b);
}

}  // namespace rr
