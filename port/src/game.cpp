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
		if (busy_ == 1) screenOn_ = 1;           // screen memory clear: colours back on
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
	objs.shell.colour = rom_.shellColour(frameCount_ >> 3);
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// AF3A-AF58: lives, bridges and score into the panel
	if (attractTimer_ == 0) showLives();
	match_.showBridges(panel_, attract());
	if (match_.player == 0) match_.showScore(panel_, attract());
	else match_.showOtherScore(panel_, attract());
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
			match_.showBridges(panel_, false);
			match_.showScore(panel_, false);
		}
		mainLoop(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	Objects &objs = world_.objects;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
		input.start || input.select || input.option;
	if (active) {
		idleTime_ = 0;
		pauseState_ = kPlaying;
	}
	// A1C5-A1F3: pause with any key of the keyboard (SKSTAT bit 2): press, release (paused),
	// press, release (playing again)
	const bool keyDown = input.key;
	if (keyDown) idleTime_ = 0;
	switch (pauseState_) {
	case kPlaying:
		if (keyDown) {
			pauseState_ = kPausing;
			return;
		}
		break;
	case kResuming:
		if (!keyDown) pauseState_--;
		break;
	case kPaused:
		if (!keyDown) return;
		pauseState_--;
		break;
	default:                                      // kPausing
		if (keyDown) return;
		pauseState_--;
		return;
	}
	if (idleTime_ & 0x80) return;                // A1F5: screen off after a long time without input

	// A1FF: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10
	if ((frameCount_ & 0x0F) == 0) {
		uint8_t n = random() & 0x0F;
		if (n >= kSlots) n = lo(n - 5);
		if (n != objs.enteringSlot) objs.slot[n].flags |= slotflag::kAwake;
	}
	{
		RiverState &s = world_.gen.s;
		Objects &objs = world_.objects;
		objs.tank.drive(frameCount_, objs.shell, attract() || plane_.flying(), !(hit_.tankAtEdge & 0x80), s.bridgeHit != 0, rom_);
	}
	moveObjects(world_.objects, world_.gen.s.level, frameCount_, attract() || plane_.flying(), rom_);   // A26C

	// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract
	// mode starts (with a new game 1 river)
	if (attractTimer_ && !(frameCount_ & 7)) {
		uint8_t timer = lo(attractTimer_ - 1);
		if (timer == 0) {
			timer = 0xFF;
			if (!attract()) {
				attractFlags_ |= 0x80;
				attractTimer_ = timer;
				newGame();
				return;
			}
		}
		attractTimer_ = timer;
	}
	// A312: the joystick of the player whose turn it is
	const int stick = match_.player & 1;
	plane_.stickPrev = lo(plane_.stick << 4);
	plane_.stick = input.stick[stick] & 0x0F;
	if ((plane_.stick != 0x0F || input.trigger[stick]) && plane_.state == kWaiting) {
		plane_.state = kFlying;                   // A332: the waiting plane goes
		plane_.speed = 0x40;
	}
	// A33F: the plane
	const uint8_t state = plane_.state;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && plane_.frame) plane_.frame--;   // explosion frames
			plane_.state--;
			if (!(plane_.state & 0x80) && respawnOrGameOver()) return;
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
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop.
void Game::mainLoopEnd(const Input &input) {
	{
		using namespace object;
		Objects &objs = world_.objects;
		const int n = objs.shooterSlot;
		const Slot *shooter = n < kSlots && (objs.slot[n].kind == kShooter || objs.slot[n].kind == kShooter + 1) ? &objs.slot[n] : nullptr;
		if (objs.missile.move(shooter, attract() || plane_.flying(), !(hit_.missileHitBank & 0x80))) sound_.missile = 9;   // A5B7
		objs.shell.move(objs.tank, plane_.state != 0, rom_);   // A607
	}

	// A673: console keys
	if (input.start) {
		sound_.startBeep = 2;
		attractFlags_ = 0;
		attractTimer_ = 0;
		newGame();
		return;
	}
	if (input.select || input.option) {
		if (selectDelay_-- == 0) selectGame();   // A699: every 30 frames while held
	} else {
		selectDelay_ = 0;
	}

	// A6DE: scrolling
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
		frameCount_ = beforeGameOver_[0];
		plane_.state = beforeGameOver_[1];
		attractTimer_ = beforeGameOver_[2];
	}
	soundDriver(random);
	frameCount_++;
	if (frameCount_ == 0) {
		attractFlags_ = lo(attractFlags_ + 1) & 0xC7;
		if (!(attractFlags_ & 7)) {
			idleTime_++;
			if (idleTime_ == 0) idleTime_ = 0x80;
		}
	}
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	colourWater_ = 0x84;
	if (pauseState_ < kPaused && bridgeFlash_) {
		if (bridgeFlash_ & 1) colourWater_ = 0x42;
		bridgeFlash_--;
	}
	colourEdge_ = 0x20;
	colourBank_ = bankColour(world_.gen.s, world_.objects);              // sub $AFD0
	if (!screenOn_) colourBank_ = colourEdge_ = colourWater_ = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	logoShown_ = scroller_.logoX;
	scroller_.step(attractTimer_ != 0);
	if (vbiSeesBeforeGameOver_) {
		vbiSeesBeforeGameOver_ = false;
		frameCount_ = 0xFF;
		plane_.state = kNoPlane;
		attractTimer_ = 0xFF;
	}
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		scroller_.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver (sound.cpp), with what it needs of the game.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	SoundContext c{plane_};
	c.paused = pauseState_ >= kPaused;
	c.attract = attract();
	c.attractSound = attractSound;
	c.attractEnginePitch = attractEnginePitch;
	c.attractEngineStick = attractEngineStick;
	c.shellTimer = world_.objects.shell.timer;
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
	if (attractTimer_ && !(frameCount_ & 7)) {
		const uint8_t timer = lo(attractTimer_ - 1);
		attractTimer_ = timer ? timer : 0xFF;
	}
	scrollRiver();
	// the counters of vbi()
	frameCount_++;
	if (frameCount_ == 0) {
		attractFlags_ = lo(attractFlags_ + 1) & 0xC7;
		if (!(attractFlags_ & 7)) {
			idleTime_++;
			if (idleTime_ == 0) idleTime_ = 0x80;
		}
	}
	colourBank_ = bankColour(world_.gen.s, world_.objects);
	logoShown_ = scroller_.logoX;
	scroller_.step(attractTimer_ != 0);
}

void Game::setSkipState(const SkipState &s) {
	world_.setState(s.world);
	frameCount_ = s.frameCount;
	attractFlags_ = s.attractFlags;
	attractTimer_ = s.attractTimer;
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
