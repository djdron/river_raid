#include "status_bar.h"

#include <cstring>

namespace rr {
namespace {

// Screen buffer column of horizontal position (colour clock) 0 is -64: a colour clock is
// 2 columns, and position 32 is the left edge of the buffer.
int columnOf(int hpos) { return 2 * (hpos - 32); }

// Visible columns: the panel background spans 24..359, characters and sprites 10..373.
constexpr int kBackFirst = 24, kBackLast = 359;
constexpr int kVisFirst = 10, kVisLast = 373;

void plot(uint8_t screen[kScreenHeight][kScreenWidth], int y, int x, int w, uint8_t c) {
	for (int i = x; i < x + w; i++)
		if (i >= kVisFirst && i <= kVisLast) screen[y][i] = c;
}

// One row of ANTIC mode 6 characters: 8 x 8 pixels, each pixel one colour clock (2 columns),
// bits 7-6 of a character code select one of the four playfield colours.
void drawMode6(uint8_t screen[kScreenHeight][kScreenWidth], int top, const uint8_t *text, int count,
               const uint8_t *charset, const uint8_t colours[4], int left) {
	for (int c = 0; c < count; c++) {
		const uint8_t code = text[c];
		const uint8_t *glyph = charset + (code & 0x3F) * 8;
		for (int row = 0; row < 8; row++)
			for (int bit = 0; bit < 8; bit++)
				if (glyph[row] & (0x80 >> bit)) plot(screen, top + row, left + c * 16 + bit * 2, 2, colours[code >> 6]);
	}
}

// Rows of the panel and the scrolling line.
constexpr int kPanelTop = 181;                         // grey from here to 209
constexpr int kPanelRows[3] = {182, 191, 199};         // mode 6 rows (blank line after the first)
constexpr int kPanelBottom = 209;
constexpr int kScrollerTop = 211;
constexpr int kNormalLeft = 32;                        // normal-width playfield (DMACTL $3E)

}  // namespace

Charsets buildCharsets(const Rom &rom, const uint8_t osFont[kOsFontBytes]) {
	// RAM $0600-$09FF as A0FB-A143 leaves it. The copy loop starts at byte 8, so character 0
	// of each OS page stays blank.
	uint8_t ram[0x400] = {};
	for (int x = 8; x < 256; x++) {
		ram[0x200 + x] = osFont[x];            // $0800 <- $E000: characters 0-31
		ram[0x300 + x] = osFont[0x100 + x];    // $0900 <- $E100: characters 32-63
		ram[x] = osFont[0x100 + x];            // $0600 <- $E100
		ram[0x100 + x] = osFont[0x300 + x];    // $0700 <- $E300: lower case
	}
	for (int x = 0; x < 0x40; x++) ram[0x0C0 + x] = rom.at(uint16_t(0xBEEE + x));   // Activision lettering
	for (int x = 0; x < 0x70; x++) ram[0x218 + x] = rom.at(uint16_t(0xBF2E + x));   // panel symbols, "0"
	for (int x = 0; x < 0x18; x++) ram[0x2D0 + x] = rom.at(uint16_t(0xBF9E + x));   // "P1", "P2"
	for (int x = 0; x < 0x30; x++) ram[0x1D0 + x] = rom.at(uint16_t(0xBFB6 + x));   // digits of "1982, 1983"
	Charsets cs;
	std::memcpy(cs.scroller, ram, 512);
	std::memcpy(cs.panel, ram + 0x200, 512);
	return cs;
}

void initialPanel(const Rom &rom, uint8_t panel[kPanelBytes]) {
	for (int i = 0; i < kPanelBytes; i++) panel[i] = rom.at(uint16_t(0xBB70 + i));
	panel[0x0B] = 0xDB;   // one player: second player's markers in colour 3 (black)
	panel[0x12] = 0xD0;
}

void Scroller::step(bool attract) {
	// B44B-B47F. During play the line shows the logo text at $B8E7 without scrolling.
	if (!attract) {
		logoX = 0x44;
		textLo = 0xE7;
		fineScroll = 0;
		return;
	}
	uint8_t logo = uint8_t(logoX - 1);   // the logo moves 1 colour clock per frame with the text
	if (fineScroll == 0) {
		// next character: 8 frames per character
		textLo++;
		if (textLo >= 0xD7) textLo = 0x4B;   // text $B84B-$B8D6, then from the start
		if (textLo < 0x7E || textLo >= 0x9B) logo = 0;   // logo only while its text passes
		else if (textLo == 0x7E) logo = 0xEC;              // enters at the right edge
		fineScroll = 7;
	} else {
		fineScroll--;
	}
	logoX = logo;
}

void drawStatusBar(const Rom &rom, const Charsets &cs, const uint8_t panel[kPanelBytes], uint8_t fuel,
                   const Scroller &scroller, uint8_t logoX, uint8_t screen[kScreenHeight][kScreenWidth]) {
	for (int y = kRiverTop + kRiverRows; y < kScreenHeight; y++) std::memset(screen[y], 0, kScreenWidth);

	// Panel (DLI B66B-B6BC): background $06, colours PF0 $1E, PF1 $8E, PF2 $1E, PF3 $00.
	// Playfield has priority over sprites (PRIOR 4).
	for (int y = kPanelTop; y <= kPanelBottom; y++)
		for (int x = kBackFirst; x <= kBackLast; x++) screen[y][x] = 0x06;
	// fuel needle: missile 1, 2 colour clocks wide, colour $1E, PM rows $CC-$D5,
	// horizontal position $5B / 8 + $5C (B527)
	const int needleX = columnOf(fuel / 8 + 0x5C);
	for (int y = 0xCC - 8; y <= 0xD5 - 8; y++) plot(screen, y, needleX, 4, 0x1E);
	static const uint8_t kPanelColours[4] = {0x1E, 0x8E, 0x1E, 0x00};
	for (int r = 0; r < 3; r++) drawMode6(screen, kPanelRows[r], panel + r * 20, 20, cs.panel, kPanelColours, kNormalLeft);

	// Rainbow logo (DLI B72A-B773): player 3 at quad width (data $FC) at logoX and player 2
	// (data $FF) starting $15 colour clocks right of it, one clock further left on each row,
	// both in the row colours from $B48D. PM data from sub $B0F1 (rows $DB-$E1).
	for (int k = 0; k < 7; k++) {
		const uint8_t colour = rom.at(uint16_t(0xB48D + 6 - k));
		const int y = kScrollerTop + k;
		for (int bit = 0; bit < 6; bit++) plot(screen, y, columnOf(logoX) + bit * 8, 8, colour);
		const int p2 = uint8_t(logoX + 0x15 - k);
		plot(screen, y, columnOf(p2), 16, colour);
	}

	// Scrolling line: wide playfield, characters from $B800 + textLo in colour $0C, shifted
	// right by the fine scroll (1 colour clock per step); playfield is drawn over the logo.
	uint8_t text[25];
	for (int i = 0; i < 25; i++) {
		const unsigned addr = 0xB800u + scroller.textLo + unsigned(i);
		text[i] = addr < 0xC000 ? rom.at(uint16_t(addr)) : 0;
	}
	static const uint8_t kScrollerColours[4] = {0x0C, 0x0C, 0x0C, 0x0C};
	drawMode6(screen, kScrollerTop, text, 25, cs.scroller, kScrollerColours, 2 + 2 * scroller.fineScroll);
}

}  // namespace rr
