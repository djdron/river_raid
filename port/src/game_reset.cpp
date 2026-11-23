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
	world.gen.s = RiverState();
	world.startGame(1, pal);
	resetState();
	river.clear();
	for (int i = 0; i < kPanelBytes; i++) panel.chars[i] = rom_.panelText(i);   // A0F0
	// the attract mode's new game: B0F1 with X = $29, AF80, then the respawn (A36E-A3E3) which
	// in the attract mode ends with sub $B21A; then frame 38
	attractFlags = 0x80;
	setUpGame();
	bullet.y = 1;
	match.lives = match.other.lives = 4;
	match.respawned = 1;
	powerOnDone = 1;
	screenOn = 1;
	attractFlags = 0x81;
	attractTimer = 0xFF;
	plane.state = kNoPlane;
	plane.speed = 0x80;
	plane.fuel = 0xFF;
	plane.fuelFrac = 0xFF;
	plane.x = 0x7C;
	colourBank = bankColour(world.gen.s, world.objects);
	// the scrolling line has been moving since frame 11 (VBI steps up to frame 38; 9 to 25 on PAL)
	scroller = Scroller();
	for (int f = scrollerStartFrame(pal); f <= attractStartFrame(pal); f++) {
		logoShown = scroller.logoX;
		scroller.step(true);
	}
	pushLine();
}

// A688-A17E: a new game (START), or the attract mode's river starting.
void Game::newGame() {
	match.player = match.gameNumber & 1;
	// A15D with X = $79: $79-$7F cleared
	match.respawned = 0;
	sound.event = sound.eventTime = sound.explosion = sound.fire = sound.bonus = sound.missile = 0;
	reset(true);   // the rest (AF80, A17E) when the reset ends: finishReset()
	resume_ = Resume::NewGame;
}

// sub $B0F1: reset of the river and the objects. full: a new game ($4D-$76 from the reset
// table: also level, scores, lives, the plane's state); otherwise a restart ($4D-$5F).
void Game::reset(bool full) {
	RiverState &s = world.gen.s;
	auto table = [&](uint16_t addr) { return rom_.resetValue(addr - 0x4D); };
	if (full) s.level = table(0x60);              // loaded before B16B reads it for the bank colour
	world.restoreCheckpoint();   // the river's part: $4D-$5F, $2B-$4C and $0500-$05FF, the bridge in slot 10
	// (restoreCheckpoint also continues from the checkpoint, A3C8; finishRespawn() does it again)
	bullet.y = table(0x56);
	plane.x = table(0x57);
	plane.xFrac = table(0x58);
	plane.speed = table(0x59);
	plane.fuel = table(0x5B);
	plane.fuelFrac = table(0x5C);
	plane.frame = table(0x5E);
	if (full) {
		match.bridges = {{table(0x61), table(0x62)}};
		match.score = {{table(0x63), table(0x64), table(0x65)}};
		for (int i = 0; i < 4; i++) s.checkpoint[i] = table(uint16_t(0x66 + i));
		match.lives = table(0x6A);
		match.other.level = table(0x6B);
		match.other.bridges = {{table(0x6C), table(0x6D)}};
		match.other.score = {{table(0x6E), table(0x6F), table(0x70)}};
		for (int i = 0; i < 4; i++) match.other.checkpoint[i] = table(uint16_t(0x71 + i));
		match.other.lives = table(0x75);
		plane.state = table(0x76);
	}
	// $2B-$4C cleared (the river's part in restoreCheckpoint)
	plane.accel = 0;
	scrollFrac = 0;
	bridgeFlash = 0;
	world.objects.shell.frame = world.objects.shell.x = world.objects.shell.xFrac = 0;
	sound.crashPitch = 0;   // $052C ($0500-$05FF cleared)
	// player-missile memory rows 1-$BB cleared (the sprites gone until prepare()), screen memory cleared
	spritesShown = false;
	river.clear();
	screenOn = 0;                               // B101: colours off while clearing
	busy_ = resetFrames();
}

// A36E-A3B6: the explosion is over: the next player, the next plane, or game over.
// Returns true when the river is reset for the next plane (the loop continues after it).
bool Game::respawnOrGameOver() {
	if (match.other.lives) match.swapPlayers(world.gen.s);
	match.respawned = 1;
	if (match.lives) {
		reset(false);                             // A3BC; then finishRespawn()
		resume_ = Resume::Respawn;
		return true;
	}
	// A37D: game over
	match.updateHighScore(false);
	match.updateHighScore(true);
	if (match.twoPlayers()) {
		if (match.player) match.swapPlayers(world.gen.s);
		for (int i = 0; i < 4; i++) panel.chars[Panel::kLives + i] = rom_.playerTwoText(i) ^ 0xC0;
		match.showOtherBridges(panel, attract(*this));
	} else {
		match.showHighScore(panel, attract(*this));
	}
	// (In the original the high score check makes this frame's work end close to the vertical
	// blank: depending on the objects on screen, sub $B21A sets the frame counter to $FF just
	// before or just after that frame's VBI counts it, or even in the next frame. The port
	// does it before the VBI, and the "Game over" text (A3B4) after it, as seen most often.
	// On PAL the panel's display interrupt keeps the CPU 12 lines longer, and the VBI comes
	// first: it still sees the frame counter, the plane and the attract timer as they were.)
	if (pal_) {
		vbiSeesBeforeGameOver_ = true;
		beforeGameOver_[0] = frameCount;
		beforeGameOver_[1] = plane.state;
		beforeGameOver_[2] = attractTimer;
	}
	gameOverScreen();
	gameOverText_ = true;                         // A3B4, after this frame's VBI (see vbi())
	return false;
}

// A3C1-A3E3: the river continues from the restart point, the plane in the middle of it.
void Game::finishRespawn() {
	RiverState &s = world.gen.s;
	s.rndA = s.checkpoint[0];
	s.rndB = s.checkpoint[1];
	s.rndC = s.checkpoint[2];
	s.bank = s.checkpoint[3];
	plane.x = lo(s.bank + 0x26 + s.islandWidth);
	if (attract(*this)) gameOverScreen();
}

// A000-A17C: power-on.
void Game::powerOn(bool pal) {
	pal_ = pal;
	world.gen.s = RiverState();
	world.gen.s.depotLevelLimit = pal ? 0x2F : 0x2E;
	resetState();
	river.clear();
	for (int i = 0; i < kPanelBytes; i++) panel.chars[i] = rom_.panelText(i);
	// A15D (X = $09): zero page cleared, then a new game's reset
	reset(true);
	setUpGame();
	powerOnDone = 1;
	attractTimer = 1;
	plane.state = kNoPlane;
}

}  // namespace rr
