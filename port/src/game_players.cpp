// Choosing a game (SELECT, OPTION) and setting it up; the lives in the panel. The match itself
// (players, score, high scores) is Match (match.h). Part of Game (game.h).
#include "game.h"

#include "game_internal.h"

namespace rr {

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() { match.setUp(world.gen, panel, attract(*this)); }

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	match.selectNext(panel);
	idleTime = 0;
	attractFlags = 0;
	sound.startBeep = 2;
	sound.missile = 0;
	world.objects.shell.timer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	attractTimer = 0x25;
	selectDelay = 0x1E;
}

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() { match.showLives(panel, sound.bonus); }

}  // namespace rr
