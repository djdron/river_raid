// River viewer (window, sound and input through SDL: sdl/host.h): the game's attract mode, the flight over the river that the original
// shows when nobody presses START.
//
// usage: river_view [rom] [-os file] [-pal] [-colours ntsc|pal] [-palette altirra|atari800] [-seed N] [-random N]
//   rom: path to the cartridge image; by default rom/River_Raid.rom is looked for in the
//   current folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -seed N: another river (and objects): the river generator starts every game from N
//            (1..16777215); games 3-8 start at its bridges 5, 20, 50; difficulty grows as usual
//   -random N: start of POKEY's random counter (otherwise taken from the clock, as on the real
//   machine, where it depends on the moment of power-on).
//
// What it shows, as the original's attract mode does (game 1): the game running without a
// plane (game.h): the river scrolls down 1 line per frame (60 frames a second), ships and
// helicopters wake up and move, tanks drive to the bridges and fire, helicopters fire
// missiles; below, the status panel and the scrolling line with the credits, "Press START key"
// and the rainbow Activision logo. It starts on an empty screen, like after power-on.
//
// Keys:
//   Space          pause / continue
//   Up / Down      faster / slower, through a stop to flying backwards (x1/4 .. x16 either way)
//   mouse wheel    scroll along the river by hand (PgUp / PgDn by a screen)
//   Backspace      back to the newest line, normal speed
//   B / Shift+B    next / previous bridge
//   Home           start again
//   F1 / F2 / F3   sound test: explosion / firing / score sound, through the game's own sound
//                  driver (the attract mode itself is silent, as in the original)
//   F              full screen
//   Esc            leave full screen
//   C / V          NTSC / PAL colours; Altirra / atari800 colour model
// A mouse click or going full screen hides the mouse pointer until the mouse moves. Closing the window (or Alt+F4,
// Cmd+Q) quits.
//
// Looking back shows the river from a map of the lines as they entered the screen, with the
// objects where they were placed; flying forward again replays it up to the present, where the
// game continues. The game, and the scrolling line, only run in the present.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "game.h"
#include "os_font.h"
#include "pokey_random.h"
#include "pokey_sound.h"
#include "river_draw.h"
#include "river_gen.h"
#include "status_bar.h"

#include "host.h"
#include "host_main.h"

namespace {

// Part of the 384 x 240 screen buffer shown in the window: everything that is ever drawn.
constexpr int kViewLeft = 10, kViewWidth = 364;
constexpr int kViewTop = 0, kViewHeight = rr::kScreenHeight;
constexpr int kScale = 3;   // initial window scale
// Width of a column relative to the height of a line on an NTSC TV: a column is half a
// colour clock, which is a little narrower than a scanline is high.
constexpr double kPixelAspect = 0.86;
constexpr double kFrameRate = 59.92;   // NTSC Atari
constexpr int kSampleRate = 48000;

// The attract mode: the game running without a player, from power-on, plus the status bar.
class Attract {
public:
	Attract(const rr::Rom &rom, const uint8_t osFont[rr::kOsFontBytes], bool pal, uint32_t seed)
		: rom_(rom), game_(rom), pal_(pal), seed_(seed), charsets_(rr::buildCharsets(rom, osFont)) {
		rr::initialPanel(rom, panel_);
		game_.afterLine = [this](const uint8_t *line) {
			// look-back map: each line as it enters the screen, objects where they were placed
			uint8_t row[rr::kRowWidth];
			rr::drawScreenLine(rom_, game_.gen.s, line, row);
			rows_.insert(rows_.end(), row, row + rr::kRowWidth);
			level_.push_back(game_.gen.s.level);
		};
		restart();
	}

	void restart() {
		rows_.clear();
		level_.clear();
		scroller_ = rr::Scroller();
		logoShown_ = 0;
		frame_ = 0;
		random_ = rr::PokeyRandom(seed_);
		pokey_ = rr::PokeySound(kSampleRate);
		audio_.clear();
		std::memset(live_, 0, sizeof live_);
		running_ = false;
	}

	// One frame of the original. Timing after power-on as in the original: the scrolling line
	// starts moving at frame 11, the game (and its river) at frame 38.
	void frame() {
		frame_++;
		if (frame_ == kFirstRiverFrame) {
			game_.startAttract(pal_);
			running_ = true;
		} else if (running_) {
			game_.prepare();
			game_.display();
			game_.drawRiver(live_);
			game_.logic(rr::Input(), [this] { return random_.read(192, 13); });   // A205, after scanline 160
			game_.vbi([this] { return random_.read(248, 30); });
		}
		// sound: the registers the VBI set at the end of the previous frame play for one frame
		pokey_.run(rr::PokeyRandom::kCyclesPerLine * rr::PokeyRandom::kLinesPerFrame, audio_);
		for (int c = 0; c < 4; c++) {
			pokey_.write(2 * c, game_.g.pokey.audf[c]);
			pokey_.write(2 * c + 1, game_.g.pokey.audc[c]);
		}
		random_.nextFrame();
		logoShown_ = scroller_.logoX;   // the display uses the position from before the step
		if (frame_ >= kFirstScrollerFrame) scroller_.step(true);
	}

	// Sound test: start one of the game's sound effects through its own sound driver.
	// (The attract mode itself is silent, as in the original.)
	void soundTest(int which) {
		if (!running_) return;
		switch (which) {
		case 0: game_.g.soundExplosion = 0x17; break;   // explosion, as A4E6 sets it
		case 1: game_.g.soundFire = 0x0F; break;        // firing (A5A6)
		case 2: game_.g.soundBonus = 0x40; break;       // score (A587)
		default: break;
		}
	}
	// Samples generated so far (taken by the caller).
	std::vector<int16_t> &audio() { return audio_; }

	long lines() const { return long(level_.size()); }
	// First line of the bridge section before / after `line` (-1 if none generated yet).
	long bridgeBefore(long line) const {
		for (long i = std::min(line, lines() - 1); i > 0; i--)
			if (level_[size_t(i)] != level_[size_t(i - 1)] && i < line) return i;
		return 0;
	}
	long bridgeAfter(long line) const {
		for (long i = std::max(line + 1, 1L); i < lines(); i++)
			if (level_[size_t(i)] != level_[size_t(i - 1)]) return i;
		return -1;
	}
	int level(long line) const { return line >= 0 && line < lines() ? level_[size_t(line)] : 0; }
	int currentLevel() const { return game_.gen.s.level; }

	// Draws the screen. back = 0: the live frame, with the objects moving; back > 0: the river
	// `back` lines earlier, from the map of lines as they entered the screen.
	void draw(long back, uint8_t screen[rr::kScreenHeight][rr::kScreenWidth]) const {
		std::memset(screen, 0, size_t(rr::kScreenHeight) * rr::kScreenWidth);
		if (back == 0) {
			std::memcpy(screen[rr::kRiverTop], live_[rr::kRiverTop], size_t(rr::kRiverRows) * rr::kRowWidth);
		} else {
			const long newest = lines() - 1 - back;
			for (int k = 0; k < rr::kRiverRows; k++) {
				const long i = newest - k;
				if (i < 0) break;
				std::memcpy(screen[rr::kRiverTop + k], &rows_[size_t(i) * rr::kRowWidth], rr::kRowWidth);
			}
		}
		rr::drawStatusBar(rom_, charsets_, panel_, 0xFF, scroller_, logoShown_, screen);   // full tank
	}

	void setRiverSeed(uint32_t seed) { game_.gen.setSeed(seed); }   // see RiverGen::seed

private:
	const rr::Rom &rom_;
	rr::Game game_;
	bool pal_;
	uint32_t seed_;
	rr::PokeyRandom random_;
	rr::PokeySound pokey_{kSampleRate};
	std::vector<int16_t> audio_;
	bool running_ = false;
	rr::Charsets charsets_;
	uint8_t panel_[rr::kPanelBytes];
	rr::Scroller scroller_;
	uint8_t logoShown_ = 0;
	long frame_ = 0;
	static constexpr long kFirstScrollerFrame = 11, kFirstRiverFrame = 38;
	uint8_t live_[rr::kScreenHeight][rr::kRowWidth];   // the river area of the current frame
	std::vector<uint8_t> rows_;    // every line as it entered the screen, kRowWidth colour codes each
	std::vector<uint8_t> level_;   // bridge number of each line
};

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-shot") || !std::strcmp(argv[i], "-seed") ||
				!std::strcmp(argv[i], "-random") ||
				!std::strcmp(argv[i], "-colours") || !std::strcmp(argv[i], "-palette"))
				i++;
			continue;
		}
		return argv[i];
	}
	std::string up;
	for (int d = 0; d < 4; d++, up += "../") {
		const std::string p = up + "rom/River_Raid.rom";
		if (FILE *f = std::fopen(p.c_str(), "rb")) { std::fclose(f); return p; }
	}
	return "rom/River_Raid.rom";
}

void fail(const std::string &msg) { host::Host::showError("River Raid viewer", msg); }

}  // namespace

int main(int argc, char **argv) {
	bool pal = false;
	bool palColours = true;    // colours of a PAL machine (C switches to NTSC)
	rr::PaletteModel paletteModel = rr::PaletteModel::Altirra;   // V switches
	const char *osPath = nullptr;
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N saves frame N as BMP (.raw: screen buffer) and quits
	long shotFrame = 0;
	uint32_t seed = uint32_t(host::Host::ticksNs() ^ uint64_t(std::time(nullptr)));   // like POKEY at power-on
	uint32_t riverSeed = 0;            // -seed: 0 = the cartridge's river
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = palColours = true;
		else if (!std::strcmp(argv[i], "-colours") && i + 1 < argc) palColours = !std::strcmp(argv[++i], "pal");
		else if (!std::strcmp(argv[i], "-palette") && i + 1 < argc)
			paletteModel = !std::strcmp(argv[++i], "atari800") ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
		else if (!std::strcmp(argv[i], "-os") && i + 1 < argc) osPath = argv[++i];
		else if (!std::strcmp(argv[i], "-random") && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 0));
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) riverSeed = uint32_t(std::strtoul(argv[++i], nullptr, 0)) & 0xFFFFFF;
		else if (!std::strcmp(argv[i], "-shot") && i + 1 < argc) {
			shotPath = argv[++i];
			if (const char *colon = std::strrchr(shotPath, ':')) {
				shotFrame = std::atol(colon + 1);
				static std::string path;
				path.assign(shotPath, colon);
				shotPath = path.c_str();
			}
		}
	}

	const std::string romPath = findRom(argc, argv);
	rr::Rom rom;
	if (!rom.load(romPath)) {
		fail("Cannot read the ROM: " + romPath + "\nPass the path to the River Raid cartridge image as the first argument.");
		return 1;
	}
	uint8_t osFont[rr::kOsFontBytes];
	if (osPath) {
		if (!rr::loadOsFont(osPath, osFont)) {
			fail(std::string("Cannot read the OS font from ") + osPath + " (expected a 10 KB or 16 KB OS ROM or a 1 KB font)");
			return 1;
		}
	} else {
		rr::builtinOsFont(osFont);
	}

	host::Host window;
	if (!window.open("River Raid", kViewWidth, kViewHeight, kPixelAspect, kScale, shotPath ? 0 : kSampleRate)) {
		fail(window.error());
		return 1;
	}

	uint32_t palette[256];
	auto makePalette = [&]() {
		for (int c = 0; c < 256; c++) {
			uint8_t rgb[3];
			rr::atariRgb(uint8_t(c), rgb, palColours, paletteModel);
			palette[c] = (uint32_t(rgb[0]) << 16) | (uint32_t(rgb[1]) << 8) | rgb[2];
		}
	};
	makePalette();
	// the window title shows the colours in use
	auto showColours = [&]() {
		const std::string title = std::string("River Raid - ") + (palColours ? "PAL" : "NTSC") + " colours, " +
			rr::paletteName(paletteModel);
		window.setTitle(title);
	};
	showColours();

	Attract attract(rom, osFont, pal, seed);
	attract.setRiverSeed(riverSeed);
	static uint8_t screen[rr::kScreenHeight][rr::kScreenWidth];
	// Speed in original frames per real frame; negative = backwards.
	static const double kSpeeds[] = {-16, -8, -4, -2, -1, -0.5, -0.25, 0, 0.25, 0.5, 1, 2, 4, 8, 16};
	const int kSpeedCount = int(sizeof kSpeeds / sizeof kSpeeds[0]);
	int speedIndex = 10;   // x1
	double pending = 0;    // original frames still to run
	bool paused = false;
	long back = 0;         // look-back in lines
	uint64_t last = host::Host::ticksNs();
	std::string title;

	if (shotPath)
		for (long f = 0; f < shotFrame; f++) attract.frame();

	for (bool running = true; running;) {
		host::Event e;
		while (window.poll(e)) {
			using host::Key;
			if (e.type == host::Event::Type::Quit) running = false;
			else if (e.type == host::Event::Type::MouseWheel) back += long(e.wheel * 16);
			else if (e.type == host::Event::Type::KeyDown) {
				switch (e.key) {
				case Key::Space: paused = !paused; break;
				case Key::C: palColours = !palColours; makePalette(); showColours(); break;
				case Key::V:
					paletteModel = paletteModel == rr::PaletteModel::Altirra ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
					makePalette();
					showColours();
					break;
				case Key::Up: speedIndex = std::min(speedIndex + 1, kSpeedCount - 1); break;
				case Key::Down: speedIndex = std::max(speedIndex - 1, 0); break;
				case Key::PageUp: back += rr::kRiverRows; break;
				case Key::PageDown: back -= rr::kRiverRows; break;
				case Key::Backspace: back = 0; speedIndex = 10; break;
				case Key::Home: attract.restart(); back = 0; break;
				case Key::B: {
					// the bridge line is shown a little above the bottom of the river area; Shift+B just
					// after a bridge goes to the one before it
					constexpr long kMargin = 100;
					const long shown = attract.lines() - 1 - back - kMargin;
					if (e.shift) {
						back = attract.lines() - 1 - (attract.bridgeBefore(shown - 64) + kMargin);
					} else {
						long next = attract.bridgeAfter(shown);
						while (next < 0) {   // not generated yet: fly on
							for (int i = 0; i < 64; i++) attract.frame();
							next = attract.bridgeAfter(shown);
						}
						while (attract.lines() - 1 < next + kMargin) attract.frame();
						back = attract.lines() - 1 - (next + kMargin);
					}
					break;
				}
				case Key::F1: attract.soundTest(0); break;
				case Key::F2: attract.soundTest(1); break;
				case Key::F3: attract.soundTest(2); break;
				case Key::F: window.setFullscreen(!window.fullscreen()); break;
				case Key::Escape: if (window.fullscreen()) window.setFullscreen(false); break;
				default: break;
				}
			}
		}

		const uint64_t now = host::Host::ticksNs();
		const double dt = std::min(double(now - last) / 1e9, 0.1);
		last = now;
		if (!paused && !shotPath) {
			pending += kSpeeds[speedIndex] * dt * kFrameRate;
			for (; pending >= 1; pending--) {
				if (back > 0) back--;   // replay lines already generated
				else attract.frame();
			}
			for (; pending <= -1; pending++) back++;
		}
		back = std::clamp(back, 0L, std::max(0L, attract.lines() - 1));
		{
			// at higher speeds or after a stall, playSound drops what is queued
			std::vector<int16_t> &samples = attract.audio();
			window.playSound(samples.data(), samples.size());
			samples.clear();
		}
		if (attract.lines() > 0 && back == attract.lines() - 1 && kSpeeds[speedIndex] < 0) pending = 0;

		attract.draw(back, screen);
		uint32_t *pixels = window.pixels();
		for (int y = 0; y < kViewHeight; y++)
			for (int x = 0; x < kViewWidth; x++) pixels[y * kViewWidth + x] = palette[screen[kViewTop + y][kViewLeft + x]];

		if (shotPath && std::strstr(shotPath, ".raw")) {   // screen buffer: 240 rows of 384 colour codes
			if (FILE *f = std::fopen(shotPath, "wb")) {
				std::fwrite(screen, 1, sizeof screen, f);
				std::fclose(f);
			}
		} else if (shotPath) {
			window.savePicture(shotPath);
		}
		if (shotPath) running = false;
		window.present();

		char buf[160];
		const long shown = attract.lines() - 1 - back;
		std::snprintf(buf, sizeof buf, "River Raid - bridge %d - line %ld - speed x%g%s%s", attract.level(shown), shown,
			kSpeeds[speedIndex], back ? " - looking back" : "", paused ? " - paused" : "");
		if (title != buf) {
			title = buf;
			window.setTitle(buf);
		}
	}

	return 0;
}
