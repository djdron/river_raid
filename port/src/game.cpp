// River Raid game frame: the frame in the original's order (prepare, the main loop, the river
// scrolling, the vertical blank) and one frame of the river alone. See game.h. Addresses refer
// to the original cartridge code. The rest of Game: game_plane.cpp (the plane, explosions,
// points), game_players.cpp (games and players, the panel), game_reset.cpp (power-on, the
// attract mode, new game, reset, respawn), game_state.cpp (saved states), game_ram.cpp (the
// original's RAM).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>

namespace rr {

Game::Game(const Rom &rom) : world(rom), rom_(rom) {}

// A new river line goes above the others.
void Game::pushLine() {
	river.push(world);
	if (afterLine) afterLine(river.lines.front().data());
}

void Game::frame(const Input &input, const std::function<uint8_t()> &random) {
	if (busy_ > 0) {
		// sub $B0F1 is clearing memory: the main loop is stuck, only the interrupts run
		busy_--;
		if (busy_ == 1) screenOn = 1;           // screen memory clear: colours back on
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
	Objects &objs = world.objects;
	spritesShown = true;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	objs.shell.colour = rom_.shellColour(frameCount >> 3);
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// AF3A-AF58: lives, bridges and score into the panel
	if (attractTimer == 0) showLives();
	showBridges();
	if (player == 0) showScore();
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
		if (!attract(*this)) {
			showLives();
			showBridges();
			showScore();
		}
		mainLoop(input, random);
	}
}

// A197-A6EA. Returns early when a reset (new game, respawn) starts: the rest runs when it ends.
void Game::mainLoop(const Input &input, const std::function<uint8_t()> &random) {
	Objects &objs = world.objects;
	// A19E: any joystick, fire or console key: not idle, and no pause
	const bool active = input.stick[0] != 0x0F || input.stick[1] != 0x0F || input.trigger[0] || input.trigger[1] ||
		input.start || input.select || input.option;
	if (active) {
		idleTime = 0;
		pauseState = kPlaying;
	}
	// A1C5-A1F3: pause with any key of the keyboard (SKSTAT bit 2): press, release (paused),
	// press, release (playing again)
	const bool keyDown = input.key;
	if (keyDown) idleTime = 0;
	switch (pauseState) {
	case kPlaying:
		if (keyDown) {
			pauseState = kPausing;
			return;
		}
		break;
	case kResuming:
		if (!keyDown) pauseState--;
		break;
	case kPaused:
		if (!keyDown) return;
		pauseState--;
		break;
	default:                                      // kPausing
		if (keyDown) return;
		pauseState--;
		return;
	}
	if (idleTime & 0x80) return;                // A1F5: screen off after a long time without input

	// A1FF: every 16 frames a random slot wakes up (it may move); 11-15 count as 6-10
	if ((frameCount & 0x0F) == 0) {
		uint8_t n = random() & 0x0F;
		if (n >= kSlots) n = lo(n - 5);
		if (n != objs.enteringSlot) objs.slot[n].flags |= slotflag::kAwake;
	}
	{
		RiverState &s = world.gen.s;
		Objects &objs = world.objects;
		objs.tank.drive(frameCount, objs.shell, attract(*this) || plane.flying(), !(hit.tankAtEdge & 0x80), s.bridgeHit != 0, rom_);
	}
	moveObjects(world.objects, world.gen.s.level, frameCount, attract(*this) || plane.flying(), rom_);   // A26C

	// A2F5: the attract timer, -1 every 8 frames; when it runs out after a game, the attract
	// mode starts (with a new game 1 river)
	if (attractTimer && !(frameCount & 7)) {
		uint8_t timer = lo(attractTimer - 1);
		if (timer == 0) {
			timer = 0xFF;
			if (!attract(*this)) {
				attractFlags |= 0x80;
				attractTimer = timer;
				newGame();
				return;
			}
		}
		attractTimer = timer;
	}
	// A312: the joystick of the player whose turn it is
	const int stick = player & 1;
	plane.stickPrev = lo(plane.stick << 4);
	plane.stick = input.stick[stick] & 0x0F;
	if ((plane.stick != 0x0F || input.trigger[stick]) && plane.state == kWaiting) {
		plane.state = kFlying;                   // A332: the waiting plane goes
		plane.speed = 0x40;
	}
	// A33F: the plane
	const uint8_t state = plane.state;
	if (state & 0x80) {                           // A35C: exploding (or no plane)
		if (state != kNoPlane) {
			if (!(lo(state + 1) & 7) && plane.frame) plane.frame--;   // explosion frames
			plane.state--;
			if (!(plane.state & 0x80) && respawnOrGameOver()) return;
		}
	} else if (state == kFlying) {
		flyPlane(input);
	} else if (state >= kLifeLost) {              // A348: the river refilling
		if (state == kLifeLost) {
			sound.crashPitch = 0x80;
			lives--;
		}
		plane.state--;
	}
	mainLoopEnd(input);
}

// A5B7-A6EA: the rest of the main loop.
void Game::mainLoopEnd(const Input &input) {
	{
		using namespace object;
		Objects &objs = world.objects;
		const int n = objs.shooterSlot;
		const Slot *shooter = n < kSlots && (objs.slot[n].kind == kShooter || objs.slot[n].kind == kShooter + 1) ? &objs.slot[n] : nullptr;
		if (objs.missile.move(shooter, attract(*this) || plane.flying(), !(hit.missileHitBank & 0x80))) sound.missile = 9;   // A5B7
		objs.shell.move(objs.tank, plane.state != 0, rom_);   // A607
	}

	// A673: console keys
	if (input.start) {
		sound.startBeep = 2;
		attractFlags = 0;
		attractTimer = 0;
		newGame();
		return;
	}
	if (input.select || input.option) {
		if (selectDelay-- == 0) selectGame();   // A699: every 30 frames while held
	} else {
		selectDelay = 0;
	}

	// A6DE: scrolling
	if (plane.state & 0x80) {
		if (!attract(*this)) return;                  // exploding: the river stops
		plane.speed = gameNumber < 4 ? 0x80 : 0xFE; // A6F1: attract mode
	} else if (plane.state == kWaiting) {        // the plane level, no scrolling
		plane.frame = 9;
		return;
	}
	scrollRiver();
}

// A6FD-A70F: the speed is added twice per frame to a fraction; each carry is one new river
// line. Speeds $FE and up give a line on both steps.
void Game::scrollRiver() {
	for (int step = 0; step < 2; step++) {
		if (plane.speed < 0xFE) {
			const int sum = scrollFrac + plane.speed;
			scrollFrac = lo(sum);
			if (sum < 0x100) continue;
		}
		pushLine();
	}
}

// B2A5-B3F0: the vertical blank: the sound driver, then the frame counter.
void Game::vbi(const std::function<uint8_t()> &random) {
	if (vbiSeesBeforeGameOver_) {                 // PAL: sub $B21A comes after this VBI
		frameCount = beforeGameOver_[0];
		plane.state = beforeGameOver_[1];
		attractTimer = beforeGameOver_[2];
	}
	soundDriver(random);
	frameCount++;
	if (frameCount == 0) {
		attractFlags = lo(attractFlags + 1) & 0xC7;
		if (!(attractFlags & 7)) {
			idleTime++;
			if (idleTime == 0) idleTime = 0x80;
		}
	}
	// B3F2-B43E: colours for the next frame. The water flashes red while a hit bridge flashes;
	// while sub $B0F1 clears the screen the playfield colours are off.
	colourWater = 0x84;
	if (pauseState < kPaused && bridgeFlash) {
		if (bridgeFlash & 1) colourWater = 0x42;
		bridgeFlash--;
	}
	colourEdge = 0x20;
	colourBank = bankColour(world.gen.s, world.objects);              // sub $AFD0
	if (!screenOn) colourBank = colourEdge = colourWater = 0;
	// B44B-B47F: the scrolling line moves while the game is not being played
	logoShown = scroller.logoX;
	scroller.step(attractTimer != 0);
	if (vbiSeesBeforeGameOver_) {
		vbiSeesBeforeGameOver_ = false;
		frameCount = 0xFF;
		plane.state = kNoPlane;
		attractTimer = 0xFF;
	}
	if (gameOverText_) {                          // A3B4: "Game over" scrolls past first
		gameOverText_ = false;
		scroller.textLo = 0x20;
	}
}

// B2A5-B3D8: the sound driver (sound.cpp), with what it needs of the game.
void Game::soundDriver(const std::function<uint8_t()> &random) {
	SoundContext c{plane};
	c.paused = pauseState >= kPaused;
	c.attract = attract(*this);
	c.attractSound = attractSound;
	c.attractEnginePitch = attractEnginePitch;
	c.attractEngineStick = attractEngineStick;
	c.shellTimer = world.objects.shell.timer;
	sound.drive(pokey, c, random, rom_);
}

// One frame of the river only, for jumping ahead in the attract mode (see game.h).
void Game::skipFrame() {
	// what prepare() does to the river's state: the tank and its shell go once they have
	// scrolled below the screen (a tank still there would keep the generator from placing the
	// next one)
	Objects &objs = world.objects;
	if (objs.tank.y >= 0xBC) objs.tank.kind = objs.tank.y = 0;
	if (objs.shell.y >= 0xBC) {
		objs.shell.timer = 0xFF;
		objs.shell.frame = 0;
		objs.shell.y = 0;
	}
	// A2F5: the attract timer (in the attract mode it only goes round)
	if (attractTimer && !(frameCount & 7)) {
		const uint8_t timer = lo(attractTimer - 1);
		attractTimer = timer ? timer : 0xFF;
	}
	scrollRiver();
	// the counters of vbi()
	frameCount++;
	if (frameCount == 0) {
		attractFlags = lo(attractFlags + 1) & 0xC7;
		if (!(attractFlags & 7)) {
			idleTime++;
			if (idleTime == 0) idleTime = 0x80;
		}
	}
	colourBank = bankColour(world.gen.s, world.objects);
	logoShown = scroller.logoX;
	scroller.step(attractTimer != 0);
}

}  // namespace rr
