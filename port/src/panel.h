// The status panel's 60 characters below the river ($1000-$103B): score, the high score or the
// other player's, the bridge counter, the game number, the lives as plane symbols. The game
// writes them as the original does; drawStatusBar (status_bar.h) draws them.
#pragma once
#include <cstdint>

#include "fixed.h"
#include "ram_map.h"
#include "status_bar.h"

namespace rr {

struct Panel {
	// character positions (40 per row, the second row from $28)
	static constexpr int kScore = 0x02;
	static constexpr int kPlayer1Mark = 0x0B, kPlayer2Mark = 0x12;
	static constexpr int kHighScore = 0x0C;        // or the other player's score
	static constexpr int kGameNumber = 0x2A;
	static constexpr int kLives = 0x33;            // 9 plane symbols; "GAME OVER" text of player 2 there
	static constexpr int kOtherBridges = 0x37;
	static constexpr int kBridges = kPanelBridges;
	static constexpr uint8_t kLifeSymbol = 0x9C;
	// digit characters: '0' in each of the panel's colours
	static constexpr uint8_t kDigitsScore = 0x10, kDigitsOther = 0x50, kDigitsHighScore = 0xD0;
	static constexpr uint8_t kDigitsBridges = rr::kDigitsBridges;

	uint8_t chars[kPanelBytes] = {};

	void mapRam(RamMap &m) { m.bytes("panel", 0x1000, chars, kPanelBytes); }

	// B086-B0C6: BCD digits from `at`, leading zeros blank
	template <int N> void print(const Bcd<N> &bcd, int at, uint8_t digitZero) {
		bool leading = true;
		for (int k = 0; k < 2 * N; k++) {
			const uint8_t digit = uint8_t(bcd.digit(k));
			if (leading && digit == 0) {
				chars[at++] = 0;
			} else {
				leading = false;
				chars[at++] = uint8_t(digit + digitZero);
			}
		}
	}
	// sub $AF5B: `shown` plane symbols
	void showLives(int shown) {
		for (int i = 0; i < 9; i++) chars[kLives + i] = i < shown ? kLifeSymbol : 0;
	}
	// sub $AF80: the one- or two-player markers
	void markPlayers(bool twoPlayers) {
		chars[kPlayer1Mark] = twoPlayers ? 0x4F : 0xDB;
		chars[kPlayer2Mark] = twoPlayers ? 0x50 : 0xD0;
	}
	// A699: the game number, digit 1-8
	void showGameNumber(uint8_t gameNumber) {
		chars[kGameNumber] = 0;
		chars[kGameNumber + 1] = uint8_t(gameNumber + 0xD1);
	}
};

}  // namespace rr
