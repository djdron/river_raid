// The part of the screen below the river: the status panel and the scrolling line.
//
// Screen layout (display list at $3F00, NTSC), in rows of the 384 x 240 screen buffer:
//     0..19   blank
//    20..179  river: 160 bitmap lines, the newest at the top (river_draw.h)
//   180..209  status panel on grey ($06): 3 rows of large characters (ANTIC mode 6), with
//             score, game number, fuel gauge, high score, bridge number
//   210..218  the scrolling line (mode 6, fine scrolled), and the rainbow Activision logo
//             made of two sprites; in the attract mode it scrolls the credits and
//             "Press START key", during play it shows the logo
//
// Character sets: the game builds two 512-byte sets in RAM at power-on (A0FB-A143) from the
// OS font and its own characters: $0800 for the panel (CHBASE 8), $0600 for the scrolling
// line (CHBASE 6).
#pragma once
#include <cstdint>

#include "os_font.h"
#include "river_gen.h"

namespace rr {

constexpr int kScreenWidth = 384;
constexpr int kScreenHeight = 240;
constexpr int kRiverTop = 20;
constexpr int kRiverRows = 160;

struct Charsets {
	uint8_t scroller[512];   // $0600
	uint8_t panel[512];      // $0800
};
Charsets buildCharsets(const Rom &rom, const uint8_t osFont[kOsFontBytes]);

// Panel memory $1000-$103B: 3 rows of 20 characters (bits 7-6 select the colour).
constexpr int kPanelBytes = 60;
// Panel as set up at power-on for a one-player game: the table at $BB70 (A0F0) with the
// second player's markers hidden (sub $AF80).
void initialPanel(const Rom &rom, uint8_t panel[kPanelBytes]);

// State of the scrolling line, updated once per frame in the vertical blank (B44B-B47F).
struct Scroller {
	uint8_t textLo = 0x4B;   // $3FB3 low byte of the text address ($B800 + textLo)
	uint8_t fineScroll = 0;  // $26   HSCROL, 7..0
	uint8_t logoX = 0;       // $27   horizontal position of the rainbow logo, 0 = hidden
	// attract: the line scrolls (the game is not running, $2A != 0)
	void step(bool attract);
};

// Draws rows 180..218. fuel is $5B (the gauge needle); logoX is the logo position the display
// uses this frame (the value before the frame's step()).
void drawStatusBar(const Rom &rom, const Charsets &cs, const uint8_t panel[kPanelBytes], uint8_t fuel,
		const Scroller &scroller, uint8_t logoX, uint8_t screen[kScreenHeight][kScreenWidth]);

}  // namespace rr
