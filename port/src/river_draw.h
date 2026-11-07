// Drawing a generated line the way the game shows it.
//
// The bitmap from RiverGen::nextLine() holds only 2-bit colour indices. The game turns them
// into colours per scanline, in its display list interrupt (DLI, $B4xx) and vertical blank:
//   - index 0 is the background (black), 2 is the bank edge ($20), 3 is the water ($84);
//   - index 1 is the bank. Its shade depends on the section ($D2 or $D6, see bankColour());
//     next to a bridge the DLI replaces it with the road colours from the table at $B4E5;
//   - each object slot is drawn with one hardware sprite (player 3), re-used down the
//     screen: the DLI copies one row of the object's shape and colour per scanline.
//
// drawScreenLine() draws the line as it looks when it enters the screen (row 20), which is
// what a picture of the whole river needs: as the river scrolls, the objects scroll with it.
// Objects the game itself moves (ships, helicopters, the tank on the road) are drawn where the
// generator placed them; with the slot positions of the running game the result matches the
// original screen exactly, apart from the tank on the road (drawn by the game logic).
#pragma once
#include <cstdint>

#include "river_gen.h"

namespace rr {

// One screen row in the layout of the emulator's screen buffer: 384 half colour clocks,
// a mode E pixel is 2 wide, columns kVisibleFirst..kVisibleLast are on screen.
constexpr int kRowWidth = 384;
constexpr int kVisibleFirst = 24;
constexpr int kVisibleLast = 359;

// Bank colour (COLPF0) of a line when it enters the screen (sub $AFD0).
uint8_t bankColour(const RiverState &s);

// Draws generated line `line` (state `s` right after generating it) into out[kRowWidth]
// as Atari colour codes (hue << 4 | luminance).
void drawScreenLine(const Rom &rom, const RiverState &s, const uint8_t line[kLineBytes], uint8_t out[kRowWidth]);

// Approximate NTSC colour of an Atari colour code, 8-bit RGB.
void atariRgb(uint8_t colour, uint8_t rgb[3]);

}  // namespace rr
