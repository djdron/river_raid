// The two kinds of Atari: NTSC (America, Japan) and PAL (Europe). The cartridge is the same and
// so is its game logic per frame; the machines differ in the TV frame (262 or 312 scanlines of
// 114 machine cycles) and the machine clock, so a PAL machine runs the game at 49.86 frames per
// second instead of 59.92, about 17% slower, and its sounds are a little lower. The game reads
// which one it is from GTIA at power-on ($D014 -> $BF) and lays out the panel below the river
// accordingly (see status_bar.h).
#pragma once

namespace rr {

struct TvSystem {
	bool pal;
	int linesPerFrame;   // scanlines per frame
	double clock;        // machine cycles per second
	static constexpr int kCyclesPerLine = 114;
	long cyclesPerFrame() const { return long(kCyclesPerLine) * linesPerFrame; }
	double frameRate() const { return clock / cyclesPerFrame(); }
	// width of a pixel column / height of a line as the TV shows them
	double pixelAspect() const { return pal ? 1.03 : 0.86; }
};

inline constexpr TvSystem kNtsc{false, 262, 1789772.5};
inline constexpr TvSystem kPal{true, 312, 1773447.0};
inline const TvSystem &tvSystem(bool pal) { return pal ? kPal : kNtsc; }

}  // namespace rr
