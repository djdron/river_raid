// The plane flying, shooting, hitting and crashing; the objects exploding and their points.
// Part of Game (game.h).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>

namespace rr {

// A3E9-A5B4: the plane is flying.
void Game::flyPlane(const Input &input) {
	using namespace object;
	RiverState &s = world_.gen.s;
	Objects &objs = world_.objects;
	// A3E9: the plane touched an object
	const uint8_t touched = hit_.planeHitObject;
	if (!(touched & 0x80)) {
		const uint8_t kind = objs.slot[touched].kind;
		if (kind >= kJet) {
			if (kind <= kBridge) {                // an enemy or a bridge: both explode
				if (kind == kBridge) s.bridgeHit++;
				crash(kCrashSound, 0x1F);
				explode(touched, kRammed);
				return;
			}
			// A40A: a fuel depot
			const uint8_t event = plane_.refuel() ? kTankFullSound : kRefuelSound;
			if (event != sound_.event) {
				sound_.event = event;
				sound_.eventTime = 8;
			}
		}
	}
	// A424: the tank shell exploding on the plane, the helicopter's missile, the bank
	const bool shellExploding = objs.shell.timer >= 0x11;   // (also with no shell: timer $FF)
	if ((!(hit_.planeHitShell & 0x80) && shellExploding) || !(hit_.missileHitPlane & 0x80) ||
		!(hit_.planeHitBank & 0x80)) {
		crash(kCrashSound, 0x1F);
		return;
	}
	if (!plane_.burnFuel()) {                    // A43F
		crash(kOutOfFuelSound, 0x23);
		return;
	}
	plane_.steer();
	plane_.changeSpeed();
	// A4B6: the bullet hit an object
	const uint8_t shot = hit_.bulletHitObject;
	if (!(shot & 0x80) && objs.slot[shot].kind >= kJet) {
		explode(shot, kShot);
		return;
	}
	// A4C1: the tank shell's explosion destroys an object
	const uint8_t shelled = hit_.shellHitObject;
	if (!(shelled & 0x80) && shellExploding && objs.slot[shelled].kind >= kHelicopter) {
		objs.slot[shelled].kind = kShot;
		sound_.explosion = 0x17;
	}
	// A58D: the bullet stops at the bank, flies up 6 lines per frame, or is fired
	if (!(hit_.bulletHitBank & 0x80)) {
		moveBullet(1);
	} else if (bullet_.y >= 0x1C) {
		moveBullet(bullet_.y - 6);
	} else if (input.trigger[match_.player & 1]) {
		sound_.fire = 0x0F;
		moveBullet(0xA9);
	} else {
		moveBullet(1);
	}
}

// A4E0-A589: the object in slot n explodes (kind kShot or kRammed); points.
void Game::explode(int n, uint8_t kind) {
	using namespace object;
	RiverState &s = world_.gen.s;
	Objects &objs = world_.objects;
	Slot &obj = objs.slot[size_t(n)];
	obj.kind = kind;
	uint8_t type = obj.type;
	sound_.explosion = 0x17;
	if (type == kBridge) {
		bridgeFlash_ = 0x17;
		match_.bridges.add(0, 1);                      // bridge counter +1
		s.bridgeHit++;
		// A500: a tank on the bridge goes with it
		if (lo(obj.x - 8) < objs.tank.x && lo(obj.x + 0x1F) >= objs.tank.x) {
			objs.tank.kind |= 0x80;
			sound_.explosion = 0x37;
			type = kBridgeWithTank;
		} else if (match_.bridges.d[0] != 0 || match_.bridges.d[1] >= 5) {
			// A52B: from bridge 5 on, a tank close to the bridge fires
			const bool near = objs.tank.kind < 6 ? lo(obj.x - objs.tank.x) < 0x29 : lo(objs.tank.x - obj.x) < 0x41;
			if (near) objs.shell.timer++;
		}
	}
	// A547: points; every 10 000 points a new plane, up to 9
	const Match::Points points = match_.addPoints(rom_.points(type));
	if (points == Match::Points::GameEnds) {    // past 999 999
		crash(kCrashSound, 0x1F);
		return;
	}
	if (points == Match::Points::BonusLife) sound_.bonus = 0x40;
	moveBullet(1);                                // A589: the bullet is used up
}

// sub $B1C0: the plane crashes, with an event sound of this length.
void Game::crash(uint8_t event, uint8_t length) {
	RiverState &s = world_.gen.s;
	Objects &objs = world_.objects;
	sound_.eventTime = length;
	sound_.event = event;
	// where the next plane starts: past a destroyed bridge, from the next section; on the run-up
	// to a bridge not hit, one checkpoint back
	if ((objs.slot[kSlots - 1].flags ^ s.blockFlags) & kBankShade) {
		if (s.bridgeHit) {
			if (s.blockFlags & kBridgeBlock) world_.gen.sectionCheckpoint();
		} else if (!(s.blockFlags & kBridgeBlock)) {
			uint8_t level = lo(s.level - 1);
			if (level == 0x4E) level = 0x50;
			s.level = level;
			std::memcpy(s.checkpoint, s.prevCheckpoint, 4);
		}
	}
	plane_.frame = 6;
	// B200
	plane_.state = kCrashed;
	console_.attractTimer = 0;
	removePlaneThings();
}

// sub $B21A: no plane (game over or attract mode).
void Game::gameOverScreen() {
	console_.frameCount = 0xFF;
	plane_.state = kNoPlane;
	console_.attractTimer = 0xFF;
	removePlaneThings();
}

// B204-B220: the bullet and the helicopter's missile go.
void Game::removePlaneThings() {
	bullet_.remove();
	world_.objects.shooterSlot = kNoSlot;
}

// A5AA-A5B4: the bullet to row y.
void Game::moveBullet(int y) { bullet_.moveTo(y); }

}  // namespace rr
