// River Raid game frame: the frame in the original's order (prepare, the main loop, the river
// scrolling, the vertical blank) and one frame of the river alone. See game.h. Addresses refer
// to the original cartridge code. The rest of Game: game_plane.cpp (the plane, explosions,
// points), game_players.cpp (choosing and setting up a game; the match itself is Match,
// match.h), game_reset.cpp (power-on, the attract mode, new game, reset, respawn),
// game_state.cpp (saved states), game_ram.cpp (the original's RAM).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>

namespace rr {

using namespace game_detail;

Game::Game(const Rom &rom) : world_(rom), rom_(rom) {}

// A new river line goes above the others.
void Game::pushLine() {
	river_.push(world_);
	if (afterLine) afterLine(river_.lines.front().data());
}

void Game::frame(const Input &input, const std::function<uint8_t()> &random) {
	if (busy_ > 0) {
		// sub $B0F1 is clearing memory: the main loop is stuck, only the interrupts run
		busy_--;
		if (busy_ == 1) console_.screenOn = 1;           // screen memory clear: colours back on
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

// AE99-AF38, at the top of the frame: the sprites shown again (the original copies the tank's,
// the shell's and the plane's shapes into player-missile memory, and the missile's row); the
// tank and the shell gone once they have scrolled below the screen; then lives, bridges and
// score into the panel.
void Game::prepare() {
	Objects &objs = world_.objects;
	spritesShown_ = true;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	objs.shell.colour = rom_.shellColour(console_.frameCount >> 3);
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// AF3A-AF58: lives, bridges and score into the panel
	if (console_.attractTimer == 0) showLives();
	const Scoreboard board = scoreboard();
	board.bridges();
	if (match_.player == 0) board.score();
	else board.player2Score();
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
		if (!attract()) {
			showLives();
			scoreboard().bridges();
			scoreboard().score();
		}
		mainLoop(input, random);
	}
}

// A197-A6EA, step by step in the original's order. Returns early when a reset (new game,
// respawn) starts: the rest runs when it ends (finishReset).
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	if (!console_.keys(active(input), input.key)) return;   // A19E-A1F5
	wakeObject(random);                                     // A1FF
	driveTank();                                            // A21B
	moveObjects(world_.objects, world_.gen.s.level, console_.frameCount, attract() || plane_.flying(), rom_);   // A26C
	if (attractTimerRunsOut()) return;                      // A2F5
	readStick(input);                                       // A312
	if (updatePlane(input)) return;                         // A33F
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop (after a respawn's reset it goes on from here).
void Game::mainLoopEnd(const Input &input) {
	moveMissile();                                          // A5B7
	moveShell();                                            // A607
	if (consoleKeys(input)) return;                         // A673-A6DC
	scroll();                                               // A6DE-A70F
}

// A19E: any joystick, fire or console key: not idle, and no pause.
bool Game::active(const Input &input) {
	return input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] || input.start ||
		input.select || input.option;
}

// A1FF-A21A: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10.
void Game::wakeObject(const std::function<uint8_t()> &random) {
	if ((console_.frameCount & 0x0F) != 0) return;
	Objects &objs = world_.objects;
	uint8_t n = random() & 0x0F;
	if (n >= kSlots) n = lo(n - 5);
	if (n != objs.enteringSlot) objs.slot[n].flags |= slotflag::kAwake;
}

// A21B-A26B: the tank on the bank road.
void Game::driveTank() {
	Objects &objs = world_.objects;
	objs.tank.drive(console_.frameCount, objs.shell, attract() || plane_.flying(), !(hit_.tankAtEdge & 0x80),
		world_.gen.s.bridgeHit != 0, rom_);
}

// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract mode
// starts (with a new game 1 river). True then.
bool Game::attractTimerRunsOut() {
	if (!console_.attractTimerTick() || attract()) return false;
	console_.attractFlags |= 0x80;
	newGame();
	return true;
}

// A312-A33E: the joystick of the player whose turn it is; it starts a waiting plane.
void Game::readStick(const Input &input) {
	const int stick = match_.player & 1;
	plane_.stickPrev = lo(plane_.stick << 4);
	plane_.stick = input.stick[stick] & 0x0F;
	if ((plane_.stick != 0x0F || input.trigger[stick]) && plane_.state == kWaiting) {
		plane_.state = kFlying;                   // A332: the waiting plane goes
		plane_.speed = 0x40;
	}
}

// A33F-A36D: the plane: flying, exploding, or the river refilling after a life is lost. True
// when its explosion ends in a reset (the next plane).
bool Game::updatePlane(const Input &input) {
	const uint8_t state = plane_.state;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && plane_.frame) plane_.frame--;   // explosion frames
			plane_.state--;
			if (!(plane_.state & 0x80) && respawnOrGameOver()) return true;
		}
	} else if (state == kFlying) {
		flyPlane(input);
	} else if (state >= kLifeLost) {              // A348: the river refilling
		if (state == kLifeLost) {
			sound_.crashPitch = 0x80;
			match_.lives--;
		}
		plane_.state--;
	}
	return false;
}

// A5B7-A606: the helicopter's missile (fired by the shooter slot).
void Game::moveMissile() {
	using namespace object;
	Objects &objs = world_.objects;
	const int n = objs.shooterSlot;
	const Slot *shooter = n < kSlots && (objs.slot[n].kind == kShooter || objs.slot[n].kind == kShooter + 1) ? &objs.slot[n] : nullptr;
	if (objs.missile.move(shooter, attract() || plane_.flying(), !(hit_.missileHitBank & 0x80))) sound_.missile = 9;
}

// A607-A672: the tank's shell.
void Game::moveShell() {
	Objects &objs = world_.objects;
	objs.shell.move(objs.tank, plane_.state != 0, rom_);
}

// A673-A6DC: START (a new game: true) and SELECT / OPTION (the next game every 30 frames while
// held).
bool Game::consoleKeys(const Input &input) {
	if (input.start) {
		sound_.startBeep = 2;
		console_.attractFlags = 0;
		console_.attractTimer = 0;
		newGame();
		return true;
	}
	if (console_.selectRepeat(input.select || input.option)) selectGame();
	return false;
}

// A6DE-A70F: the river scrolls with the plane's speed; not while it explodes in a game, nor
// while it waits (level); in the attract mode at a fixed speed.
void Game::scroll() {
	if (plane_.state & 0x80) {
		if (!attract()) return;                  // exploding: the river stops
		plane_.speed = match_.gameNumber < 4 ? 0x80 : 0xFE; // A6F1: attract mode
	} else if (plane_.state == kWaiting) {        // the plane level, no scrolling
		plane_.frame = 9;
		return;
	}
	scrollRiver();
}

// A6FD-A70F: the speed is added twice per frame to a fraction; each carry is one new river
// line. Speeds $FE and up give a line on both steps.
void Game::scrollRiver() {
	for (int step = 0; step < 2; step++) {
		if (plane_.speed < 0xFE) {
			const int sum = scrollFrac_ + plane_.speed;
			scrollFrac_ = lo(sum);
			if (sum < 0x100) continue;
		}
		pushLine();
	}
}

// B2A5-B3F0: the vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	if (vbiSeesBeforeGameOver_) {                 // PAL: sub $B21A comes after this VBI
		console_.frameCount = beforeGameOver_[0];
		plane_.state = beforeGameOver_[1];
		console_.attractTimer = beforeGameOver_[2];
	}
	soundDriver(random);
	console_.countFrame();
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	colourWater_ = 0x84;
	if (!console_.paused() && bridgeFlash_) {
		if (bridgeFlash_ & 1) colourWater_ = 0x42;
		bridgeFlash_--;
	}
	colourEdge_ = 0x20;
	colourBank_ = bankColour(world_.gen.s, world_.objects);              // sub $AFD0
	if (!console_.screenOn) colourBank_ = colourEdge_ = colourWater_ = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	logoShown_ = scroller_.logoX;
	scroller_.step(console_.attractTimer != 0);
	if (vbiSeesBeforeGameOver_) {
		vbiSeesBeforeGameOver_ = false;
		console_.frameCount = 0xFF;
		plane_.state = kNoPlane;
		console_.attractTimer = 0xFF;
	}
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		scroller_.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver (sound.cpp), with what it needs of the game.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	const SoundContext c{plane_, console_.paused(), attract(), world_.objects.shell.timer, attractSound};
	sound_.drive(pokey_, c, random, rom_);
}

// One frame of the river only, for jumping ahead in the attract mode (see game.h).
void Game::skipFrame() {
	// what prepare() does to the river's state: the tank and its shell go once they have
	// scrolled below the screen (a tank still there would keep the generator from placing the
	// next one)
	Objects &objs = world_.objects;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// A2F5: the attract timer (in the attract mode it only goes round)
	console_.attractTimerTick();
	scrollRiver();
	console_.countFrame();   // the counters of vbi()
	colourBank_ = bankColour(world_.gen.s, world_.objects);
	logoShown_ = scroller_.logoX;
	scroller_.step(console_.attractTimer != 0);
}

void Game::setSkipState(const SkipState &s) {
	world_.setState(s.world);
	console_.frameCount = s.frameCount;
	console_.attractFlags = s.attractFlags;
	console_.attractTimer = s.attractTimer;
	scrollFrac_ = s.scrollFrac;
	logoShown_ = s.logoShown;
	scroller_ = s.scroller;
	river_.top = s.screenTop;
}

void Game::playSound(Sound s) {
	switch (s) {
	case Sound::Explosion: sound_.explosion = 0x17; break;   // as A4E6 sets it
	case Sound::Fire: sound_.fire = 0x0F; break;             // A5A6
	case Sound::Bonus: sound_.bonus = 0x40; break;           // A587
	}
}

}  // namespace rr
