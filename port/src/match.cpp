#include "match.h"

#include <cstring>

namespace rr {

void Match::setUp(RiverGen &gen, Panel &panel, bool attract) {
	if (twoPlayers()) {
		showOtherScore(panel, attract);
	} else {
		showHighScore(panel, attract);
	}
	panel.markPlayers(twoPlayers());
	const RiverGen::Start &start = gen.start(gameNumber >> 1);
	for (int i = 0; i < 4; i++) other.checkpoint[i] = gen.s.checkpoint[i] = start.checkpoint[i];
	gen.s.level = other.level = start.level;
	// the bridge counter, 4 BCD digits: past 9999 its last 4, as the counter of a game flown that
	// far would show (it goes round; and like it, the tanks fire only from 5 on again)
	const int b = start.bridge % 10000;
	bridges = other.bridges = {{uint8_t((b / 1000) << 4 | b / 100 % 10), uint8_t((b / 10 % 10) << 4 | b % 10)}};
}

void Match::selectNext(Panel &panel) {
	gameNumber = (gameNumber + 1) & 7;
	panel.showGameNumber(gameNumber);
	lives = 0;
	other.lives = 0;
	player = 0;
	score = other.score = Bcd<3>();
}

void Match::swapPlayers(RiverState &river) {
	if (!twoPlayers()) return;
	player ^= 1;
	PlayerData current;
	current.level = river.level;
	current.bridges = bridges;
	current.score = score;
	std::memcpy(current.checkpoint, river.checkpoint, 4);
	current.lives = lives;
	river.level = other.level;
	bridges = other.bridges;
	score = other.score;
	std::memcpy(river.checkpoint, other.checkpoint, 4);
	lives = other.lives;
	other = current;
}

Match::Points Match::addPoints(uint8_t points) {
	const uint8_t tenThousands = score.d[1] & 0xF0;
	if (score.add(points)) {         // past 999 999: the game ends
		score = {{0x99, 0x99, 0x99}};
		lives = 0;
		return Points::GameEnds;
	}
	if ((score.d[1] & 0xF0) != tenThousands && lives != 9) {
		lives++;
		return Points::BonusLife;
	}
	return Points::Added;
}

void Match::showLives(Panel &panel, uint8_t bonusSound) const {
	int shown = lives;
	if (shown && (bonusSound & 0x08)) shown--;
	panel.showLives(shown);
}

}  // namespace rr
