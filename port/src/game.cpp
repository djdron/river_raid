// River Raid game frame. See game.h. Addresses refer to the original cartridge code.
#include "game.h"

namespace rr {
namespace {
inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }
inline bool attract(const GameState &g) { return (g.flags29 & 0x80) != 0; }
}  // namespace

Game::Game(const Rom &rom) : gen(rom), rom_(rom) {}

void Game::startAttract(bool pal) {
	gen.s = RiverState();
	gen.startGame(1, pal);
	g = GameState();
	screen.clear();
	// player-missile memory as sub $B0F1 leaves it: the rainbow logo (players 2 and 3, rows
	// $DB-$E1) and the fuel needle (missile 1, rows $CC-$D5); NTSC rows
	for (int i = 0; i < 7; i++) {
		g.pm.at(uint16_t(0x0EDB + i)) = 0xFF;
		g.pm.at(uint16_t(0x0FDB + i)) = 0xFC;
	}
	for (int i = 0; i < 10; i++) g.pm.at(uint16_t(0x0BCC + i)) = 0x0C;
	// game over path (A3B1, sub $B21A) at the end of the power-on sequence, then frame 38
	g.flags29 = 0x81;
	g.attractTimer = 0xFF;
	g.deathTimer = 0xFF;
	g.speed = 0x80;
	g.fuel = 0xFF;
	g.fuelLo = 0xFF;
	g.planeX = 0x7C;
	std::array<uint8_t, kLineBytes> line;
	gen.nextLine(line.data());
	screen.insert(screen.begin(), line);
	if (afterLine) afterLine(line.data());
}

void Game::frame(const std::function<uint8_t()> &random) {
	prepare();
	display();
	logic(random);
	vbi(random);
}

// AE99-AF38, at the top of the frame: shapes of the tank (player 1), the helicopter missile
// (missile 0), the tank shell (player 0) and the plane (player 2) into player-missile memory.
void Game::prepare() {
	RiverState &s = gen.s;
	auto copy = [&](uint16_t table, int index, uint16_t dest, int count) {   // sub $B00F
		const uint16_t src = rom_.word(uint16_t(table + 2 * index));
		for (int y = count - 1; y >= 0; y--) g.pm.at(uint16_t(dest + y)) = rom_.at(uint16_t(src + y));
	};
	// tank: gone once it has scrolled below the screen
	const uint8_t tankY = s.roadY;
	if (s.roadY >= 0xBC) s.roadKind = s.roadY = 0;
	copy(0xB4D1, s.roadKind, uint16_t(0x0D00 + tankY), 11);
	g.shellColour = rom_.at(uint16_t(0xBAC5 + ((g.frameCount >> 3) & 7)));
	// helicopter missile: one row of missile 0, the rows above it cleared
	if (s.vC2 >= 0x1C) {
		g.pm.at(uint16_t(0x0AFE + s.vC2)) &= 0xFC;
		g.pm.at(uint16_t(0x0AFF + s.vC2)) &= 0xFC;
		uint8_t m = g.pm.at(uint16_t(0x0B00 + s.vC2)) & 0xFC;
		if (s.vC1) m |= 0x03;
		g.pm.at(uint16_t(0x0B00 + s.vC2)) = m;
	}
	// tank shell
	const uint8_t shellY = s.v40;
	if (s.v40 >= 0xBC) {
		s.v50 = 0xFF;
		g.shellFrame = 0;
		s.v40 = 0;
	}
	copy(0xBAB7, g.shellFrame, uint16_t(0x0C00 + shellY), 16);
	// plane
	copy(0xBACD, g.planeFrame, 0x0EAA, 14);
	// (AF3A-AF4F: score digits into the panel; not needed in the attract mode)
}

void Game::logic(const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	if (g.idle & 0x80) return;   // A1F5: screen off after a long time without input

	// A1F9: objects move on alternate frames, odd and even slots in turn
	s.vB1 = g.frameCount ^ s.toggle33;   // $B1
	// A1FF: every 16 frames a random slot is woken up (bit 6: it may move)
	if ((g.frameCount & 0x0F) == 0) {
		uint8_t x = random() & 0x0F;
		if (x >= 0x0B) x = lo(x - 5);
		if (x != s.newSlot) s.slot[x].flags |= 0x40;
	}
	tankOnRoad();
	moveObjects();

	// A2F5: attract timer, -1 every 8 frames
	if (g.attractTimer && !(g.frameCount & 7)) {
		uint8_t x = lo(g.attractTimer - 1);
		if (x == 0) {
			x = 0xFF;
			// (A303: at the first expiry the attract mode starts; not modelled, it is already on)
		}
		g.attractTimer = x;
	}
	// A312: joystick (no input in the attract mode)
	g.joystickPrev = lo(g.joystick << 4);
	g.joystick = 0x0F;
	// A33F: no plane ($76 = $FF): straight to A5B7
	helicopterMissile();
	tankShell();
	// A673-A6DC: console keys (none pressed): OPTION delay cleared
	g.optionDelay = 0;
	// A6DE/A6ED: in the attract mode the river scrolls at speed $80 (games 1-4) or $FE
	if (!attract(g)) return;
	g.speed = g.gameNumber < 4 ? 0x80 : 0xFE;
	scrollRiver();
}

// A21B-A26A: the tank on the road drives towards the river, every 16 frames (every 4 for
// kinds 4-7), stops at the edge and fires; its kind animates through the table $BAAF.
void Game::tankOnRoad() {
	RiverState &s = gen.s;
	const uint8_t mask = (s.roadKind >= 4 && s.roadKind < 8) ? 0x03 : 0x0F;
	if (mask & g.frameCount) return;
	bool move = false;
	if (s.v50 & 0x80) {                          // no shell in flight
		if (attract(g) || g.deathTimer == 0) {
			if (s.v41 & 0x80) {                  // tank that fires
				if (s.roadY >= 0x1A) {
					if (g.hit.p1pf & 0x80) move = true;   // not at the river edge yet
					else s.v50++;                          // at the edge: fire ($50 = 0)
				}
			} else if (s.bridgeHit == 0) {
				move = true;
			}
		}
	}
	if (move) {
		if ((s.roadKind & 0x7F) < 6) s.roadX++;
		else s.roadX--;
	}
	s.roadKind = (s.roadKind & 0x80) ? 0x03 : rom_.at(uint16_t(0xBAAF + s.roadKind));
}

// A26C-A2ED: animation and movement of the objects in the slots.
void Game::moveObjects() {
	RiverState &s = gen.s;
	for (int x = s.firstSlot; x < kSlots; x++) {
		Slot &sl = s.slot[x];
		uint8_t kind = sl.kind;
		if (kind >= 0x0E) continue;                       // bridge, house...
		if (kind < 0x0C) {                                // animation: every 8 frames, or 2 for kinds 7+
			const uint8_t mask = kind >= 7 ? 0x01 : 0x07;
			if (!(mask & g.frameCount)) {
				sl.kind = rom_.at(uint16_t(0xBB24 + kind));
				kind = sl.kind;
			}
		}
		if (!attract(g) && g.deathTimer != 0) continue;   // nothing moves while the plane respawns
		if ((s.level >> 1) == 0) continue;                // nothing moves before bridge 2
		if (kind != 7) {
			if (kind < 7) continue;                       // explosions, fuel depots
			if (kind == 0x0C && (g.frameCount & 2)) continue;
			if ((x ^ s.vB1) & 1) continue;             // every other frame
			const uint8_t f = sl.flags;
			if (!(f & 0x40)) continue;                    // not woken up yet
			if (f & 0x01) {                               // touching the bank edge (DLI collision bit)
				if (!(f & 0x10)) sl.flags = f ^ 0x18;     // turn round, once
			} else {
				sl.flags = f & 0xEF;
			}
		}
		// A2CA: one step left or right, wrapping round the screen
		if (sl.flags & 0x08) {
			sl.x = lo(sl.x - 1);
			if (sl.x < 0x2C) sl.x = 0xD4;
		} else {
			sl.x = lo(sl.x + 1);
			if (sl.x >= 0xD5) sl.x = 0x2C;
		}
	}
}

// A5B7-A605: a helicopter in slot $51 fires a missile (missile 0) along its direction.
void Game::helicopterMissile() {
	RiverState &s = gen.s;
	uint8_t y = 0;
	const int x = s.jetSlot;
	if (x < kSlots && (s.slot[x].kind == 0x0A || s.slot[x].kind == 0x0B)) {
		if (s.vC1) {                                      // flying: until it hits the bank
			if (g.hit.m0pf & 0x80) y = lo(s.vC1 + g.missileDx);
		} else {
			if (!attract(g) && g.deathTimer != 0) { s.vC1 = 0; return; }
			const uint8_t dy = lo(s.slot[x].y - 0x3C);
			if (dy < 0x1C) return;                        // too high on the screen: wait
			s.vC2 = dy;
			g.soundMissile = 9;
			y = lo(s.slot[x].x - 3);
			if (s.slot[x].flags & 0x08) {
				g.missileDx = 0xFE;
			} else {
				g.missileDx = 0x02;
				y = lo(y + 10);                           // ADC #$09 with carry set
			}
		}
	}
	s.vC1 = y;
}

// A607-A671: the tank's shell flies from the tank across the river ($50 counts 0..$38).
void Game::tankShell() {
	RiverState &s = gen.s;
	uint8_t frame = 0;
	if (!(s.v50 & 0x80)) {
		bool step = true;
		if (s.v50 == 0) {                                 // A60F: launch
			if (s.roadY < 0x19) {
				step = false;
			} else {
				s.v40 = lo(s.roadY - 6);
				g.shellXLo = 0;
				if (s.roadKind < 6) {                     // facing right: ADC #9 (carry clear), SBC #4
					const int sum = s.roadX + 9;
					g.shellX = lo(sum - 4 - (sum > 0xFF ? 0 : 1));
				} else {
					g.shellX = lo(s.roadX - 4);
				}
				frame = 1;
			}
		} else if (s.v50 < 0x11) {                       // A62A: flight
			s.v40 = lo(s.v40 + rom_.at(uint16_t(0xB4AF + s.v50)));
			if (s.roadKind < 6) {
				const int sum = g.shellXLo + s.v3F;
				g.shellXLo = lo(sum);
				g.shellX = lo(g.shellX + s.v3E + (sum >> 8));
			} else {
				const int diff = g.shellXLo - s.v3F;
				g.shellXLo = lo(diff);
				g.shellX = lo(g.shellX - s.v3E - (diff < 0 ? 1 : 0));
			}
			frame = 1;
		} else {                                          // A657: explosion frames
			frame = lo(((s.v50 - 0x11) >> 3) + 2);
		}
		if (step) {
			uint8_t t = lo(s.v50 + 1);                    // A65E
			if (t >= 0x39) {
				t = g.deathTimer ? 0xFF : 0;
				frame = 0;
			}
			s.v50 = t;
		}
	}
	g.shellFrame = frame;
}

// A6FD-A70F: $59 is added twice per frame to $30; each carry is one new river line.
// Speeds $FE and up give a line on both steps.
void Game::scrollRiver() {
	for (int step = 0; step < 2; step++) {
		if (g.speed < 0xFE) {
			const int sum = g.scrollFrac + g.speed;
			g.scrollFrac = lo(sum);
			if (sum < 0x100) continue;
		}
		std::array<uint8_t, kLineBytes> line;
		gen.nextLine(line.data());
		screen.insert(screen.begin(), line);
		if (screen.size() > 256) screen.pop_back();
		if (afterLine) afterLine(line.data());
	}
}

// B2A5-B3F0: vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	soundDriver(random);
	// B41C: bridge flash
	if (g.state24 < 2 && g.bridgeFlash) g.bridgeFlash--;
	g.frameCount++;
	if (g.frameCount == 0) {
		g.flags29 = lo(g.flags29 + 1) & 0xC7;
		if (!(g.flags29 & 7)) {
			g.idle++;
			if (g.idle == 0) g.idle = 0x80;
		}
	}
}

// B2A5-B3D8: the sound driver. Once per frame it sets the four POKEY channels from the
// game state (AUDCTL stays 0: 64 kHz clock, 17-bit noise):
//   channel 1: the engine (pitch from the speed $59, tone from the stick), or the low-fuel
//              warning, or the sound of $7A/$7B (refuel, bonus life...), or the start beep $C6;
//   channel 2: explosion noise ($7C) or the helicopter missile ($7F);
//   channel 3: firing ($7D) or the score sound ($7E);
//   channel 4: the tank shell in flight ($50).
void Game::soundDriver(const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	PokeyRegs &p = g.pokey;
	uint8_t a = 0, x = 0, y = 0;
	auto audc234 = [&](uint8_t v) { p.audc[1] = v; p.audc[2] = v; p.audc[3] = v; };
	if (g.c6) {                                   // B2A6: start beep
		g.c6--;
		p.audf[0] = 0x40;
		p.audc[0] = 0xAF;
		audc234(0);
		return;
	}
	if (g.state24 >= 2) {                         // B2BA: paused
		p.audc[0] = 0;
		audc234(0);
		return;
	}
	// channel 1 (a, x, y follow the original's registers: A is left over for AUDF2)
	bool writeF1 = true;                          // B34C writes AUDF1 and AUDC1, B34F only AUDC1
	y = 0x39;
	if (g.sound7A) {                              // B2D2: event sound, $7B frames long
		a = g.sound7B;
		if (g.sound7A <= 2) {
			if (g.sound7A == 2) y = 0x11;
			a = uint8_t((a >> 1) | 0x80);
		} else {
			y = g.sound7A == 3 ? 0x1F : 0x3F;
			a = lo(a + 0xA0);
		}
		x = a;
		if (--g.sound7B == 0) {
			g.sound7A = 0;
			a = 0;
		}
	} else if (g.deathTimer != 0) {               // B2F9: no plane: channel 1 off
		a = g.deathTimer;
		x = 0;
		writeF1 = false;
	} else {
		bool engine = true;
		if (g.fuel < 0x40) {                      // B2FD: low fuel: beeps every 63 frames
			y = g.sound7B ? g.sound7B : 0x3F;
			g.sound7B = --y;
			bool beep = false;
			if (g.fuel < 0x04) {                  // almost empty: a tone falling with the fuel
				a = uint8_t((g.fuel << 2) | (g.fuelLo >> 6));
				a = lo((a ^ 0xFF) + 0x20);
				a = uint8_t(a << 1);
				beep = a != 0;
			}
			if (!beep && y >= 0x1C) {             // first frames of each 63: the beep
				a = y;
				beep = true;
			}
			if (beep) {                           // B326
				a = uint8_t(a << 1);
				y = a;
				x = 0xAF;
				engine = false;
			}
		}
		if (engine) {                             // B32C: engine, pitch from the speed
			y = lo(0x1F - (g.speed >> 4));
			a = g.deathPitch;
			if (a >= 0x1B) {                      // $052C: falling pitch after a crash
				g.deathPitch--;
				y = a;
			}
			x = rom_.at(uint16_t(0xB4AC + (g.joystick & 3)));   // tone from up/down
			a = x;
		}
	}
	if (writeF1) p.audf[0] = y;
	p.audc[0] = x;
	// channel 2
	x = g.soundExplosion;
	if (x) {                                      // B352: explosion noise
		g.soundExplosion--;
		x = uint8_t((((x >> 1) + 4) & 0x0F) + 0x80);
		a = uint8_t(random() | 0x18);
	}
	y = g.soundMissile;
	if (y) {                                      // B367: helicopter missile
		g.soundMissile--;
		if (!attract(g)) {
			a = lo(y + 0x46);
			x = a;
			a = uint8_t(((lo((a ^ 0x0E) + 4)) & 0x0F) << 2);
		}
	}
	p.audf[1] = a;
	p.audc[1] = x;
	// channel 3
	x = 0;
	a = g.soundFire;
	if (a) {                                      // B386: firing
		g.soundFire--;
		a = lo(0x1C - a);
		const uint8_t t = uint8_t(a << 1);
		a = lo(uint8_t(t << 1) + t + (t >> 7));   // ASL, STA $04, ASL, ADC $04 (with the carry)
		x = 0xA8;
	}
	y = g.soundScore;
	if (y) {                                      // B399: score
		y--;
		g.soundScore = y;
		x = uint8_t((y & 0x0F) + 0xA0);
		a = rom_.at(uint16_t(0xB4E1 + (y >> 4)));
	}
	p.audf[2] = a;
	p.audc[2] = x;
	// channel 4
	x = 0;
	if (!attract(g) && !(s.v50 & 0x80) && s.v50 != 0) {   // B3B6: tank shell
		if (s.v50 < 0x11) {
			p.audf[3] = rom_.at(uint16_t(0xB4C0 + s.v50));
			x = 0xAC;
		} else {
			x = 0x8C;
			p.audf[3] = uint8_t(random() | 0x20);
		}
	}
	p.audc[3] = x;
}

}  // namespace rr
