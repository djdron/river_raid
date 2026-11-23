// The match: which game is played, whose turn it is, the score, bridges and lives of the player
// whose turn it is and of the other one, the high scores; Scoreboard shows them in the status
// panel. Addresses $09, $61-$6A, $6B-$75, $77, $79, $80-$9F.
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

	void mapRam(RamMap &m) {
		m.byte("gameNumber", 0x09, gameNumber).byte("player", 0x77, player).byte("respawned", 0x79, respawned);
		m.bytes("bridges", 0x61, bridges.d, 2).bytes("score", 0x63, score.d, 3).byte("lives", 0x6A, lives);
		other.mapRam(m);
		highScores.mapRam(m);
	}
};

// The match's numbers and lives in the status panel; numbers are not shown in the attract mode
// (B086-B0C6).
class Scoreboard {
public:
	Scoreboard(const Match &match, Panel &panel, bool attract) : match_(match), panel_(panel), attract_(attract) {}

	void lives(uint8_t bonusSound) const;   // sub $AF5B: the newest blinks while the bonus sounds
	void bridges() const { show(match_.bridges, Panel::kBridges, Panel::kDigitsBridges); }   // sub $B05C
	// sub $B07C: the score of the player whose turn it is, on the left
	void score() const { show(match_.score, Panel::kScore, Panel::kDigitsScore); }
	// sub $B068: the same score ($63-$65 is always the one of the player whose turn it is), on the
	// right in player 2's colours, where a one-player game shows the high score: on player 2's
	// turn, and when a two-player game is set up
	void player2Score() const { show(match_.score, Panel::kHighScore, Panel::kDigitsOther); }
	void highScore() const { show(match_.highScores.of(match_.gameNumber), Panel::kHighScore, Panel::kDigitsHighScore); }   // sub $B06E
	// A3AA-A3AE: at game over in a two-player game, the other player's bridges where the lives were
	void otherBridges() const { show(match_.other.bridges, Panel::kOtherBridges, Panel::kDigitsOther); }

private:
	template <int N> void show(const Bcd<N> &bcd, int at, uint8_t digitZero) const {
		if (!attract_) panel_.print(bcd, at, digitZero);
	}
	const Match &match_;
	Panel &panel_;
	bool attract_;
};

}  // namespace rr
