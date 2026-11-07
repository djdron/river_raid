// River Raid game frame. See game.h. Addresses refer to the original cartridge code.
#include "game.h"

#include <cstring>

namespace rr {
namespace {
inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }
inline bool attract(const GameState &g) { return (g.attractFlags & 0x80) != 0; }
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
	for (int i = 0; i < 60; i++) g.panel[i] = rom_.at(uint16_t(0xBB70 + i));   // A0F0
	// the attract mode's new game: B0F1 with X = $29, AF80, then the respawn (A36E-A3E3) which
	// in the attract mode ends with sub $B21A; then frame 38
	g.attractFlags = 0x80;
	setUpGame();
	g.bulletY = 1;
	g.lives = g.other.lives = 4;
	g.respawned = 1;
	g.powerOnDone = 1;
	g.screenOn = 1;
	g.attractFlags = 0x81;
	g.attractTimer = 0xFF;
	g.planeState = 0xFF;
	g.speed = 0x80;
	g.fuel = 0xFF;
	g.fuelFrac = 0xFF;
	g.planeX = 0x7C;
	g.colourBank = bankColour(gen.s);
	// the scrolling line has been moving since frame 11 (VBI steps up to frame 38)
	g.scroller = Scroller();
	for (int f = 11; f <= 38; f++) {
		g.logoShown = g.scroller.logoX;
		g.scroller.step(true);
	}
	pushLine();
}

// A new river line goes above the others. Screen memory is a ring of two 80-line blocks,
// $2000-$2EFF and $3000-$3EFF; the address of the top line moves up by one line.
void Game::pushLine() {
	std::array<uint8_t, kLineBytes> line;
	gen.nextLine(line.data());
	screen.insert(screen.begin(), line);
	if (screen.size() > 256) screen.pop_back();
	uint8_t topLo = uint8_t(g.screenTop), topHi = uint8_t(g.screenTop >> 8);
	if (topLo < kLineBytes) topHi--;
	topLo = uint8_t(topLo - kLineBytes);
	if (topHi < 0x20) topHi = 0x3E;
	if (topHi == 0x2F) topHi = 0x2E;
	g.screenTop = uint16_t(topLo | (topHi << 8));
	if (afterLine) afterLine(line.data());
}

void Game::frame(const Input &input, const std::function<uint8_t()> &random) {
	if (busy_ > 0) {
		// sub $B0F1 is clearing memory: the main loop is stuck, only the interrupts run
		busy_--;
		if (busy_ == 1) g.screenOn = 1;           // screen memory clear: colours back on
		display();
		if (afterDisplay) afterDisplay();
		if (busy_ == 0) finishReset(input, random);
		vbi(random);
		return;
	}
	prepare();
	display();
	if (afterDisplay) afterDisplay();
	logic(input, random);
	vbi(random);
}

// AE99-AF38, at the top of the frame: shapes of the tank (player 1), the helicopter missile
// (missile 0), the tank shell (player 0) and the plane (player 2) into player-missile memory.
void Game::prepare() {
	RiverState &s = gen.s;
	auto copy = [&](uint16_t table, int index, uint16_t dest, int count) {   // sub $B00F
		const uint16_t src = rom_.word(uint16_t(table + ((2 * index) & 0xFF)));   // ASL A; TAX
		for (int y = count - 1; y >= 0; y--) g.pm.at(uint16_t(dest + y)) = rom_.at(uint16_t(src + y));
	};
	// tank: gone once it has scrolled below the screen
	const uint8_t tankY = s.tankY;
	if (s.tankY >= 0xBC) s.tankKind = s.tankY = 0;
	copy(0xB4D1, s.tankKind, uint16_t(0x0D00 + tankY), 11);
	g.shellColour = rom_.at(uint16_t(0xBAC5 + ((g.frameCount >> 3) & 7)));
	// helicopter missile: one row of missile 0, the rows above it cleared
	if (s.missileY >= 0x1C) {
		g.pm.at(uint16_t(0x0AFE + s.missileY)) &= 0xFC;
		g.pm.at(uint16_t(0x0AFF + s.missileY)) &= 0xFC;
		uint8_t m = g.pm.at(uint16_t(0x0B00 + s.missileY)) & 0xFC;
		if (s.missileX) m |= 0x03;
		g.pm.at(uint16_t(0x0B00 + s.missileY)) = m;
	}
	// tank shell
	const uint8_t shellY = s.shellY;
	if (s.shellY >= 0xBC) {
		s.shellTimer = 0xFF;
		g.shellFrame = 0;
		s.shellY = 0;
	}
	copy(0xBAB7, g.shellFrame, uint16_t(0x0C00 + shellY), 16);
	// plane
	copy(0xBACD, g.planeFrame, 0x0EAA, 14);
	// AF3A-AF58: lives, bridges and score into the panel
	if (g.attractTimer == 0) showLives();
	showBridges();
	if (g.player == 0) showScore();
	else showOtherScore();
}

void Game::logic(const Input &input, const std::function<uint8_t()> &random) {
	logicPass(input, random);
}

// The main loop continues after sub $B0F1 (13 frames after it started).
void Game::finishReset(const Input &input, const std::function<uint8_t()> &random) {
	const Resume r = resume_;
	resume_ = Resume::None;
	if (r == Resume::Respawn) {
		finishRespawn();
		logicTail(input, random);
	} else if (r == Resume::NewGame) {
		// AF80, A16C-A17E: the panel, then the main loop runs once more in this frame
		setUpGame();
		if (!attract(g)) {
			showLives();
			showBridges();
			showScore();
		}
		logicPass(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::logicPass(const Input &input, const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
	                    input.start || input.select || input.option;
	if (active) {
		g.idleTime = 0;
		g.pauseState = 0;
	}
	// A1C5-A1F3: pause with any keyboard key: press (3), release (2: paused), press (1), release (0)
	const uint8_t key = input.key ? 0 : 4;   // SKSTAT bit 2
	if (!key) g.idleTime = 0;
	switch (g.pauseState) {
	case 0:
		if (!key) { g.pauseState = 3; return; }
		break;
	case 1:
		if (key) g.pauseState--;
		break;
	case 2:
		if (key) return;
		g.pauseState--;
		break;
	default:
		if (!key) return;
		g.pauseState--;
		return;
	}
	if (g.idleTime & 0x80) return;   // A1F5: screen off after a long time without input

	// A1F9: objects move on alternate frames, odd and even slots in turn
	s.vB1 = g.frameCount ^ s.slotParity;   // $B1
	// A1FF: every 16 frames a random slot is woken up (bit 6: it may move)
	if ((g.frameCount & 0x0F) == 0) {
		uint8_t x = random() & 0x0F;
		if (x >= 0x0B) x = lo(x - 5);
		if (x != s.enteringSlot) s.slot[x].flags |= 0x40;
	}
	tankOnRoad();
	moveObjects();

	// A2F5: attract timer, -1 every 8 frames; when it runs out after a game, the attract mode
	// starts (with a new game 1 river)
	if (g.attractTimer && !(g.frameCount & 7)) {
		uint8_t x = lo(g.attractTimer - 1);
		if (x == 0) {
			x = 0xFF;
			if (!attract(g)) {
				g.attractFlags |= 0x80;
				g.attractTimer = x;
				newGame();
				return;
			}
		}
		g.attractTimer = x;
	}
	// A312: joystick of the player whose turn it is
	g.joystickPrev = lo(g.joystick << 4);
	g.joystick = input.stick[g.player & 1] & 0x0F;
	if (g.joystick != 0x0F || input.trigger[g.player & 1]) {
		if (g.planeState == 0x2F) {               // A332: waiting plane: go
			g.planeState = 0;
			g.speed = 0x40;
		}
	}
	// A33F: the plane's state
	const uint8_t t = g.planeState;
	if (t & 0x80) {                               // A35C: exploding (or no plane: $FF)
		if (t != 0xFF) {
			if (!(lo(t + 1) & 7) && g.planeFrame) g.planeFrame--;
			g.planeState--;
			if (!(g.planeState & 0x80) && respawnOrGameOver()) return;
		}
	} else if (t == 0) {
		alive(input);
	} else if (t >= 0x30) {                       // A348: river refilling
		if (t == 0x30) {
			g.crashPitch = 0x80;
			g.lives--;
		}
		g.planeState--;
	}
	logicTail(input, random);
}

// A5B7-A6EA: the rest of the main loop.
void Game::logicTail(const Input &input, const std::function<uint8_t()> &) {
	helicopterMissile();                          // A5B7
	tankShell();                                  // A607

	// A673: console keys
	if (input.start) {
		g.startBeep = 2;
		g.attractFlags = 0;
		g.attractTimer = 0;
		newGame();
		return;
	}
	if (input.select || input.option) {
		if (g.selectDelay-- == 0) selectGame();   // A699: every 30 frames while held
	} else {
		g.selectDelay = 0;
	}

	// A6DE: scrolling
	if (g.planeState & 0x80) {
		if (!attract(g)) return;                  // exploding: the river stops
		g.speed = g.gameNumber < 4 ? 0x80 : 0xFE; // A6F1: attract mode
	} else if (g.planeState == 0x2F) {            // waiting for the player: plane level, no scroll
		g.planeFrame = 9;
		return;
	}
	scrollRiver();
}

// A688-A17E: a new game (START), or the attract mode's river starting.
void Game::newGame() {
	g.player = g.gameNumber & 1;
	// A15D with X = $79: $79-$7F cleared
	g.respawned = 0;
	g.eventSound = g.eventSoundTime = g.soundExplosion = g.soundFire = g.soundScore = g.soundMissile = 0;
	reset(true);   // the rest (AF80, A17E) when the reset ends: finishReset()
	resume_ = Resume::NewGame;
}

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the table at
// $BAE1: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = gen.s;
	const uint8_t level = s.level;
	uint8_t save[4];
	std::memcpy(save, s.checkpoint, 4);
	auto table = [&](int i) { return rom_.at(uint16_t(0xBAE1 + i)); };
	if (full) s.level = table(0x13);   // loaded before B16B reads it for the bank colour
	gen.restoreCheckpoint();      // $4D-$5F of the river, $2B-$4C and $0500-$05FF cleared, bridge in slot 10
	// restoreCheckpoint also continues from the checkpoint (A3C8); here only the reset part
	// counts: the checkpoint is applied by the caller
	(void)level;
	(void)save;
	g.bulletY = table(0x09);      // $56
	g.planeX = table(0x0A);       // $57
	g.planeXFrac = table(0x0B);   // $58
	g.speed = table(0x0C);        // $59
	g.fuel = table(0x0E);         // $5B
	g.fuelFrac = table(0x0F);       // $5C
	g.planeFrame = table(0x11);   // $5E
	if (full) {
		g.bridges[0] = table(0x14); g.bridges[1] = table(0x15);
		for (int i = 0; i < 3; i++) g.score[i] = table(0x16 + i);
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(0x19 + i);
		g.lives = table(0x1D);
		g.other.level = table(0x1E);
		g.other.bridges[0] = table(0x1F); g.other.bridges[1] = table(0x20);
		for (int i = 0; i < 3; i++) g.other.score[i] = table(0x21 + i);
		for (int i = 0; i < 4; i++) g.other.checkpoint[i] = table(0x24 + i);
		g.other.lives = table(0x28);
		g.planeState = table(0x29);
	}
	// $2B-$4C cleared (the river's part in restoreCheckpoint)
	g.planeAccel = 0;
	g.scrollFrac = 0;
	g.bridgeFlash = 0;
	g.shellFrame = g.shellX = g.shellXFrac = 0;
	g.crashPitch = 0;   // $052C ($0500-$05FF cleared)
	// player-missile memory rows 1-$BB cleared, screen memory cleared
	for (int page = 0; page < 5; page++)
		for (int row = 1; row < 0xBC; row++) g.pm.bytes[size_t(page * 0x100 + row)] = 0;
	screen.clear();
	g.screenOn = 0;                               // B101: colours off while clearing
	busy_ = kResetFrames;
	for (int i = 0; i < 7; i++) {                 // rainbow logo (players 2 and 3), NTSC rows
		g.pm.at(uint16_t(0x0EDB + i)) = 0xFF;
		g.pm.at(uint16_t(0x0FDB + i)) = 0xFC;
	}
	for (int i = 0; i < 10; i++) g.pm.at(uint16_t(0x0BCC + i)) = 0x0C;   // fuel needle (missile 1)
}

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() {
	if (g.gameNumber & 1) {
		showOtherScore();
		g.panel[0x0B] = 0x4F;
		g.panel[0x12] = 0x50;
	} else {
		showHighScore();
		g.panel[0x0B] = 0xDB;
		g.panel[0x12] = 0xD0;
	}
	const int v = g.gameNumber >> 1;
	for (int i = 0; i < 4; i++) {
		gen.s.checkpoint[i] = rom_.at(uint16_t(0xB494 + 4 * i + v));
		g.other.checkpoint[i] = gen.s.checkpoint[i];
	}
	gen.s.level = g.other.level = rom_.at(uint16_t(0xB4A4 + v));
	g.bridges[1] = g.other.bridges[1] = rom_.at(uint16_t(0xB4A8 + v));
	g.bridges[0] = g.other.bridges[0] = 0;
}

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	g.gameNumber = (g.gameNumber + 1) & 7;
	g.panel[0x2A] = 0;
	g.panel[0x2B] = lo(g.gameNumber + 0xD1);
	g.idleTime = 0;
	g.attractFlags = 0;
	g.lives = 0;
	g.other.lives = 0;
	g.player = 0;
	g.startBeep = 2;
	for (int i = 0; i < 3; i++) g.score[i] = g.other.score[i] = 0;
	g.soundMissile = 0;
	gen.s.shellTimer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	g.attractTimer = 0x25;
	g.selectDelay = 0x1E;
}

// A36E-A3B6: the explosion is over: the next player, the next plane, or game over.
// Returns true when the river is reset for the next plane (the loop continues after it).
bool Game::respawnOrGameOver() {
	if (g.other.lives) swapPlayers();
	g.respawned = 1;
	if (g.lives) {
		reset(false);                             // A3BC; then finishRespawn()
		resume_ = Resume::Respawn;
		return true;
	}
	// A37D: game over
	updateHighScore(false);
	updateHighScore(true);
	if (g.gameNumber & 1) {
		if (g.player) swapPlayers();
		for (int i = 0; i < 4; i++) g.panel[0x33 + i] = rom_.at(uint16_t(0xBB8F + i)) ^ 0xC0;
		printBcd(g.other.bridges, 2, 0x37, 0x50);   // B064 with $B1 = $50, X = $6C, Y = $37
	} else {
		showHighScore();
	}
	// (In the original the high score check makes this frame's work end close to the vertical
	// blank: depending on the objects on screen, sub $B21A sets $18 = $FF just before or just
	// after that frame's VBI counts it, or even in the next frame. The port does it before the
	// VBI, and the "Game over" text address (A3B4) after it, as seen most often.)
	gameOverScreen();
	gameOverText_ = true;                         // A3B4, after this frame's VBI (see vbi())
	return false;
}

// A3C1-A3E3: the river continues from the restart point, the plane in the middle of it.
void Game::finishRespawn() {
	RiverState &s = gen.s;
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
	g.planeX = lo(s.bank + 0x26 + s.islandWidth);
	if (attract(g)) gameOverScreen();
}
// A21B-A26A: the tank on the road drives towards the river, every 16 frames (every 4 for
// kinds 4-7), stops at the edge and fires; its kind animates through the table $BAAF.
void Game::tankOnRoad() {
	RiverState &s = gen.s;
	const uint8_t mask = (s.tankKind >= 4 && s.tankKind < 8) ? 0x03 : 0x0F;
	if (mask & g.frameCount) return;
	bool move = false;
	if (s.shellTimer & 0x80) {                          // no shell in flight
		if (attract(g) || g.planeState == 0) {
			if (s.tankFires & 0x80) {                  // tank that fires
				if (s.tankY >= 0x1A) {
					if (g.hit.p1pf & 0x80) move = true;   // not at the river edge yet
					else s.shellTimer++;                          // at the edge: fire ($50 = 0)
				}
			} else if (s.bridgeHit == 0) {
				move = true;
			}
		}
	}
	if (move) {
		if ((s.tankKind & 0x7F) < 6) s.tankX++;
		else s.tankX--;
	}
	s.tankKind = (s.tankKind & 0x80) ? 0x03 : rom_.at(uint16_t(0xBAAF + s.tankKind));
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
		if (!attract(g) && g.planeState != 0) continue;   // nothing moves while the plane respawns
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
	const int x = s.shooterSlot;
	if (x < kSlots && (s.slot[x].kind == 0x0A || s.slot[x].kind == 0x0B)) {
		if (s.missileX) {                                      // flying: until it hits the bank
			if (g.hit.m0pf & 0x80) y = lo(s.missileX + g.missileStep);
		} else {
			if (!attract(g) && g.planeState != 0) { s.missileX = 0; return; }
			const uint8_t dy = lo(s.slot[x].y - 0x3C);
			if (dy < 0x1C) return;                        // too high on the screen: wait
			s.missileY = dy;
			g.soundMissile = 9;
			y = lo(s.slot[x].x - 3);
			if (s.slot[x].flags & 0x08) {
				g.missileStep = 0xFE;
			} else {
				g.missileStep = 0x02;
				y = lo(y + 10);                           // ADC #$09 with carry set
			}
		}
	}
	s.missileX = y;
}

// A607-A671: the tank's shell flies from the tank across the river ($50 counts 0..$38).
void Game::tankShell() {
	RiverState &s = gen.s;
	uint8_t frame = 0;
	if (!(s.shellTimer & 0x80)) {
		bool step = true;
		if (s.shellTimer == 0) {                                 // A60F: launch
			if (s.tankY < 0x19) {
				step = false;
			} else {
				s.shellY = lo(s.tankY - 6);
				g.shellXFrac = 0;
				if (s.tankKind < 6) {                     // facing right: ADC #9 (carry clear), SBC #4
					const int sum = s.tankX + 9;
					g.shellX = lo(sum - 4 - (sum > 0xFF ? 0 : 1));
				} else {
					g.shellX = lo(s.tankX - 4);
				}
				frame = 1;
			}
		} else if (s.shellTimer < 0x11) {                       // A62A: flight
			s.shellY = lo(s.shellY + rom_.at(uint16_t(0xB4AF + s.shellTimer)));
			if (s.tankKind < 6) {
				const int sum = g.shellXFrac + s.shellSpeedFrac;
				g.shellXFrac = lo(sum);
				g.shellX = lo(g.shellX + s.shellSpeed + (sum >> 8));
			} else {
				const int diff = g.shellXFrac - s.shellSpeedFrac;
				g.shellXFrac = lo(diff);
				g.shellX = lo(g.shellX - s.shellSpeed - (diff < 0 ? 1 : 0));
			}
			frame = 1;
		} else {                                          // A657: explosion frames
			frame = lo(((s.shellTimer - 0x11) >> 3) + 2);
		}
		if (step) {
			uint8_t t = lo(s.shellTimer + 1);                    // A65E
			if (t >= 0x39) {
				t = g.planeState ? 0xFF : 0;
				frame = 0;
			}
			s.shellTimer = t;
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
		pushLine();
	}
}

// B2A5-B3F0: vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	soundDriver(random);
	g.frameCount++;
	if (g.frameCount == 0) {
		g.attractFlags = lo(g.attractFlags + 1) & 0xC7;
		if (!(g.attractFlags & 7)) {
			g.idleTime++;
			if (g.idleTime == 0) g.idleTime = 0x80;
		}
	}
	// B3F2-B43E: colours for the next frame. Water flashes red while a hit bridge flashes ($31);
	// while sub $B0F1 clears the screen ($C7 = 0) the playfield colours are off.
	g.colourWater = 0x84;
	if (g.pauseState < 2 && g.bridgeFlash) {
		if (g.bridgeFlash & 1) g.colourWater = 0x42;
		g.bridgeFlash--;
	}
	g.colourEdge = 0x20;
	g.colourBank = bankColour(gen.s);              // sub $AFD0
	if (!g.screenOn) g.colourBank = g.colourEdge = g.colourWater = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played ($2A != 0)
	g.logoShown = g.scroller.logoX;
	g.scroller.step(g.attractTimer != 0);
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		g.scroller.textLo = 0x20;
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
	if (g.startBeep) {                                   // B2A6: start beep
		g.startBeep--;
		p.audf[0] = 0x40;
		p.audc[0] = 0xAF;
		audc234(0);
		return;
	}
	if (g.pauseState >= 2) {                         // B2BA: paused
		p.audc[0] = 0;
		audc234(0);
		return;
	}
	// channel 1 (a, x, y follow the original's registers: A is left over for AUDF2)
	bool writeF1 = true;                          // B34C writes AUDF1 and AUDC1, B34F only AUDC1
	y = 0x39;
	if (g.eventSound) {                              // B2D2: event sound, $7B frames long
		a = g.eventSoundTime;
		if (g.eventSound <= 2) {
			if (g.eventSound == 2) y = 0x11;
			a = uint8_t((a >> 1) | 0x80);
		} else {
			y = g.eventSound == 3 ? 0x1F : 0x3F;
			a = lo(a + 0xA0);
		}
		x = a;
		if (--g.eventSoundTime == 0) {
			g.eventSound = 0;
			a = 0;
		}
	} else if (g.planeState != 0) {               // B2F9: no plane: channel 1 off
		a = g.planeState;
		x = 0;
		writeF1 = false;
	} else {
		bool engine = true;
		if (g.fuel < 0x40) {                      // B2FD: low fuel: beeps every 63 frames
			y = g.eventSoundTime ? g.eventSoundTime : 0x3F;
			g.eventSoundTime = --y;
			bool beep = false;
			if (g.fuel < 0x04) {                  // almost empty: a tone falling with the fuel
				a = uint8_t((g.fuel << 2) | (g.fuelFrac >> 6));
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
			a = g.crashPitch;
			if (a >= 0x1B) {                      // $052C: falling pitch after a crash
				g.crashPitch--;
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
	if (!attract(g) && !(s.shellTimer & 0x80) && s.shellTimer != 0) {   // B3B6: tank shell
		if (s.shellTimer < 0x11) {
			p.audf[3] = rom_.at(uint16_t(0xB4C0 + s.shellTimer));
			x = 0xAC;
		} else {
			x = 0x8C;
			p.audf[3] = uint8_t(random() | 0x20);
		}
	}
	p.audc[3] = x;
}

// A3E9-A5B4: the plane is flying.
void Game::alive(const Input &input) {
	RiverState &s = gen.s;
	// A3E9: the plane touched an object
	const uint8_t hitSlot = g.hit.p2p3;
	if (!(hitSlot & 0x80)) {
		const uint8_t kind = s.slot[hitSlot].kind;
		if (kind >= 7) {
			if (kind <= 0x0E) {                   // enemy or bridge: both explode
				if (kind == 0x0E) s.bridgeHit++;
				crash(1, 0x1F);
				explode(hitSlot, 3);
				return;
			}
			// A40A: fuel depot: +2 per frame (ADC #1 with the carry of CMP #$0E)
			int f = g.fuel + 2;
			uint8_t sound = 4;
			if (f > 0xFF) {
				f = 0xFF;
				g.fuelFrac = 0xFF;
				sound = 3;                        // tank full
			}
			g.fuel = lo(f);
			if (sound != g.eventSound) {
				g.eventSound = sound;
				g.eventSoundTime = 8;
			}
		}
	}
	// A424: tank shell exploding on the plane, helicopter missile, the bank
	if ((!(g.hit.p2p0 & 0x80) && s.shellTimer >= 0x11) || !(g.hit.m0p2 & 0x80) || !(g.hit.p2pf & 0x80)) {
		crash(1, 0x1F);
		return;
	}
	// A43F: fuel, $20 / 256 per frame
	{
		const int f = g.fuelFrac - 0x20;
		if (f < 0) {
			if (g.fuel == 0) {                    // out of fuel
				crash(2, 0x23);
				return;
			}
			g.fuel--;
		}
		g.fuelFrac = lo(f);
	}
	// A455: steering. Left/right accelerate ($2B += 8 per frame up to $F8); the position moves
	// when the fraction $58 overflows. Frames 7/8/9: plane banking left / right / level.
	const uint8_t stick = g.joystick;
	uint8_t frame = 9;
	if (!((stick & 0x0C) ^ 0x0C)) {
		g.planeAccel = 0;
		g.planeXFrac = 0;
	} else {
		const int acc = g.planeAccel + 8;
		const bool full = acc > 0xFF;             // carry: already at full speed
		if (!full) g.planeAccel = lo(acc);
		frame = 8;
		if (stick & 0x08) {                       // right not pressed: left
			frame = 7;
			if (full) {
				g.planeX--;
				g.planeXFrac = 0x08;              // (A is left at $08 by AND #$08)
			} else {
				const int d = g.planeXFrac - g.planeAccel;
				if (d < 0) g.planeX--;
				g.planeXFrac = lo(d);
			}
		} else {                                  // right
			if (full) {
				g.planeX++;
				g.planeXFrac = 0;                 // (A is left at 0 by AND #$08)
			} else {
				const uint8_t start = (g.joystickPrev & 0x80) ? 0xFF : g.planeXFrac;   // just pressed: move now
				const int sum = start + g.planeAccel;
				if (sum > 0xFF) g.planeX++;
				g.planeXFrac = lo(sum);
			}
		}
	}
	g.planeFrame = frame;
	// A497: speed: up +2 (up to $FE), down -2 (down to $40), neither: back towards $80 by 2
	{
		const uint8_t v = g.speed;
		bool faster;
		if (!(stick & 0x01)) faster = true;
		else if (!(stick & 0x02)) faster = false;
		else if (v == 0x80) faster = false, (void)0;
		else faster = v < 0x80;
		if (!((stick & 0x03) == 0x03 && v == 0x80)) {
			if (faster) {
				if (v + 2 <= 0xFF) g.speed = lo(v + 2);
			} else if (v >= 0x41) {
				g.speed = lo(v - 2);
			}
		}
	}
	// A4B6: the bullet hit an object
	const uint8_t shot = g.hit.m2p3;
	if (!(shot & 0x80) && s.slot[shot].kind >= 7) {
		explode(shot, 2);
		return;
	}
	// A4C1: the tank shell's explosion destroys an object
	const uint8_t shelled = g.hit.p3p0;
	if (!(shelled & 0x80) && s.shellTimer >= 0x11 && s.slot[shelled].kind >= 8) {
		s.slot[shelled].kind = 2;
		g.soundExplosion = 0x17;
	}
	// A58D: the bullet: stops at the bank, flies up 6 lines per frame, or is fired
	if (!(g.hit.m2pf & 0x80)) {
		moveBullet(input, 1);
	} else if (g.bulletY >= 0x1C) {
		moveBullet(input, g.bulletY - 6);
	} else if (input.trigger[g.player & 1]) {
		g.soundFire = 0x0F;
		moveBullet(input, 0xA9);
	} else {
		moveBullet(input, 1);
	}
}

// A4E0-A589: object in `slot` explodes (kind 2 or 3), score.
void Game::explode(int slot, uint8_t kind) {
	RiverState &s = gen.s;
	Slot &sl = s.slot[size_t(slot)];
	sl.kind = kind;
	uint8_t type = sl.type;
	g.soundExplosion = 0x17;
	if (type == 0x0E) {                           // a bridge
		g.bridgeFlash = 0x17;
		// bridge counter +1 (BCD)
		int c = 1;
		auto bcdAdd = [](uint8_t a, uint8_t b, int &carry) {
			int lo4 = (a & 0x0F) + (b & 0x0F) + carry;
			int hi4 = (a >> 4) + (b >> 4);
			if (lo4 > 9) { lo4 -= 10; hi4++; }
			carry = 0;
			if (hi4 > 9) { hi4 -= 10; carry = 1; }
			return uint8_t((hi4 << 4) | lo4);
		};
		g.bridges[1] = bcdAdd(g.bridges[1], 0, c);
		g.bridges[0] = bcdAdd(g.bridges[0], 0, c);
		s.bridgeHit++;
		// A500: a tank on the bridge goes with it
		const uint8_t left = lo(sl.x - 8);
		if (left < s.tankX && lo(sl.x + 0x1F) >= s.tankX) {
			s.tankKind |= 0x80;
			g.soundExplosion = 0x37;
			type = 0x10;
		} else if (g.bridges[0] != 0 || g.bridges[1] >= 5) {
			// A52B: from bridge 5 on, a tank close to the bridge fires
			bool near;
			if (s.tankKind < 6) near = lo(sl.x - s.tankX) < 0x29;
			else near = lo(s.tankX - sl.x) < 0x41;
			if (near) s.shellTimer++;
		}
	}
	// A547: score (BCD), points from the table at $BB0B by type
	{
		const uint8_t thousands = g.score[1] & 0xF0;
		s.vB1 = thousands;                        // $B1
		int c = 0;
		auto bcdAdd = [](uint8_t a, uint8_t b, int &carry) {
			int lo4 = (a & 0x0F) + (b & 0x0F) + carry;
			int hi4 = (a >> 4) + (b >> 4);
			if (lo4 > 9) { lo4 -= 10; hi4++; }
			carry = 0;
			if (hi4 > 9) { hi4 -= 10; carry = 1; }
			return uint8_t((hi4 << 4) | lo4);
		};
		g.score[2] = bcdAdd(g.score[2], rom_.at(uint16_t(0xBB0B + type)), c);
		g.score[1] = bcdAdd(g.score[1], 0, c);
		g.score[0] = bcdAdd(g.score[0], 0, c);
		if (c) {                                  // past 999 999: the game ends
			g.score[0] = g.score[1] = g.score[2] = 0x99;
			g.lives = 0;
			crash(1, 0x1F);
			return;
		}
		if ((g.score[1] & 0xF0) != thousands && g.lives != 9) {   // every 10 000 points
			g.lives++;
			g.soundScore = 0x40;
		}
	}
	Input none;
	moveBullet(none, 1);                          // A589
}

// sub $B1C0: the plane crashes. sound/length: channel 1 event sound ($7A/$7B).
void Game::crash(uint8_t sound, uint8_t length) {
	RiverState &s = gen.s;
	g.eventSoundTime = length;
	g.eventSound = sound;
	// where the next plane starts: after a destroyed bridge the next section's start
	if ((s.slot[10].flags ^ s.blockFlags) & 0x04) {
		if (s.bridgeHit) {
			if (s.blockFlags & 0x80) gen.sectionCheckpoint();
		} else if (!(s.blockFlags & 0x80)) {
			uint8_t lvl = lo(s.level - 1);
			if (lvl == 0x4E) lvl = 0x50;
			s.level = lvl;
			std::memcpy(s.checkpoint, s.prevCheckpoint, 4);
		}
	}
	g.planeFrame = 6;
	// B200
	g.planeState = 0xFE;
	g.attractTimer = 0;
	Input none;
	clearBullet();
	s.shooterSlot = 0x0B;
	g.pm.at(uint16_t(0x0B00 + s.missileY)) &= 0xFC;
	g.bulletY = 1;
	(void)none;
}

// sub $B21A: no plane (game over or attract mode).
void Game::gameOverScreen() {
	RiverState &s = gen.s;
	g.frameCount = 0xFF;
	g.planeState = 0xFF;
	g.attractTimer = 0xFF;
	clearBullet();
	s.shooterSlot = 0x0B;
	g.pm.at(uint16_t(0x0B00 + s.missileY)) &= 0xFC;
	g.bulletY = 1;
}

// sub $B221 / $B223: the bullet is 8 rows of missile 2 (bits $20) from row $56; at least row $1C.
void Game::drawBullet(uint8_t bits) {
	int x = g.bulletY - 1;
	if (x == 0) return;
	if (x < 0x1B) x = 0x1B;
	for (int i = 1; i <= 8; i++) g.pm.at(uint16_t(0x0B00 + x + i)) = bits;
}
void Game::clearBullet() { drawBullet(0); }

// A5AA-A5B4: the bullet to row y.
void Game::moveBullet(const Input &, int y) {
	clearBullet();
	g.bulletY = lo(y);
	drawBullet(0x20);
}

// sub $B247: two-player game: the other player's turn.
void Game::swapPlayers() {
	if (!(g.gameNumber & 1)) return;
	g.player ^= 1;
	RiverState &s = gen.s;
	PlayerData cur;
	cur.level = s.level;
	std::memcpy(cur.bridges, g.bridges, 2);
	std::memcpy(cur.score, g.score, 3);
	std::memcpy(cur.checkpoint, s.checkpoint, 4);
	cur.lives = g.lives;
	s.level = g.other.level;
	std::memcpy(g.bridges, g.other.bridges, 2);
	std::memcpy(g.score, g.other.score, 3);
	std::memcpy(s.checkpoint, g.other.checkpoint, 4);
	g.lives = g.other.lives;
	g.other = cur;
}

// sub $B261: the score ($63 or the other player's $6E) becomes the high score of this pair of
// games if higher; then the check bytes $90-$9F are recomputed.
void Game::updateHighScore(bool otherPlayer) {
	const uint8_t *score = otherPlayer ? g.other.score : g.score;
	uint8_t *hi = &g.highScore[(g.gameNumber & 0x0E) * 2];
	for (int i = 0; i < 3; i++) {
		if (score[i] < hi[i]) return;
		if (score[i] > hi[i]) break;
	}
	std::memcpy(hi, score, 3);
	for (int y = 15; y >= 0; y--) {               // sub $B298
		int a = 0, c = 0;
		for (int x = 14; x >= 0; x--) {
			const int r = a + g.highScore[x] + c;
			c = r > 0xFF;
			a = (r & 0xFF) ^ y;
		}
		g.highScoreCheck[y] = uint8_t(a);
	}
}

// sub $AF5B: lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() {
	int x = 0;
	int y = g.lives;
	if (y) {
		bool show = true;
		if (g.soundScore && (g.soundScore & 0x08)) {
			if (--y == 0) show = false;
		}
		if (show)
			do { g.panel[0x33 + x++] = 0x9C; } while (--y);
	}
	while (x < 9) g.panel[0x33 + x++] = 0;
}

// B086-B0C6: BCD bytes as digits in the panel, leading zeros blank; nothing in the attract mode.
void Game::printBcd(const uint8_t *d, int bytes, int at, uint8_t base) {
	gen.s.vB1 = base;   // $B1 holds the base character
	if (attract(g)) return;
	int y = at;
	bool leading = true;
	for (int i = 0; i < bytes; i++) {
		const uint8_t digits[2] = {uint8_t(d[i] >> 4), uint8_t(d[i] & 0x0F)};
		for (uint8_t v : digits) {
			if (leading && v == 0) {
				g.panel[y++] = 0;
			} else {
				leading = false;
				g.panel[y++] = lo(v + base);
			}
		}
	}
}
void Game::showBridges() { printBcd(g.bridges, 2, 0x23, 0x90); }
void Game::showScore() { printBcd(g.score, 3, 0x02, 0x10); }
void Game::showOtherScore() { printBcd(g.score, 3, 0x0C, 0x50); }
void Game::showHighScore() { printBcd(&g.highScore[(g.gameNumber & 0x0E) * 2], 3, 0x0C, 0xD0); }

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	gen.s = RiverState();
	gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	g = GameState();
	screen.clear();
	for (int i = 0; i < 60; i++) g.panel[i] = rom_.at(uint16_t(0xBB70 + i));
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	g.powerOnDone = 1;
	g.attractTimer = 1;
	g.planeState = 0xFF;
}

}  // namespace rr
