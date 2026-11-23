// Starting: power-on, the attract mode, a new game, the reset of the river and the objects,
// the next plane or game over. Part of Game (game.h).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>

namespace rr {

void Game::startAttract(bool pal) {
	pal_ = pal;
	world_.gen.s = RiverState();
	world_.startGame(1, pal);
	resetState();
	river_.clear();
	for (int i = 0; i < kPanelBytes; i++) panel_.chars[i] = rom_.panelText(i);   // A0F0
	// the attract mode's new game: B0F1 with X = $29, AF80, then the respawn (A36E-A3E3) which
	// in the attract mode ends with sub $B21A; then frame 38
	attractFlags_ = 0x80;
	setUpGame();
	bullet_.y = 1;
	match_.lives = match_.other.lives = 4;
	match_.respawned = 1;
	powerOnDone_ = 1;
	screenOn_ = 1;
	attractFlags_ = 0x81;
	attractTimer_ = 0xFF;
	plane_.state = kNoPlane;
	plane_.speed = 0x80;
	plane_.fuel = 0xFF;
	plane_.fuelFrac = 0xFF;
	plane_.x = 0x7C;
	colourBank_ = bankColour(world_.gen.s, world_.objects);
	// the scrolling line has been moving since frame 11 (VBI steps up to frame 38; 9 to 25 on PAL)
	scroller_ = Scroller();
	for (int f = scrollerStartFrame(pal); f <= attractStartFrame(pal); f++) {
		logoShown_ = scroller_.logoX;
		scroller_.step(true);
	}
	pushLine();
}

// A688-A17E: a new game (START), or the attract mode's river starting.
void Game::newGame() {
	match_.player = match_.gameNumber & 1;
	// A15D with X = $79: $79-$7F cleared
	match_.respawned = 0;
	sound_.event = sound_.eventTime = sound_.explosion = sound_.fire = sound_.bonus = sound_.missile = 0;
	reset(true);   // the rest (AF80, A17E) when the reset ends: finishReset()
	resume_ = Resume::NewGame;
}

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the reset
// table: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = world_.gen.s;
	auto table = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	if (full) s.level = table(0x60);              // loaded before B16B reads it for the bank colour
	world_.restoreCheckpoint();   // the river's part: $4D-$5F, $2B-$4C and $0500-$05FF, the bridge in slot 10
	// (restoreCheckpoint also continues from the checkpoint, A3C8; finishRespawn() does it again)
	bullet_.y = table(0x56);
	plane_.x = table(0x57);
	plane_.xFrac = table(0x58);
	plane_.speed = table(0x59);
	plane_.fuel = table(0x5B);
	plane_.fuelFrac = table(0x5C);
	plane_.frame = table(0x5E);
	if (full) {
		match_.bridges = {{table(0x61), table(0x62)}};
		match_.score = {{table(0x63), table(0x64), table(0x65)}};
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(uint16_t(0x66 + i));
		match_.lives = table(0x6A);
		match_.other.level = table(0x6B);
		match_.other.bridges = {{table(0x6C), table(0x6D)}};
		match_.other.score = {{table(0x6E), table(0x6F), table(0x70)}};
		for (int i = 0; i < 4; i++) match_.other.checkpoint[i] = table(uint16_t(0x71 + i));
		match_.other.lives = table(0x75);
		plane_.state = table(0x76);
	}
	// $2B-$4C cleared (the river's part in restoreCheckpoint)
	plane_.accel = 0;
	scrollFrac_ = 0;
	bridgeFlash_ = 0;
	world_.objects.shell.frame = world_.objects.shell.x = world_.objects.shell.xFrac = 0;
	sound_.crashPitch = 0;   // $052C ($0500-$05FF cleared)
	// player-missile memory rows 1-$BB cleared (the sprites gone until prepare()), screen memory cleared
	spritesShown_ = false;
	river_.clear();
	screenOn_ = 0;                               // B101: colours off while clearing
	busy_ = resetFrames();
}

// A36E-A3B6: the explosion is over: the next player, the next plane, or game over.
// Returns true when the river is reset for the next plane (the loop continues after it).
bool Game::respawnOrGameOver() {
	if (match_.other.lives) match_.swapPlayers(world_.gen.s);
	match_.respawned = 1;
	if (match_.lives) {
		reset(false);                             // A3BC; then finishRespawn()
		resume_ = Resume::Respawn;
		return true;
	}
	// A37D: game over
	match_.updateHighScore(false);
	match_.updateHighScore(true);
	if (match_.twoPlayers()) {
		if (match_.player) match_.swapPlayers(world_.gen.s);
		for (int i = 0; i < 4; i++) panel_.chars[Panel::kLives + i] = rom_.playerTwoText(i) ^ 0xC0;
		match_.showOtherBridges(panel_, attract());
	} else {
		match_.showHighScore(panel_, attract());
	}
	// (In the original the high score check makes this frame's work end close to the vertical
	// blank: depending on the objects on screen, sub $B21A sets the frame counter to $FF just
	// before or just after that frame's VBI counts it, or even in the next frame. The port
	// does it before the VBI, and the "Game over" text (A3B4) after it, as seen most often.
	// On PAL the panel's display interrupt keeps the CPU 12 lines longer, and the VBI comes
	// first: it still sees the frame counter, the plane and the attract timer as they were.)
	if (pal_) {
		vbiSeesBeforeGameOver_ = true;
		beforeGameOver_[0] = frameCount_;
		beforeGameOver_[1] = plane_.state;
		beforeGameOver_[2] = attractTimer_;
	}
	gameOverScreen();
	gameOverText_ = true;                         // A3B4, after this frame's VBI (see vbi())
	return false;
}

// A3C1-A3E3: the river continues from the restart point, the plane in the middle of it.
void Game::finishRespawn() {
	RiverState &s = world_.gen.s;
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
	plane_.x = lo(s.bank + 0x26 + s.islandWidth);
	if (attract()) gameOverScreen();
}

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	world_.gen.s = RiverState();
	world_.gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	resetState();
	river_.clear();
	for (int i = 0; i < kPanelBytes; i++) panel_.chars[i] = rom_.panelText(i);
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	powerOnDone_ = 1;
	attractTimer_ = 1;
	plane_.state = kNoPlane;
}

}  // namespace rr
