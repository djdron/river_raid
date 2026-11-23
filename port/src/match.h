// The match: which game is played, whose turn it is, the score, bridges and lives of the player
// whose turn it is and of the other one, the high scores; and how they show in the status panel.
// Addresses $09, $61-$6A, $6B-$75, $77, $79, $80-$9F.
#pragma once
#include <cstdint>

#include "fixed.h"
#include "panel.h"
#include "players.h"
#include "ram_map.h"
#include "river_gen.h"

namespace rr {

struct Match {
	uint8_t gameNumber = 0;     // $09 0..7 (shown as 1..8); odd: two players
	uint8_t player = 0;         // $77 whose turn it is (0 or 1)
	Bcd<2> bridges;             // $61-$62 bridges passed
	Bcd<3> score;               // $63-$65
	uint8_t lives = 0;          // $6A planes left
	PlayerData other;           // $6B-$75 the other player in a two-player game
	uint8_t respawned = 0;      // $79 1 once a plane of this game has been lost (the panel marks the player)
	HighScores highScores;      // $80-$9F

	bool twoPlayers() const { return (gameNumber & 1) != 0; }

	// sub $AF80: one- or two-player markers in the panel and the starting point of the game
	// (the river's level and restart point, both players' bridges). `attract`: no numbers shown.
	void setUp(RiverGen &gen, Panel &panel, bool attract);
	// A69B-A6BA: SELECT or OPTION: the next game number, nobody's lives or score yet.
	void selectNext(Panel &panel);
	// sub $B247: two-player game: the other player's turn (the river's level and restart point
	// change hands too).
	void swapPlayers(RiverState &river);
	// sub $B261: the score (this or the other player's) becomes the high score of this pair of
	// games if not lower.
	void updateHighScore(bool otherPlayer) { highScores.update(otherPlayer ? other.score : score, gameNumber); }

	// A547: points (BCD); every 10 000 points a new plane, up to 9.
	enum class Points { Added, BonusLife, GameEnds };
	Points addPoints(uint8_t points);

	// The panel. Numbers are not shown in the attract mode (B086-B0C6).
	void showLives(Panel &panel, uint8_t bonusSound) const;   // sub $AF5B: the newest blinks while the bonus sounds
	void showBridges(Panel &panel, bool attract) const { show(panel, attract, bridges, Panel::kBridges, Panel::kDigitsBridges); }       // sub $B05C
	void showScore(Panel &panel, bool attract) const { show(panel, attract, score, Panel::kScore, Panel::kDigitsScore); }              // sub $B07C
	// sub $B068: player 2's score, where the high score is in a one-player game
	void showOtherScore(Panel &panel, bool attract) const { show(panel, attract, score, Panel::kHighScore, Panel::kDigitsOther); }
	void showHighScore(Panel &panel, bool attract) const {                                                                              // sub $B06E
		show(panel, attract, highScores.of(gameNumber), Panel::kHighScore, Panel::kDigitsHighScore);
	}
	void showOtherBridges(Panel &panel, bool attract) const { show(panel, attract, other.bridges, Panel::kOtherBridges, Panel::kDigitsOther); }

	void mapRam(RamMap &m) {
		m.byte("gameNumber", 0x09, gameNumber).byte("player", 0x77, player).byte("respawned", 0x79, respawned);
		m.bytes("bridges", 0x61, bridges.d, 2).bytes("score", 0x63, score.d, 3).byte("lives", 0x6A, lives);
		other.mapRam(m);
		highScores.mapRam(m);
	}

private:
	template <int N> static void show(Panel &panel, bool attract, const Bcd<N> &bcd, int at, uint8_t digitZero) {
		if (!attract) panel.print(bcd, at, digitZero);
	}
};

}  // namespace rr
