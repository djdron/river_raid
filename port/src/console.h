// The console's modes and counters: the frame counter, the attract mode and its timer, the time
// without input (after which the screen goes off), the pause, SELECT's repeat, the screen being
// cleared. Addresses $18, $24, $25, $28, $29, $2A, $78, $C7.
#pragma once
#include <cstdint>

#include "ram_map.h"

namespace rr {

struct Console {
	// pauseState: a key press pauses, the next press resumes
	static constexpr uint8_t kPlaying = 0, kResuming = 1, kPaused = 2, kPausing = 3;

	uint8_t frameCount = 0;     // $18 +1 per frame (VBI)
	uint8_t attractFlags = 0;   // $29 bit 7: attract mode (no player); bits 2-0 count frameCount wraps
	uint8_t attractTimer = 0;   // $2A counts down every 8 frames while not playing; 0 while playing
	uint8_t idleTime = 0;       // $78 +1 every 2048 frames without input; bit 7: screen off
	uint8_t pauseState = 0;     // $24 3 key pressed, 2 paused, 1 key pressed again, 0 playing
	uint8_t selectDelay = 0;    // $25 frames until SELECT/OPTION changes the game again
	uint8_t powerOnDone = 0;    // $28
	uint8_t screenOn = 0;       // $C7 0 while the screen is being cleared (colours off)

	// The attract mode: the game plays itself, nobody has pressed START
	bool attract() const { return (attractFlags & 0x80) != 0; }
	bool paused() const { return pauseState >= kPaused; }
	bool screenOff() const { return (idleTime & 0x80) != 0; }

	// A19E-A1F5, the main loop's start. `active`: a joystick, fire or console key (not idle, no
	// pause); `keyDown`: a key of the keyboard (SKSTAT bit 2), which pauses: press, release
	// (paused), press, release (playing again). False: the main loop stops here this frame
	// (pausing, paused, or the screen off after a long time without input).
	bool keys(bool active, bool keyDown);
	// A2F5: the attract timer, -1 every 8 frames; from 0 it goes round to $FF. True when it has
	// just run out.
	bool attractTimerTick();
	// The VBI's counters (B3D9-B3F0): the frame counter; every 256 frames the attract flags'
	// count, and every 2048 the idle time (128 x 2048 frames: the screen goes off).
	void countFrame();
	// A699: SELECT or OPTION held: true at once and then every 30 frames (selectDelay).
	bool selectRepeat(bool held) {
		if (!held) {
			selectDelay = 0;
			return false;
		}
		return selectDelay-- == 0;
	}

	void mapRam(RamMap &m) {
		m.byte("frameCount", 0x18, frameCount).byte("attractFlags", 0x29, attractFlags).byte("attractTimer", 0x2A, attractTimer);
		m.byte("idleTime", 0x78, idleTime).byte("pauseState", 0x24, pauseState).byte("selectDelay", 0x25, selectDelay);
		m.byte("powerOnDone", 0x28, powerOnDone).byte("screenOn", 0xC7, screenOn);
	}
};

}  // namespace rr
