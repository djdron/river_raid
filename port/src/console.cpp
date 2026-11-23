#include "console.h"

namespace rr {

bool Console::keys(bool active, bool keyDown) {
	if (active) {
		idleTime = 0;
		pauseState = kPlaying;
	}
	if (keyDown) idleTime = 0;
	switch (pauseState) {
	case kPlaying:
		if (keyDown) {
			pauseState = kPausing;
			return false;
		}
		break;
	case kResuming:
		if (!keyDown) pauseState--;
		break;
	case kPaused:
		if (!keyDown) return false;
		pauseState--;
		break;
	default:                                      // kPausing
		if (keyDown) return false;
		pauseState--;
		return false;
	}
	return !screenOff();                          // A1F5
}

bool Console::attractTimerTick() {
	if (!attractTimer || (frameCount & 7)) return false;
	attractTimer = uint8_t(attractTimer - 1);
	if (attractTimer) return false;
	attractTimer = 0xFF;
	return true;
}

void Console::countFrame() {
	frameCount++;
	if (frameCount == 0) {
		attractFlags = uint8_t((attractFlags + 1) & 0xC7);
		if (!(attractFlags & 7)) {
			idleTime++;
			if (idleTime == 0) idleTime = 0x80;
		}
	}
}

}  // namespace rr
