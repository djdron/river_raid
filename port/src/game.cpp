// River Raid game frame. See game.h. Addresses refer to the original cartridge code.
#include "game.h"

#include <cstring>

namespace rr {
namespace {

inline uint8_t lo(int v) { return uint8_t(v & 0xFF); }

// Player-missile memory: missiles, then players 0-3, one byte per scanline.
constexpr uint16_t kMissileMemory = 0x0B00;
constexpr uint16_t kShellMemory = 0x0C00;      // player 0: the tank shell
constexpr uint16_t kTankMemory = 0x0D00;       // player 1: the tank on the road
constexpr uint16_t kPlaneMemory = 0x0EAA;      // player 2 from row $AA: the plane
constexpr uint8_t kMissile0 = 0x03;            // the helicopter's missile
constexpr uint8_t kBulletBits = 0x20;          // the plane's bullet (missile 2, one bit wide)

// GameState::planeState
constexpr uint8_t kFlying = 0;
constexpr uint8_t kWaiting = 0x2F;             // at the bridge, until the player moves
constexpr uint8_t kLifeLost = 0x30;            // the river has been refilled
constexpr uint8_t kCrashed = 0xFE;             // the explosion starts; down to $80
constexpr uint8_t kNoPlane = 0xFF;             // attract mode, game over

// GameState::pauseState: a key press pauses, the next press resumes
constexpr uint8_t kPlaying = 0, kResuming = 1, kPaused = 2, kPausing = 3;

// GameState::eventSound
constexpr uint8_t kCrashSound = 1, kOutOfFuelSound = 2, kTankFullSound = 3, kRefuelSound = 4;

// panel character positions (40 per row, the second row from $28)
constexpr int kPanelScore = 0x02;
constexpr int kPanelPlayer1Mark = 0x0B, kPanelPlayer2Mark = 0x12;
constexpr int kPanelHighScore = 0x0C;          // or the other player's score
constexpr int kPanelBridges = 0x23;
constexpr int kPanelGameNumber = 0x2A;
constexpr int kPanelLives = 0x33;              // 9 plane symbols; "GAME OVER" text of player 2 there
constexpr int kPanelOtherBridges = 0x37;
constexpr uint8_t kLifeSymbol = 0x9C;
// digit characters for printBcd: '0' in each of the panel's colours
constexpr uint8_t kDigitsScore = 0x10, kDigitsOther = 0x50, kDigitsBridges = 0x90, kDigitsHighScore = 0xD0;

inline bool attract(const GameState &g) { return (g.attractFlags & 0x80) != 0; }

// One byte of BCD addition with carry in and out.
uint8_t addBcd(uint8_t a, uint8_t b, int &carry) {
	int low = (a & 0x0F) + (b & 0x0F) + carry;
	int high = (a >> 4) + (b >> 4);
	if (low > 9) { low -= 10; high++; }
	carry = 0;
	if (high > 9) { high -= 10; carry = 1; }
	return uint8_t((high << 4) | low);
}

}  // namespace

Game::Game(const Rom &rom) : gen(rom), rom_(rom) {}

// sub $B0F1 leaves the rainbow under the logo (players 2 and 3, rows $DB-$E1) and the fuel
// gauge needle (missile 1, rows $CC-$D5) in player-missile memory; NTSC rows.
void Game::drawLogoAndNeedle() {
	for (int i = 0; i < 7; i++) {
		g.pm.at(uint16_t(0x0EDB + i)) = 0xFF;
		g.pm.at(uint16_t(0x0FDB + i)) = 0xFC;
	}
	for (int i = 0; i < 10; i++) g.pm.at(uint16_t(kMissileMemory + 0xCC + i)) = 0x0C;
}

void Game::startAttract(bool pal) {
	gen.s = RiverState();
	gen.startGame(1, pal);
	g = GameState();
	screen.clear();
	drawLogoAndNeedle();
	for (int i = 0; i < kPanelBytes; i++) g.panel[i] = rom_.panelText(i);   // A0F0
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
	g.planeState = kNoPlane;
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

// AE99-AF38, at the top of the frame: the shapes of the tank (player 1), the helicopter
// missile (missile 0), the tank shell (player 0) and the plane (player 2) into player-missile
// memory; then lives, bridges and score into the panel.
void Game::prepare() {
	RiverState &s = gen.s;
	auto copyShape = [&](uint16_t shape, uint16_t dest, int rows) {   // sub $B00F
		for (int row = rows - 1; row >= 0; row--) g.pm.at(uint16_t(dest + row)) = rom_.at(uint16_t(shape + row));
	};
	// the tank: gone once it has scrolled below the screen
	const uint8_t tankY = s.tankY;
	if (s.tankY >= 0xBC) s.tankKind = s.tankY = 0;
	copyShape(rom_.tankShape(s.tankKind), uint16_t(kTankMemory + tankY), 11);
	g.shellColour = rom_.shellColour(g.frameCount >> 3);
	// the helicopter's missile: one row of missile 0, the two rows above it cleared
	if (s.missileY >= 0x1C) {
		const uint16_t row = uint16_t(kMissileMemory + s.missileY);
		g.pm.at(uint16_t(row - 2)) &= ~kMissile0;
		g.pm.at(uint16_t(row - 1)) &= ~kMissile0;
		uint8_t bits = g.pm.at(row) & ~kMissile0;
		if (s.missileX) bits |= kMissile0;
		g.pm.at(row) = bits;
	}
	// the tank shell: gone below the screen
	const uint8_t shellY = s.shellY;
	if (s.shellY >= 0xBC) {
		s.shellTimer = 0xFF;
		g.shellFrame = 0;
		s.shellY = 0;
	}
	copyShape(rom_.shellShape(g.shellFrame), uint16_t(kShellMemory + shellY), 16);
	copyShape(rom_.planeShape(g.planeFrame), kPlaneMemory, 14);
	// AF3A-AF58: lives, bridges and score into the panel
	if (g.attractTimer == 0) showLives();
	showBridges();
	if (g.player == 0) showScore();
	else showOtherScore();
}

void Game::logic(const Input &input, const std::function<uint8_t()> &random) { mainLoop(input, random); }

// The main loop continues after sub $B0F1 (13 frames after it started).
void Game::finishReset(const Input &input, const std::function<uint8_t()> &random) {
	const Resume r = resume_;
	resume_ = Resume::None;
	if (r == Resume::Respawn) {
		finishRespawn();
		mainLoopEnd(input);
	} else if (r == Resume::NewGame) {
		// AF80, A16C-A17E: the panel, then the main loop runs once more in this frame
		setUpGame();
		if (!attract(g)) {
			showLives();
			showBridges();
			showScore();
		}
		mainLoop(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
		input.start || input.select || input.option;
	if (active) {
		g.idleTime = 0;
		g.pauseState = kPlaying;
	}
	// A1C5-A1F3: pause with any key of the keyboard (SKSTAT bit 2): press, release (paused),
	// press, release (playing again)
	const bool keyDown = input.key;
	if (keyDown) g.idleTime = 0;
	switch (g.pauseState) {
	case kPlaying:
		if (keyDown) {
			g.pauseState = kPausing;
			return;
		}
		break;
	case kResuming:
		if (!keyDown) g.pauseState--;
		break;
	case kPaused:
		if (!keyDown) return;
		g.pauseState--;
		break;
	default:                                      // kPausing
		if (keyDown) return;
		g.pauseState--;
		return;
	}
	if (g.idleTime & 0x80) return;                // A1F5: screen off after a long time without input

	// A1FF: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10
	if ((g.frameCount & 0x0F) == 0) {
		uint8_t n = random() & 0x0F;
		if (n >= kSlots) n = lo(n - 5);
		if (n != s.enteringSlot) s.slot[n].flags |= slotflag::kAwake;
	}
	driveTank();
	moveObjects();

	// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract
	// mode starts (with a new game 1 river)
	if (g.attractTimer && !(g.frameCount & 7)) {
		uint8_t timer = lo(g.attractTimer - 1);
		if (timer == 0) {
			timer = 0xFF;
			if (!attract(g)) {
				g.attractFlags |= 0x80;
				g.attractTimer = timer;
				newGame();
				return;
			}
		}
		g.attractTimer = timer;
	}
	// A312: the joystick of the player whose turn it is
	const int stick = g.player & 1;
	g.joystickPrev = lo(g.joystick << 4);
	g.joystick = input.stick[stick] & 0x0F;
	if ((g.joystick != 0x0F || input.trigger[stick]) && g.planeState == kWaiting) {
		g.planeState = kFlying;                   // A332: the waiting plane goes
		g.speed = 0x40;
	}
	// A33F: the plane
	const uint8_t state = g.planeState;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && g.planeFrame) g.planeFrame--;   // explosion frames
			g.planeState--;
			if (!(g.planeState & 0x80) && respawnOrGameOver()) return;
		}
	} else if (state == kFlying) {
		flyPlane(input);
	} else if (state >= kLifeLost) {              // A348: the river refilling
		if (state == kLifeLost) {
			g.crashPitch = 0x80;
			g.lives--;
		}
		g.planeState--;
	}
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop.
void Game::mainLoopEnd(const Input &input) {
	moveMissile();                                // A5B7
	moveShell();                                  // A607

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
	} else if (g.planeState == kWaiting) {        // the plane level, no scrolling
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

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the reset
// table: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = gen.s;
	auto table = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	if (full) s.level = table(0x60);              // loaded before B16B reads it for the bank colour
	gen.restoreCheckpoint();   // the river's part: $4D-$5F, $2B-$4C and $0500-$05FF, the bridge in slot 10
	// (restoreCheckpoint also continues from the checkpoint, A3C8; finishRespawn() does it again)
	g.bulletY = table(0x56);
	g.planeX = table(0x57);
	g.planeXFrac = table(0x58);
	g.speed = table(0x59);
	g.fuel = table(0x5B);
	g.fuelFrac = table(0x5C);
	g.planeFrame = table(0x5E);
	if (full) {
		g.bridges[0] = table(0x61);
		g.bridges[1] = table(0x62);
		for (int i = 0; i < 3; i++) g.score[i] = table(uint16_t(0x63 + i));
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(uint16_t(0x66 + i));
		g.lives = table(0x6A);
		g.other.level = table(0x6B);
		g.other.bridges[0] = table(0x6C);
		g.other.bridges[1] = table(0x6D);
		for (int i = 0; i < 3; i++) g.other.score[i] = table(uint16_t(0x6E + i));
		for (int i = 0; i < 4; i++) g.other.checkpoint[i] = table(uint16_t(0x71 + i));
		g.other.lives = table(0x75);
		g.planeState = table(0x76);
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
	drawLogoAndNeedle();
}

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() {
	if (g.gameNumber & 1) {
		showOtherScore();
		g.panel[kPanelPlayer1Mark] = 0x4F;
		g.panel[kPanelPlayer2Mark] = 0x50;
	} else {
		showHighScore();
		g.panel[kPanelPlayer1Mark] = 0xDB;
		g.panel[kPanelPlayer2Mark] = 0xD0;
	}
	const int start = g.gameNumber >> 1;
	for (int i = 0; i < 4; i++) g.other.checkpoint[i] = gen.s.checkpoint[i] = rom_.startCheckpoint(start, i);
	gen.s.level = g.other.level = rom_.startLevel(start);
	g.bridges[1] = g.other.bridges[1] = rom_.startBridges(start);
	g.bridges[0] = g.other.bridges[0] = 0;
}

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	g.gameNumber = (g.gameNumber + 1) & 7;
	g.panel[kPanelGameNumber] = 0;
	g.panel[kPanelGameNumber + 1] = lo(g.gameNumber + 0xD1);   // digit 1-8
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
		for (int i = 0; i < 4; i++) g.panel[kPanelLives + i] = rom_.playerTwoText(i) ^ 0xC0;
		printBcd(g.other.bridges, 2, kPanelOtherBridges, kDigitsOther);
	} else {
		showHighScore();
	}
	// (In the original the high score check makes this frame's work end close to the vertical
	// blank: depending on the objects on screen, sub $B21A sets the frame counter to $FF just
	// before or just after that frame's VBI counts it, or even in the next frame. The port
	// does it before the VBI, and the "Game over" text (A3B4) after it, as seen most often.)
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

// A21B-A26A: the tank on the road drives towards the river every 16 frames (every 4 while
// its kind is 4-7), stops at the edge and fires; its kind animates.
void Game::driveTank() {
	RiverState &s = gen.s;
	const uint8_t every = (s.tankKind >= 4 && s.tankKind < 8) ? 0x03 : 0x0F;
	if (every & g.frameCount) return;
	bool drive = false;
	if (s.shellTimer & 0x80) {                    // no shell in flight
		if (attract(g) || g.planeState == kFlying) {
			if (s.tankFires & 0x80) {
				if (s.tankY >= 0x1A) {
					if (g.hit.tankAtEdge & 0x80) drive = true;   // not at the river edge yet
					else s.shellTimer++;                         // at the edge: fire (timer 0)
				}
			} else if (s.bridgeHit == 0) {
				drive = true;                     // a tank on a bridge drives across
			}
		}
	}
	if (drive) {
		if ((s.tankKind & 0x7F) < 6) s.tankX++;
		else s.tankX--;
	}
	s.tankKind = (s.tankKind & 0x80) ? 0x03 : rom_.nextTankKind(s.tankKind);
}

// A26C-A2ED: animation and movement of the objects in the slots.
void Game::moveObjects() {
	using namespace object;
	RiverState &s = gen.s;
	for (int n = s.firstSlot; n < kSlots; n++) {
		Slot &obj = s.slot[n];
		uint8_t kind = obj.kind;
		if (kind >= kBridge) continue;                    // bridges and depots stay put
		if (kind < kBalloon) {                            // animation: every 8 frames, or 2 for kinds 7+
			const uint8_t every = kind >= kJet ? 0x01 : 0x07;
			if (!(every & g.frameCount)) {
				obj.kind = rom_.nextKind(kind);
				kind = obj.kind;
			}
		}
		if (!attract(g) && g.planeState != kFlying) continue;   // nothing moves until the plane flies
		if ((s.level >> 1) == 0) continue;               // nothing moves before bridge 2
		if (kind != kJet) {                               // jets always fly; others:
			if (kind < kJet) continue;                    // not explosions
			if (kind == kBalloon && (g.frameCount & 2)) continue;   // balloons at half speed
			if ((n ^ g.frameCount ^ s.slotParity) & 1) continue;    // A1F9: odd and even slots in turn
			const uint8_t flags = obj.flags;
			if (!(flags & slotflag::kAwake)) continue;
			if (flags & slotflag::kTouchingEdge) {
				// turn round once, at the first contact with the bank edge
				if (!(flags & slotflag::kTurned)) obj.flags = flags ^ (slotflag::kTurned | slotflag::kFacingLeft);
			} else {
				obj.flags = flags & ~slotflag::kTurned;
			}
		}
		// A2CA: one step left or right, wrapping round the screen
		if (obj.flags & slotflag::kFacingLeft) {
			obj.x = lo(obj.x - 1);
			if (obj.x < 0x2C) obj.x = 0xD4;
		} else {
			obj.x = lo(obj.x + 1);
			if (obj.x >= 0xD5) obj.x = 0x2C;
		}
	}
}

// A5B7-A605: the shooting helicopter fires a missile (missile 0) the way it faces; the missile
// flies until it hits the bank.
void Game::moveMissile() {
	using namespace object;
	RiverState &s = gen.s;
	uint8_t x = 0;
	const int n = s.shooterSlot;
	if (n < kSlots && (s.slot[n].kind == kShooter || s.slot[n].kind == kShooter + 1)) {
		const Slot &heli = s.slot[n];
		if (s.missileX) {                                 // flying
			if (g.hit.missileHitBank & 0x80) x = lo(s.missileX + g.missileStep);
		} else {
			if (!attract(g) && g.planeState != kFlying) {
				s.missileX = 0;
				return;
			}
			const uint8_t y = lo(heli.y - 0x3C);
			if (y < 0x1C) return;                         // too high on the screen: wait
			s.missileY = y;
			g.soundMissile = 9;
			x = lo(heli.x - 3);
			if (heli.flags & slotflag::kFacingLeft) {
				g.missileStep = 0xFE;                     // -2
			} else {
				g.missileStep = 0x02;
				x = lo(x + 10);
			}
		}
	}
	s.missileX = x;
}

// A607-A671: the tank's shell flies across the river, then explodes (shellTimer counts
// 0..$38: launch, flight 1-$10, explosion $11-$38).
void Game::moveShell() {
	RiverState &s = gen.s;
	uint8_t frame = 0;
	if (!(s.shellTimer & 0x80)) {
		bool count = true;
		const bool facingRight = s.tankKind < 6;
		if (s.shellTimer == 0) {                          // A60F: launch from the tank
			if (s.tankY < 0x19) {
				count = false;                            // the tank is too high: wait
			} else {
				s.shellY = lo(s.tankY - 6);
				g.shellXFrac = 0;
				if (facingRight) {
					// ADC #9 (carry clear), SBC #4: one more off unless the sum carried
					const int sum = s.tankX + 9;
					g.shellX = lo(sum - 4 - (sum > 0xFF ? 0 : 1));
				} else {
					g.shellX = lo(s.tankX - 4);
				}
				frame = 1;
			}
		} else if (s.shellTimer < 0x11) {                 // A62A: in flight
			s.shellY = lo(s.shellY + rom_.shellRise(s.shellTimer));
			if (facingRight) {
				const int frac = g.shellXFrac + s.shellSpeedFrac;
				g.shellXFrac = lo(frac);
				g.shellX = lo(g.shellX + s.shellSpeed + (frac >> 8));
			} else {
				const int frac = g.shellXFrac - s.shellSpeedFrac;
				g.shellXFrac = lo(frac);
				g.shellX = lo(g.shellX - s.shellSpeed - (frac < 0 ? 1 : 0));
			}
			frame = 1;
		} else {                                          // A657: explosion frames
			frame = lo(((s.shellTimer - 0x11) >> 3) + 2);
		}
		if (count) {
			uint8_t timer = lo(s.shellTimer + 1);         // A65E
			if (timer >= 0x39) {                          // over: the tank fires again (no shell after a crash)
				timer = g.planeState ? 0xFF : 0;
				frame = 0;
			}
			s.shellTimer = timer;
		}
	}
	g.shellFrame = frame;
}

// A6FD-A70F: the speed is added twice per frame to a fraction; each carry is one new river
// line. Speeds $FE and up give a line on both steps.
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

// B2A5-B3F0: the vertical blank: the sound driver, then the frame counter.
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
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	g.colourWater = 0x84;
	if (g.pauseState < kPaused && g.bridgeFlash) {
		if (g.bridgeFlash & 1) g.colourWater = 0x42;
		g.bridgeFlash--;
	}
	g.colourEdge = 0x20;
	g.colourBank = bankColour(gen.s);              // sub $AFD0
	if (!g.screenOn) g.colourBank = g.colourEdge = g.colourWater = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	g.logoShown = g.scroller.logoX;
	g.scroller.step(g.attractTimer != 0);
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		g.scroller.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver. Once per frame it sets the four POKEY channels from the game
// state (AUDCTL stays 0: 64 kHz clock, 17-bit noise). AUDF sets the pitch (higher = lower
// note), AUDC the distortion (high 3 bits) and the volume (low 4 bits).
//   channel 1: the engine (pitch from the speed, tone from the stick), or the low-fuel
//              warning, or an event sound (crash, refuelling...), or the start beep;
//   channel 2: explosion noise or the helicopter's missile;
//   channel 3: firing or the bonus life sound;
//   channel 4: the tank shell in flight, then its explosion.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	RiverState &s = gen.s;
	PokeyRegs &p = g.pokey;
	auto silenceChannels234 = [&]() { p.audc[1] = p.audc[2] = p.audc[3] = 0; };
	if (g.startBeep) {                            // B2A6
		g.startBeep--;
		p.audf[0] = 0x40;
		p.audc[0] = 0xAF;
		silenceChannels234();
		return;
	}
	if (g.pauseState >= kPaused) {                // B2BA
		p.audc[0] = 0;
		silenceChannels234();
		return;
	}
	// channel 1. `last` is the last value the original computed here (its A register): channel
	// 2 takes it as its pitch when it has nothing to play (at volume 0).
	uint8_t pitch = 0x39, control = 0, last = 0;
	bool setPitch = true;                         // B34C sets the pitch, B34F only the control
	if (g.eventSound) {                           // B2D2: an event sound, eventSoundTime frames long
		uint8_t volume = g.eventSoundTime;
		if (g.eventSound <= kOutOfFuelSound) {
			if (g.eventSound == kOutOfFuelSound) pitch = 0x11;
			volume = uint8_t((volume >> 1) | 0x80);
		} else {
			pitch = g.eventSound == kTankFullSound ? 0x1F : 0x3F;
			volume = lo(volume + 0xA0);
		}
		control = last = volume;
		if (--g.eventSoundTime == 0) {
			g.eventSound = 0;
			last = 0;
		}
	} else if (g.planeState != kFlying) {         // B2F9: no plane: channel 1 off
		last = g.planeState;
		setPitch = false;
	} else {
		bool engine = true;
		if (g.fuel < 0x40) {                      // B2FD: low fuel: a beep every 63 frames
			uint8_t time = g.eventSoundTime ? g.eventSoundTime : 0x3F;
			g.eventSoundTime = --time;
			uint8_t beep = 0;
			if (g.fuel < 0x04) {                  // almost empty: a tone falling with the fuel
				beep = uint8_t((g.fuel << 2) | (g.fuelFrac >> 6));
				beep = uint8_t(lo((beep ^ 0xFF) + 0x20) << 1);
			}
			if (beep == 0 && time >= 0x1C) beep = time;   // the first frames of each 63
			if (beep) {                           // B326
				pitch = last = uint8_t(beep << 1);
				control = 0xAF;
				engine = false;
			}
		}
		if (engine) {                             // B32C: the engine, pitch from the speed
			pitch = lo(0x1F - (g.speed >> 4));
			if (g.crashPitch >= 0x1B) pitch = g.crashPitch--;   // falling pitch after a crash
			control = last = rom_.engineTone(g.joystick);       // tone from up/down
		}
	}
	if (setPitch) p.audf[0] = pitch;
	p.audc[0] = control;
	// channel 2
	pitch = last;
	control = g.soundExplosion;
	if (control) {                                // B352: explosion noise, fading
		g.soundExplosion--;
		control = uint8_t((((control >> 1) + 4) & 0x0F) + 0x80);
		pitch = uint8_t(random() | 0x18);
	}
	if (const uint8_t t = g.soundMissile) {       // B367: the helicopter's missile
		g.soundMissile--;
		if (!attract(g)) {
			control = lo(t + 0x46);
			pitch = uint8_t(((lo((control ^ 0x0E) + 4)) & 0x0F) << 2);
		}
	}
	p.audf[1] = pitch;
	p.audc[1] = control;
	// channel 3
	control = 0;
	pitch = 0;
	if (const uint8_t t = g.soundFire) {          // B386: firing, rising
		g.soundFire--;
		const uint8_t twice = uint8_t(lo(0x1C - t) << 1);
		pitch = lo(uint8_t(twice << 1) + twice + (twice >> 7));   // 6 times, with the carry of the shift
		control = 0xA8;
	}
	if (g.soundScore) {                           // B399: a bonus life
		const uint8_t t = --g.soundScore;
		control = uint8_t((t & 0x0F) + 0xA0);
		pitch = rom_.scorePitch(t >> 4);
	}
	p.audf[2] = pitch;
	p.audc[2] = control;
	// channel 4
	control = 0;
	if (!attract(g) && !(s.shellTimer & 0x80) && s.shellTimer != 0) {   // B3B6: the tank shell
		if (s.shellTimer < 0x11) {
			p.audf[3] = rom_.shellPitch(s.shellTimer);   // whistling down
			control = 0xAC;
		} else {
			control = 0x8C;                       // exploding: noise
			p.audf[3] = uint8_t(random() | 0x20);
		}
	}
	p.audc[3] = control;
}

// A3E9-A5B4: the plane is flying.
void Game::flyPlane(const Input &input) {
	using namespace object;
	RiverState &s = gen.s;
	// A3E9: the plane touched an object
	const uint8_t touched = g.hit.planeHitObject;
	if (!(touched & 0x80)) {
		const uint8_t kind = s.slot[touched].kind;
		if (kind >= kJet) {
			if (kind <= kBridge) {                // an enemy or a bridge: both explode
				if (kind == kBridge) s.bridgeHit++;
				crash(kCrashSound, 0x1F);
				explode(touched, kRammed);
				return;
			}
			// A40A: a fuel depot: +2 per frame (ADC #1 with the carry of CMP #$0E)
			int fuel = g.fuel + 2;
			uint8_t sound = kRefuelSound;
			if (fuel > 0xFF) {
				fuel = 0xFF;
				g.fuelFrac = 0xFF;
				sound = kTankFullSound;
			}
			g.fuel = lo(fuel);
			if (sound != g.eventSound) {
				g.eventSound = sound;
				g.eventSoundTime = 8;
			}
		}
	}
	// A424: the tank shell exploding on the plane, the helicopter's missile, the bank
	const bool shellExploding = s.shellTimer >= 0x11;   // (also with no shell: timer $FF)
	if ((!(g.hit.planeHitShell & 0x80) && shellExploding) || !(g.hit.missileHitPlane & 0x80) ||
		!(g.hit.planeHitBank & 0x80)) {
		crash(kCrashSound, 0x1F);
		return;
	}
	// A43F: fuel, $20 / 256 per frame
	{
		const int frac = g.fuelFrac - 0x20;
		if (frac < 0) {
			if (g.fuel == 0) {
				crash(kOutOfFuelSound, 0x23);
				return;
			}
			g.fuel--;
		}
		g.fuelFrac = lo(frac);
	}
	steer();
	changeSpeed();
	// A4B6: the bullet hit an object
	const uint8_t shot = g.hit.bulletHitObject;
	if (!(shot & 0x80) && s.slot[shot].kind >= kJet) {
		explode(shot, kShot);
		return;
	}
	// A4C1: the tank shell's explosion destroys an object
	const uint8_t shelled = g.hit.shellHitObject;
	if (!(shelled & 0x80) && shellExploding && s.slot[shelled].kind >= kHelicopter) {
		s.slot[shelled].kind = kShot;
		g.soundExplosion = 0x17;
	}
	// A58D: the bullet stops at the bank, flies up 6 lines per frame, or is fired
	if (!(g.hit.bulletHitBank & 0x80)) {
		moveBullet(1);
	} else if (g.bulletY >= 0x1C) {
		moveBullet(g.bulletY - 6);
	} else if (input.trigger[g.player & 1]) {
		g.soundFire = 0x0F;
		moveBullet(0xA9);
	} else {
		moveBullet(1);
	}
}

// A455-A496: steering. Left or right accelerates (+8 per frame up to $F8); the plane moves a
// pixel each time the fraction overflows, every frame at full speed. Plane frames 7/8/9:
// banking left / right / level.
void Game::steer() {
	const uint8_t stick = g.joystick;
	uint8_t frame = 9;
	const bool left = !(stick & 0x04), right = !(stick & 0x08);
	if (!left && !right) {
		g.planeAccel = 0;
		g.planeXFrac = 0;
	} else {
		const int accel = g.planeAccel + 8;
		const bool full = accel > 0xFF;           // already at full speed
		if (!full) g.planeAccel = lo(accel);
		if (!right) {                             // left
			frame = 7;
			if (full) {
				g.planeX--;
				g.planeXFrac = 0x08;              // (what AND #$08 left in A)
			} else {
				const int frac = g.planeXFrac - g.planeAccel;
				if (frac < 0) g.planeX--;
				g.planeXFrac = lo(frac);
			}
		} else {
			frame = 8;
			if (full) {
				g.planeX++;
				g.planeXFrac = 0;                 // (what AND #$08 left in A)
			} else {
				const uint8_t start = (g.joystickPrev & 0x80) ? 0xFF : g.planeXFrac;   // just pressed: move now
				const int frac = start + g.planeAccel;
				if (frac > 0xFF) g.planeX++;
				g.planeXFrac = lo(frac);
			}
		}
	}
	g.planeFrame = frame;
}

// A497-A4B5: speed: up +2 (up to $FE), down -2 (down to $40), neither: back towards $80.
void Game::changeSpeed() {
	const uint8_t stick = g.joystick;
	const uint8_t speed = g.speed;
	bool faster;
	if (!(stick & 0x01)) faster = true;           // up (also when both are pressed)
	else if (!(stick & 0x02)) faster = false;     // down
	else if (speed == 0x80) return;               // neither, at normal speed
	else faster = speed < 0x80;
	if (faster) {
		if (speed <= 0xFD) g.speed = lo(speed + 2);
	} else if (speed >= 0x41) {
		g.speed = lo(speed - 2);
	}
}

// A4E0-A589: the object in slot n explodes (kind kShot or kRammed); points.
void Game::explode(int n, uint8_t kind) {
	using namespace object;
	RiverState &s = gen.s;
	Slot &obj = s.slot[size_t(n)];
	obj.kind = kind;
	uint8_t type = obj.type;
	g.soundExplosion = 0x17;
	if (type == kBridge) {
		g.bridgeFlash = 0x17;
		int carry = 1;                            // bridge counter +1 (BCD)
		g.bridges[1] = addBcd(g.bridges[1], 0, carry);
		g.bridges[0] = addBcd(g.bridges[0], 0, carry);
		s.bridgeHit++;
		// A500: a tank on the bridge goes with it
		if (lo(obj.x - 8) < s.tankX && lo(obj.x + 0x1F) >= s.tankX) {
			s.tankKind |= 0x80;
			g.soundExplosion = 0x37;
			type = kBridgeWithTank;
		} else if (g.bridges[0] != 0 || g.bridges[1] >= 5) {
			// A52B: from bridge 5 on, a tank close to the bridge fires
			const bool near = s.tankKind < 6 ? lo(obj.x - s.tankX) < 0x29 : lo(s.tankX - obj.x) < 0x41;
			if (near) s.shellTimer++;
		}
	}
	// A547: points (BCD); every 10 000 points a new plane, up to 9
	const uint8_t tenThousands = g.score[1] & 0xF0;
	int carry = 0;
	g.score[2] = addBcd(g.score[2], rom_.points(type), carry);
	g.score[1] = addBcd(g.score[1], 0, carry);
	g.score[0] = addBcd(g.score[0], 0, carry);
	if (carry) {                                  // past 999 999: the game ends
		g.score[0] = g.score[1] = g.score[2] = 0x99;
		g.lives = 0;
		crash(kCrashSound, 0x1F);
		return;
	}
	if ((g.score[1] & 0xF0) != tenThousands && g.lives != 9) {
		g.lives++;
		g.soundScore = 0x40;
	}
	moveBullet(1);                                // A589: the bullet is used up
}

// sub $B1C0: the plane crashes, with an event sound of this length.
void Game::crash(uint8_t sound, uint8_t length) {
	RiverState &s = gen.s;
	g.eventSoundTime = length;
	g.eventSound = sound;
	// where the next plane starts: past a destroyed bridge, from the next section; on the run-up
	// to a bridge not hit, one checkpoint back
	if ((s.slot[kSlots - 1].flags ^ s.blockFlags) & kBankShade) {
		if (s.bridgeHit) {
			if (s.blockFlags & kBridgeBlock) gen.sectionCheckpoint();
		} else if (!(s.blockFlags & kBridgeBlock)) {
			uint8_t level = lo(s.level - 1);
			if (level == 0x4E) level = 0x50;
			s.level = level;
			std::memcpy(s.checkpoint, s.prevCheckpoint, 4);
		}
	}
	g.planeFrame = 6;
	// B200
	g.planeState = kCrashed;
	g.attractTimer = 0;
	removePlaneThings();
}

// sub $B21A: no plane (game over or attract mode).
void Game::gameOverScreen() {
	g.frameCount = 0xFF;
	g.planeState = kNoPlane;
	g.attractTimer = 0xFF;
	removePlaneThings();
}

// B204-B220: the bullet and the helicopter's missile go.
void Game::removePlaneThings() {
	clearBullet();
	gen.s.shooterSlot = kNoSlot;
	g.pm.at(uint16_t(kMissileMemory + gen.s.missileY)) &= ~kMissile0;
	g.bulletY = 1;
}

// sub $B221 / $B223: the bullet is 8 rows of missile 2 below row bulletY (at least row $1C).
void Game::drawBullet(uint8_t bits) {
	int row = g.bulletY - 1;
	if (row == 0) return;
	if (row < 0x1B) row = 0x1B;
	for (int i = 1; i <= 8; i++) g.pm.at(uint16_t(kMissileMemory + row + i)) = bits;
}
void Game::clearBullet() { drawBullet(0); }

// A5AA-A5B4: the bullet to row y.
void Game::moveBullet(int y) {
	clearBullet();
	g.bulletY = lo(y);
	drawBullet(kBulletBits);
}

// sub $B247: two-player game: the other player's turn.
void Game::swapPlayers() {
	if (!(g.gameNumber & 1)) return;
	g.player ^= 1;
	RiverState &s = gen.s;
	PlayerData current;
	current.level = s.level;
	std::memcpy(current.bridges, g.bridges, 2);
	std::memcpy(current.score, g.score, 3);
	std::memcpy(current.checkpoint, s.checkpoint, 4);
	current.lives = g.lives;
	s.level = g.other.level;
	std::memcpy(g.bridges, g.other.bridges, 2);
	std::memcpy(g.score, g.other.score, 3);
	std::memcpy(s.checkpoint, g.other.checkpoint, 4);
	g.lives = g.other.lives;
	g.other = current;
}

// sub $B261: the score (this or the other player's) becomes the high score of this pair of
// games if higher; then the check bytes are recomputed (sub $B298).
void Game::updateHighScore(bool otherPlayer) {
	const uint8_t *score = otherPlayer ? g.other.score : g.score;
	uint8_t *high = &g.highScore[(g.gameNumber & 0x0E) * 2];
	for (int i = 0; i < 3; i++) {
		if (score[i] < high[i]) return;
		if (score[i] > high[i]) break;
	}
	std::memcpy(high, score, 3);
	for (int check = 15; check >= 0; check--) {
		int sum = 0, carry = 0;
		for (int i = 14; i >= 0; i--) {
			const int r = sum + g.highScore[i] + carry;
			carry = r > 0xFF;
			sum = (r & 0xFF) ^ check;
		}
		g.highScoreCheck[check] = uint8_t(sum);
	}
}

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() {
	int shown = g.lives;
	if (shown && (g.soundScore & 0x08)) shown--;
	for (int i = 0; i < 9; i++) g.panel[kPanelLives + i] = i < shown ? kLifeSymbol : 0;
}

// B086-B0C6: BCD bytes as digits in the panel, leading zeros blank; nothing in the attract mode.
void Game::printBcd(const uint8_t *bcd, int bytes, int at, uint8_t digitZero) {
	if (attract(g)) return;
	bool leading = true;
	for (int i = 0; i < bytes; i++) {
		for (const uint8_t digit : {uint8_t(bcd[i] >> 4), uint8_t(bcd[i] & 0x0F)}) {
			if (leading && digit == 0) {
				g.panel[at++] = 0;
			} else {
				leading = false;
				g.panel[at++] = lo(digit + digitZero);
			}
		}
	}
}
void Game::showBridges() { printBcd(g.bridges, 2, kPanelBridges, kDigitsBridges); }
void Game::showScore() { printBcd(g.score, 3, kPanelScore, kDigitsScore); }
void Game::showOtherScore() { printBcd(g.score, 3, kPanelHighScore, kDigitsOther); }
void Game::showHighScore() { printBcd(&g.highScore[(g.gameNumber & 0x0E) * 2], 3, kPanelHighScore, kDigitsHighScore); }

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	gen.s = RiverState();
	gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	g = GameState();
	screen.clear();
	for (int i = 0; i < kPanelBytes; i++) g.panel[i] = rom_.panelText(i);
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	g.powerOnDone = 1;
	g.attractTimer = 1;
	g.planeState = kNoPlane;
}

}  // namespace rr
