// The sound driver. See sound.h.
#include "sound.h"

#include "rom.h"

namespace rr {
namespace {
inline uint8_t lo8(int v) { return uint8_t(v & 0xFF); }
}  // namespace

// B2A5-B3D8: the sound driver. Once per frame it sets the four POKEY channels from the game
// state (AUDCTL stays 0: 64 kHz clock, 17-bit noise). AUDF sets the pitch (higher = lower
// note), AUDC the distortion (high 3 bits) and the volume (low 4 bits).
//   channel 1: the engine (pitch from the speed, tone from the stick), or the low-fuel
//              warning, or an event sound (crash, refuelling...), or the start beep;
//   channel 2: explosion noise or the helicopter's missile;
//   channel 3: firing or the bonus life sound;
//   channel 4: the tank shell in flight, then its explosion.
void Sounds::drive(PokeyRegs &p, const SoundContext &c, const std::function<uint8_t()> &random, const Rom &rom) {
	auto silenceChannels234 = [&]() { p.audc[1] = p.audc[2] = p.audc[3] = 0; };
	if (startBeep) {                            // B2A6
		startBeep--;
		p.audf[0] = 0x40;
		p.audc[0] = 0xAF;
		silenceChannels234();
		return;
	}
	if (c.paused) {                // B2BA
		p.audc[0] = 0;
		silenceChannels234();
		return;
	}
	// channel 1. `last` is the last value the original computed here (its A register): channel
	// 2 takes it as its pitch when it has nothing to play (at volume 0).
	uint8_t pitch = 0x39, control = 0, last = 0;
	bool setPitch = true;                         // B34C sets the pitch, B34F only the control
	if (event) {                           // B2D2: an event sound, eventSoundTime frames long
		uint8_t volume = eventTime;
		if (event <= kOutOfFuel) {
			if (event == kOutOfFuel) pitch = 0x11;
			volume = uint8_t((volume >> 1) | 0x80);
		} else {
			pitch = event == kTankFull ? 0x1F : 0x3F;
			volume = lo8(volume + 0xA0);
		}
		control = last = volume;
		if (--eventTime == 0) {
			event = 0;
			last = 0;
		}
	} else if (c.plane.state != Plane::kFlying && !(c.attractSound && c.attract)) {   // B2F9: no plane: channel 1 off
		last = c.plane.state;
		setPitch = false;
	} else {
		bool engine = true;
		if (c.plane.fuel < 0x40) {                      // B2FD: low fuel: a beep every 63 frames
			uint8_t time = eventTime ? eventTime : 0x3F;
			eventTime = --time;
			uint8_t beep = 0;
			if (c.plane.fuel < 0x04) {                  // almost empty: a tone falling with the fuel
				beep = uint8_t((c.plane.fuel << 2) | (c.plane.fuelFrac >> 6));
				beep = uint8_t(lo8((beep ^ 0xFF) + 0x20) << 1);
			}
			if (beep == 0 && time >= 0x1C) beep = time;   // the first frames of each 63
			if (beep) {                           // B326
				pitch = last = uint8_t(beep << 1);
				control = 0xAF;
				engine = false;
			}
		}
		if (engine) {                             // B32C: the engine, pitch from the speed
			pitch = lo8(0x1F - (c.plane.speed >> 4));
			if (crashPitch >= 0x1B) pitch = crashPitch--;   // falling pitch after a crash
			control = last = rom.engineTone(c.plane.stick);       // tone from up/down
			if (c.attractSound && c.attract) {     // (not in the original: the viewer's plane)
				if (c.attractEnginePitch) pitch = c.attractEnginePitch;
				control = last = rom.engineTone(c.attractEngineStick);
			}
		}
	}
	if (setPitch) p.audf[0] = pitch;
	p.audc[0] = control;
	// channel 2
	pitch = last;
	control = explosion;
	if (control) {                                // B352: explosion noise, fading
		explosion--;
		control = uint8_t((((control >> 1) + 4) & 0x0F) + 0x80);
		pitch = uint8_t(random() | 0x18);
	}
	if (const uint8_t t = missile) {       // B367: the helicopter's missile
		missile--;
		if (!c.attract || c.attractSound) {
			control = lo8(t + 0x46);
			pitch = uint8_t(((lo8((control ^ 0x0E) + 4)) & 0x0F) << 2);
		}
	}
	p.audf[1] = pitch;
	p.audc[1] = control;
	// channel 3
	control = 0;
	pitch = 0;
	if (const uint8_t t = fire) {          // B386: firing, rising
		fire--;
		const uint8_t twice = uint8_t(lo8(0x1C - t) << 1);
		pitch = lo8(uint8_t(twice << 1) + twice + (twice >> 7));   // 6 times, with the carry of the shift
		control = 0xA8;
	}
	if (bonus) {                           // B399: a bonus life
		const uint8_t t = --bonus;
		control = uint8_t((t & 0x0F) + 0xA0);
		pitch = rom.scorePitch(t >> 4);
	}
	p.audf[2] = pitch;
	p.audc[2] = control;
	// channel 4
	control = 0;
	if ((!c.attract || c.attractSound) && !(c.shellTimer & 0x80) && c.shellTimer != 0) {   // B3B6: the tank shell
		if (c.shellTimer < 0x11) {
			p.audf[3] = rom.shellPitch(c.shellTimer);   // whistling down
			control = 0xAC;
		} else {
			control = 0x8C;                       // exploding: noise
			p.audf[3] = uint8_t(random() | 0x20);
		}
	}
	p.audc[3] = control;
}

}  // namespace rr
