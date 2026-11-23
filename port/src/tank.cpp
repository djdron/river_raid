// The tank, its shell and the helicopter's missile. See tank.h.
#include "tank.h"

#include "river_gen.h"
#include "rom.h"

namespace rr {

void Tank::drive(uint8_t frameCount, Shell &shell, bool moving, bool atEdge, bool bridgeHit, const Rom &rom) {
	const uint8_t every = (kind >= 4 && kind < 8) ? 0x03 : 0x0F;
	if (every & frameCount) return;
	bool go = false;
	if (shell.none() && moving) {
		if (fires & 0x80) {
			if (y >= 0x1A) {
				if (!atEdge) go = true;               // not at the river edge yet
				else shell.timer++;                   // at the edge: fire (timer 0)
			}
		} else if (!bridgeHit) {
			go = true;                                // a tank on a bridge drives across
		}
	}
	if (go) {
		if ((kind & 0x7F) < 6) x++;
		else x--;
	}
	kind = (kind & 0x80) ? 0x03 : rom.nextTankKind(kind);
}

void Shell::move(const Tank &tank, bool noPlane, const Rom &rom) {
	uint8_t f = 0;
	if (!none()) {
		bool count = true;
		const bool right = tank.facingRight();
		if (timer == 0) {                             // A60F: launch from the tank
			if (tank.y < 0x19) {
				count = false;                        // the tank is too high: wait
			} else {
				y = uint8_t(tank.y - 6);
				xFrac = 0;
				if (right) {
					// ADC #9 (carry clear), SBC #4: one more off unless the sum carried
					const int sum = tank.x + 9;
					x = uint8_t(sum - 4 - (sum > 0xFF ? 0 : 1));
				} else {
					x = uint8_t(tank.x - 4);
				}
				f = 1;
			}
		} else if (timer < 0x11) {                    // A62A: in flight
			y = uint8_t(y + rom.shellRise(timer));
			if (right) {
				const int frac = xFrac + speedFrac;
				xFrac = uint8_t(frac);
				x = uint8_t(x + speed + (frac >> 8));
			} else {
				const int frac = xFrac - speedFrac;
				xFrac = uint8_t(frac);
				x = uint8_t(x - speed - (frac < 0 ? 1 : 0));
			}
			f = 1;
		} else {                                      // A657: explosion frames
			f = uint8_t(((timer - 0x11) >> 3) + 2);
		}
		if (count) {
			uint8_t t = uint8_t(timer + 1);           // A65E
			if (t >= 0x39) {                          // over: the tank fires again (no shell after a crash)
				t = noPlane ? 0xFF : 0;
				f = 0;
			}
			timer = t;
		}
	}
	frame = f;
}

bool Missile::move(const Slot *shooter, bool moving, bool hitBank) {
	uint8_t nx = 0;
	bool fired = false;
	if (shooter) {
		if (x) {                                      // flying
			if (!hitBank) nx = uint8_t(x + step);
		} else {
			if (!moving) {
				x = 0;
				return false;
			}
			const uint8_t ny = uint8_t(shooter->y - 0x3C);
			if (ny < 0x1C) return false;              // too high on the screen: wait
			y = ny;
			fired = true;
			nx = uint8_t(shooter->x - 3);
			if (shooter->flags & slotflag::kFacingLeft) {
				step = 0xFE;                          // -2
			} else {
				step = 0x02;
				nx = uint8_t(nx + 10);
			}
		}
	}
	x = nx;
	return fired;
}

}  // namespace rr
