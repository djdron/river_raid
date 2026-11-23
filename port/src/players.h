// The players' counters: bridges and score (BCD, as the original keeps them) and lives, the
// other player's in a two-player game, and the high scores with their check bytes.
#pragma once
#include <cstdint>

#include "fixed.h"
#include "ram_map.h"

namespace rr {

// The data of one player ($60-$6A; the other player's copy is at $6B-$75). The active player's
// level and checkpoint are the river generator's (world.gen.s); this copy holds the other's.
struct PlayerData {
	uint8_t level = 0;          // $60
	Bcd<2> bridges;             // $61-$62 bridges destroyed
	Bcd<3> score;               // $63-$65
	uint8_t checkpoint[4] = {}; // $66-$69 restart point
	uint8_t lives = 0;          // $6A

	// The other player's copy, $0B higher: $6B-$75
	void mapRam(RamMap &m) {
		m.byte("other.level", 0x6B, level).bytes("other.bridges", 0x6C, bridges.d, 2).bytes("other.score", 0x6E, score.d, 3);
		m.bytes("other.checkpoint", 0x71, checkpoint, 4).byte("other.lives", 0x75, lives);
	}
};

// The high score of each pair of games (1-2, 3-4, 5-6, 7-8: one player and two players),
// 3 BCD bytes at 4-byte steps ($80-$8F), and 16 check bytes ($90-$9F) that keep them over a
// reset when right.
struct HighScores {
	uint8_t table[16] = {};
	uint8_t check[16] = {};

	void mapRam(RamMap &m) { m.bytes("highScore", 0x80, table, 16).bytes("highScoreCheck", 0x90, check, 16); }

	Bcd<3> of(int gameNumber) const {
		const uint8_t *h = &table[(gameNumber & 0x0E) * 2];
		return Bcd<3>{{h[0], h[1], h[2]}};
	}
	// sub $B261: the score becomes the high score of its pair of games if not lower; then the
	// check bytes are made again (sub $B298)
	void update(const Bcd<3> &score, int gameNumber) {
		if (score < of(gameNumber)) return;
		uint8_t *h = &table[(gameNumber & 0x0E) * 2];
		for (int i = 0; i < 3; i++) h[i] = score.d[i];
		for (int c = 15; c >= 0; c--) {
			int sum = 0, carry = 0;
			for (int i = 14; i >= 0; i--) {
				const int r = sum + table[i] + carry;
				carry = r > 0xFF;
				sum = (r & 0xFF) ^ c;
			}
			check[c] = uint8_t(sum);
		}
	}
};

}  // namespace rr
