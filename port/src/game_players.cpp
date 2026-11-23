// The players: setting up a game, choosing it, the two players' turns, the high scores, the
// numbers and lives in the panel. Part of Game (game.h).
#include "game.h"

#include "game_internal.h"
#include "objects.h"

#include <cstdlib>
#include <cstring>

namespace rr {

// sub $AF80: one- or two-player markers in the panel and the starting point of the game.
void Game::setUpGame() {
	if (gameNumber & 1) {
		showOtherScore();
	} else {
		showHighScore();
	}
	panel.markPlayers(gameNumber & 1);
	const RiverGen::Start &start = world.gen.start(gameNumber >> 1);
	for (int i = 0; i < 4; i++) other.checkpoint[i] = world.gen.s.checkpoint[i] = start.checkpoint[i];
	world.gen.s.level = other.level = start.level;
	// the bridge counter, 4 BCD digits: past 9999 its last 4, as the counter of a game flown that
	// far would show (it goes round; and like it, the tanks fire only from 5 on again)
	const int b = start.bridge % 10000;
	bridges = other.bridges = {{uint8_t((b / 1000) << 4 | b / 100 % 10), uint8_t((b / 10 % 10) << 4 | b % 10)}};
}

// A699-A6DC: SELECT or OPTION: the next game number, everything back to the start.
void Game::selectGame() {
	gameNumber = (gameNumber + 1) & 7;
	panel.showGameNumber(gameNumber);
	idleTime = 0;
	attractFlags = 0;
	lives = 0;
	other.lives = 0;
	player = 0;
	sound.startBeep = 2;
	score = other.score = Bcd<3>();
	sound.missile = 0;
	world.objects.shell.timer = 0xFF;
	setUpGame();
	showLives();
	gameOverScreen();
	attractTimer = 0x25;
	selectDelay = 0x1E;
}

// sub $B247: two-player game: the other player's turn.
void Game::swapPlayers() {
	if (!(gameNumber & 1)) return;
	player ^= 1;
	RiverState &s = world.gen.s;
	PlayerData current;
	current.level = s.level;
	current.bridges = bridges;
	current.score = score;
	std::memcpy(current.checkpoint, s.checkpoint, 4);
	current.lives = lives;
	s.level = other.level;
	bridges = other.bridges;
	score = other.score;
	std::memcpy(s.checkpoint, other.checkpoint, 4);
	lives = other.lives;
	other = current;
}

// sub $B261: the score (this or the other player's) becomes the high score of this pair of
// games if not lower.
void Game::updateHighScore(bool otherPlayer) { highScores.update(otherPlayer ? other.score : score, gameNumber); }

// sub $AF5B: the lives as plane symbols in the panel (the newest blinks while the bonus sounds).
void Game::showLives() {
	int shown = lives;
	if (shown && (sound.bonus & 0x08)) shown--;
	panel.showLives(shown);
}

// B086-B0C6: BCD bytes as digits in the panel, leading zeros blank; nothing in the attract mode.
template <int N> void Game::printBcd(const Bcd<N> &bcd, int at, uint8_t digitZero) {
	if (!attract(*this)) panel.print(bcd, at, digitZero);
}

// (also used by game_reset.cpp: the other player's bridges at a turn)
template void Game::printBcd(const Bcd<2> &, int, uint8_t);
template void Game::printBcd(const Bcd<3> &, int, uint8_t);

void Game::showBridges() { printBcd(bridges, Panel::kBridges, Panel::kDigitsBridges); }

void Game::showScore() { printBcd(score, Panel::kScore, Panel::kDigitsScore); }

void Game::showOtherScore() { printBcd(score, Panel::kHighScore, Panel::kDigitsOther); }

void Game::showHighScore() { printBcd(highScores.of(gameNumber), Panel::kHighScore, Panel::kDigitsHighScore); }

}  // namespace rr
