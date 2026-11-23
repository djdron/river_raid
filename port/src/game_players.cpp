// Choosing a game (SELECT, OPTION) and setting it up; the lives in the panel. The match itself
// (players, score, high scores) is Match (match.h). Part of Game (game.h).
#include "game.h"

#include "game_internal.h"

namespace rr {

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() { match_.setUp(world_.gen, panel_, attract()); }

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	match_.selectNext(panel_);
	idleTime_ = 0;
	attractFlags_ = 0;
	sound_.startBeep = 2;
	sound_.missile = 0;
	world_.objects.shell.timer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	attractTimer_ = 0x25;
	selectDelay_ = 0x1E;
}

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() { match_.showLives(panel_, sound_.bonus); }

}  // namespace rr
