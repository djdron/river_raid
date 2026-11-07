#include "palette.h"

#include <algorithm>
#include <cmath>

namespace rr {
namespace {

// ---- Altirra ------------------------------------------------------------------------------
// Following Altirra 4.31 (palettegenerator.cpp, gtia.cpp, gtiatables.cpp; Avery Lee), with its
// default settings. Computed in float, as there.

// Luminance levels 0-15, measured on an 800XL (least squares fit of the four resistor outputs).
constexpr float kAltirraLuma[16] = {0.0f, 0.0658340f, 0.1435022f, 0.2093362f, 0.2750246f, 0.3408586f,
	0.4185267f, 0.4843608f, 0.5156392f, 0.5814733f, 0.6591414f, 0.7249754f, 0.7906638f, 0.8564978f,
	0.9341660f, 1.0f};

constexpr float kTwoPi = 6.28318530717958647692f;

struct AltirraSettings {
	float hueStart;      // degrees in the I/Q plane of hue 1
	float hueRange;      // degrees for 15 hue steps
	float brightness;    // output of luminance 0
	float contrast;      // range of luminance 0..15
	float saturation;
	bool palQuirks;      // PAL hue phases (below)
	bool srgbMatching;   // convert from NTSC primaries to sRGB
};
constexpr AltirraSettings kAltirraNtsc = {-57.0f, 27.1f * 15.0f, -0.04f, 1.04f, 0.20f, false, true};
constexpr AltirraSettings kAltirraPal = {-12.0f, 18.3f * 15.0f, 0.0f, 1.0f, 0.29f, true, false};

// PAL: the phase of each hue in steps, and the inversion of the colour signal, on even and
// odd lines; a PAL TV averages the two.
struct PalPhase { float evenPhase, evenInvert, oddPhase, oddInvert; };
constexpr PalPhase kPalPhases[15] = {
	{0.0f, 1, 0.0f, 1}, {1.0f, 1, 1.0f, 1}, {-6.0f, -1, 2.0f, 1}, {-5.0f, -1, -5.0f, -1},
	{-4.0f, -1, -4.0f, -1}, {-3.0f, -1, -3.0f, -1}, {-1.0f, -1, -1.0f, -1}, {0.0f, -1, 0.0f, -1},
	{1.0f, -1, 1.0f, -1}, {-6.0f, 1, 2.0f, -1}, {-4.0f, 1, -4.0f, 1}, {-3.0f, 1, -3.0f, 1},
	{-2.0f, 1, -2.0f, 1}, {-1.0f, 1, -1.0f, 1}, {0.0f, 1, 0.0f, 1}};

// NTSC (SMPTE C) primaries to XYZ, and XYZ to sRGB.
constexpr float kNtscToXyz[3][3] = {
	{0.6068909f, 0.1735011f, 0.2003480f},
	{0.2989164f, 0.5865990f, 0.1144845f},
	{0.0000000f, 0.0660957f, 1.1162243f}};
constexpr float kXyzToSrgb[3][3] = {
	{3.2404542f, -1.5371385f, -0.4985314f},
	{-0.9692660f, 1.8760108f, 0.0415560f},
	{0.0556434f, -0.2040259f, 1.0572252f}};

uint8_t toByte(float c) {
	// round to nearest (even), clamped
	return uint8_t(std::clamp(std::nearbyint(c * 255.0f), 0.0f, 255.0f));
}

void altirraRgb(uint8_t colour, uint8_t rgb[3], bool pal) {
	const AltirraSettings &set = pal ? kAltirraPal : kAltirraNtsc;
	const int hue = colour >> 4, lum = colour & 0x0F;
	const float start = set.hueStart * (kTwoPi / 360.0f);
	const float step = set.hueRange * (kTwoPi / (360.0f * 15.0f));
	float i = 0, q = 0;
	if (hue && set.palQuirks) {
		const PalPhase &p = kPalPhases[hue - 1];
		const float a2 = start + step * p.evenPhase, a3 = start + step * p.oddPhase;
		i = (std::cos(a2) * p.evenInvert + std::cos(a3) * p.oddInvert) * (0.5f * set.saturation);
		q = (std::sin(a2) * p.evenInvert + std::sin(a3) * p.oddInvert) * (0.5f * set.saturation);
	} else if (hue) {
		// the angle advances one step per hue, as a float sum
		float angle = start;
		for (int h = 1; h < hue; h++) angle += step;
		i = set.saturation * float(std::cos(double(angle)));
		q = set.saturation * float(std::sin(double(angle)));
	}
	const float y = set.contrast * kAltirraLuma[lum] + set.brightness;
	float c[3] = {y + 0.956f * i + 0.621f * q, y - 0.272f * i - 0.647f * q, y - 1.107f * i + 1.704f * q};
	if (set.srgbMatching) {
		float lin[3], xyz[3];
		for (int k = 0; k < 3; k++) lin[k] = std::pow(std::max(c[k], 0.0f), 2.2f);
		for (int r = 0; r < 3; r++) xyz[r] = kNtscToXyz[r][0] * lin[0] + kNtscToXyz[r][1] * lin[1] + kNtscToXyz[r][2] * lin[2];
		for (int r = 0; r < 3; r++) {
			const float v = kXyzToSrgb[r][0] * xyz[0] + kXyzToSrgb[r][1] * xyz[1] + kXyzToSrgb[r][2] * xyz[2];
			c[r] = v < 0.0031308f ? v * 12.92f : 1.055f * std::pow(std::max(v, 0.0f), 1.0f / 2.4f) - 0.055f;
		}
	}
	for (int k = 0; k < 3; k++) rgb[k] = toByte(std::max(c[k], 0.0f));
}

// ---- atari800 -----------------------------------------------------------------------------
// Following atari800 (colours_ntsc.c, colours_pal.c) with its default settings, except the
// output levels.

// atari800: GTIA output level of the 16 luminances relative to the highest (from GTIA data).
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

void atari800Rgb(uint8_t colour, uint8_t rgb[3], bool pal) {
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

}  // namespace

const char *paletteName(PaletteModel model) { return model == PaletteModel::Altirra ? "Altirra" : "atari800"; }

void atariRgb(uint8_t colour, uint8_t rgb[3], bool pal, PaletteModel model) {
	if (model == PaletteModel::Altirra) altirraRgb(colour, rgb, pal);
	else atari800Rgb(colour, rgb, pal);
}

}  // namespace rr
