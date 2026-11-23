// The sounds the game asks for, and the driver that plays them on POKEY's four channels once a
// frame (the vertical blank's B2A5-B3D8).
#pragma once
#include <cstdint>
#include <functional>

#include "plane.h"
#include "ram_map.h"

namespace rr {

class Rom;

// POKEY sound registers ($D200-$D207): frequency divider and control (distortion, volume)
// of the four channels. AUDCTL ($D208) stays 0 in River Raid.
struct PokeyRegs {
	uint8_t audf[4] = {}, audc[4] = {};
};

// What the driver needs of the rest of the game this frame.
struct SoundContext {
	const Plane &plane;
	bool paused = false;
	bool attract = false;
	uint8_t shellTimer = 0xFF;
	// not in the original: the attract mode with sound (river viewer, Game::attractSound)
	bool attractSound = false;
	uint8_t attractEnginePitch = 0, attractEngineStick = 0x0F;
};

struct Sounds {
	// event: channel 1's event sounds
	static constexpr uint8_t kCrash = 1, kOutOfFuel = 2, kTankFull = 3, kRefuel = 4;

	uint8_t startBeep = 0;    // $C6 frames of the beep after START or SELECT
	uint8_t event = 0;        // $7A channel 1: an event sound
	uint8_t eventTime = 0;    // $7B its length; also the low-fuel beep counter
	uint8_t explosion = 0;    // $7C
	uint8_t fire = 0;         // $7D
	uint8_t bonus = 0;        // $7E a new plane at 10 000 points
	uint8_t missile = 0;      // $7F
	uint8_t crashPitch = 0;   // $052C engine pitch falling after a crash

	void mapRam(RamMap &m) {
		m.byte("sound.startBeep", 0xC6, startBeep).byte("sound.event", 0x7A, event).byte("sound.eventTime", 0x7B, eventTime);
		m.byte("sound.explosion", 0x7C, explosion).byte("sound.fire", 0x7D, fire).byte("sound.bonus", 0x7E, bonus);
		m.byte("sound.missile", 0x7F, missile).byte("sound.crashPitch", 0x052C, crashPitch);
	}

	// B2A5-B3D8: sets the four channels from the sounds asked for and the game, counting them
	// down. random(): POKEY's RANDOM, for the noises' pitch.
	void drive(PokeyRegs &pokey, const SoundContext &c, const std::function<uint8_t()> &random, const Rom &rom);
};

}  // namespace rr
