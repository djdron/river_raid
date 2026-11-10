// River viewer (window, sound and input through SDL: sdl/host.h): the game's attract mode, the flight over the river that the original
// shows when nobody presses START.
//
// usage: river_view [rom] [-os file] [-pal] [-colours ntsc|pal] [-palette altirra|atari800] [-border] [-filter sharp|nearest|linear] [-seed N] [-bridge N] [-random N]
//   rom: path to the cartridge image; by default rom/River_Raid.rom is looked for in the
//   current folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -seed N: another river (and objects): game 1's river starts from generator state N
//            (1..16777215); games 3-8 start at its bridges 5, 20, 50; difficulty grows as usual
//   -bridge N: every game starts at bridge N (1..9999) of the river, at that bridge's level
//   -random N: start of POKEY's random counter (otherwise taken from the clock, as on the real
//   machine, where it depends on the moment of power-on).
//   -border: show the whole frame with its black border, not just the game's picture
//   -filter: how the picture is scaled to the window: sharp (default: whole-number scale, then
//   smoothly to fit), nearest (blocky, pixels of slightly uneven size) or linear (blurred)
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
//   O              the game's picture only / the whole frame with its border
//   L              scaling: sharp / nearest / linear
// A mouse click or going full screen hides the mouse pointer until the mouse moves. Closing the window (or Alt+F4,
// Cmd+Q) quits.
//
// Looking back runs the attract mode again from a snapshot up to the moment asked for, so the
// past is shown as it was, objects moving, flying forwards or backwards; the sound plays only
// in the present.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <new>
#include <optional>
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
constexpr int kScale = 6;   // initial window scale (less if the screen is smaller)
// Width of a column relative to the height of a line on an NTSC TV: a column is half a
// colour clock, which is a little narrower than a scanline is high.
constexpr double kPixelAspect = 0.86;
constexpr double kFrameRate = 59.92;   // NTSC Atari
constexpr int kSampleRate = 48000;

// The attract mode: the game running without a player, from power-on, plus the status bar.
//
// The present is run once (head_); every line it generates is recorded with its level and
// bridge. Looking back runs the past again: snapshots of the whole machine are kept every
// kSnapshotFrames frames, and a second copy (replay_) is started from the nearest one and run
// to the line asked for, so any earlier moment is shown as it was, objects moving.
class Attract {
public:
	Attract(const rr::Rom &rom, const uint8_t osFont[rr::kOsFontBytes], bool pal, uint32_t seed)
		: rom_(rom), pal_(pal), seed_(seed), charsets_(rr::buildCharsets(rom, osFont)), head_(rom, seed) {
		rr::initialPanel(rom, panel_);
		restart();
	}

	void restart() {
		head_ = Machine(rom_, seed_);
		head_.game.gen.setRiver(riverSeed_, startBridge_);
		level_.clear();
		bridge_.clear();
		snapshots_.clear();
		replay_.reset();
		std::memset(live_, 0, sizeof live_);
		liveStale_ = false;
		pokey_ = rr::PokeySound(kSampleRate);
		audio_.clear();
	}

	// One frame of the original, in the present. fast: skip what is only seen or heard (the
	// picture and the sound), for flying ahead.
	void frame(bool fast = false) {
		if (head_.frame % kSnapshotFrames == 0 && head_.frame / kSnapshotFrames == long(snapshots_.size()))
			snapshots_.push_back(head_);
		liveStale_ = fast;
		head_.step(pal_, fast ? nullptr : live_, [this](const rr::RiverState &s) {
			// a new level starts with each bridge: count them as the game's BRIDGE counter would
			if (level_.empty()) bridgeCount_ = head_.game.gen.start(0).bridge;
			else if (s.level != level_.back()) bridgeCount_++;
			level_.push_back(s.level);
			bridge_.push_back(bridgeCount_);
		});
		// sound: the registers the VBI set at the end of the previous frame play for one frame
		if (!fast) pokey_.run(rr::PokeyRandom::kCyclesPerLine * rr::PokeyRandom::kLinesPerFrame, audio_);
		for (int c = 0; c < 4; c++) {
			pokey_.write(2 * c, head_.game.g.pokey.audf[c]);
			pokey_.write(2 * c + 1, head_.game.g.pokey.audc[c]);
		}
	}

	// Sound test: start one of the game's sound effects through its own sound driver.
	// (The attract mode itself is silent, as in the original.)
	void soundTest(int which) {
		if (!head_.running) return;
		switch (which) {
		case 0: head_.game.g.soundExplosion = 0x17; break;   // explosion, as A4E6 sets it
		case 1: head_.game.g.soundFire = 0x0F; break;        // firing (A5A6)
		case 2: head_.game.g.soundBonus = 0x40; break;       // score (A587)
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
	int bridge(long line) const { return line >= 0 && line < lines() ? bridge_[size_t(line)] : 0; }

	// Draws the screen at the moment line `back` lines before the newest one entered it: the
	// present for back = 0, otherwise that moment run again. The panel's BRIDGE counter shows
	// the bridge of the newest line on screen.
	void draw(long back, uint8_t screen[rr::kScreenHeight][rr::kScreenWidth]) {
		const Machine *m = &head_;
		const uint8_t (*live)[rr::kRowWidth] = live_;
		const long target = lines() - 1 - back;
		if ((back > 0 || liveStale_) && target >= 0) {   // (the present not drawn: run it again too)
			// run on from where the replay is, or start again from the nearest snapshot before
			if (!replay_ || replay_->lines - 1 > target || target - (replay_->lines - 1) > kSnapshotFrames) {
				size_t i = snapshots_.size();
				while (i > 1 && snapshots_[i - 1].lines - 1 >= target) i--;   // at least one frame to run
				replay_.emplace(snapshots_[i - 1]);
			}
			while (replay_->lines - 1 < target)   // draw only the frames that may reach the line
				replay_->step(pal_, replay_->lines >= target ? replayLive_ : nullptr, nullptr);
			m = &*replay_;
			live = replayLive_;
		}
		std::memset(screen, 0, size_t(rr::kScreenHeight) * rr::kScreenWidth);
		std::memcpy(screen[rr::kRiverTop], live[rr::kRiverTop], size_t(rr::kRiverRows) * rr::kRowWidth);
		uint8_t panel[rr::kPanelBytes];
		std::memcpy(panel, panel_, sizeof panel);
		if (lines() > 0) rr::showPanelBridge(panel, bridge(std::max(target, 0L)));
		rr::drawStatusBar(rom_, charsets_, panel, 0xFF, m->scroller, m->logoShown, screen);   // full tank
	}

	void setRiver(uint32_t seed, int bridge) {
		riverSeed_ = seed;
		startBridge_ = bridge;
		restart();
	}

private:
	static constexpr long kSnapshotFrames = 128;
	static constexpr long kFirstScrollerFrame = 11, kFirstRiverFrame = 38;

	// Everything that runs: the game, POKEY's random counter, the scrolling line. Copied whole
	// for a snapshot.
	struct Machine {
		rr::Game game;
		rr::PokeyRandom random;
		rr::Scroller scroller;
		uint8_t logoShown = 0;
		long frame = 0;
		long lines = 0;             // lines generated so far
		bool running = false;

		Machine(const rr::Rom &rom, uint32_t seed) : game(rom), random(seed) {}
		Machine(const Machine &) = default;
		Machine &operator=(Machine &&m) {   // Game holds a reference to the ROM: rebuild in place
			this->~Machine();
			new (this) Machine(m);
			return *this;
		}

		// One frame of the original. Timing after power-on as in the original: the scrolling
		// line starts moving at frame 11, the game (and its river) at frame 38. live: if not null, the
		// river area is drawn there. newLine: called with the generator state after each new line.
		void step(bool pal, uint8_t (*live)[rr::kRowWidth], const std::function<void(const rr::RiverState &)> &newLine) {
			frame++;
			game.afterLine = [this, &newLine](const uint8_t *) {
				lines++;
				if (newLine) newLine(game.gen.s);
			};
			if (frame == kFirstRiverFrame) {
				game.startAttract(pal);
				running = true;
			} else if (running) {
				game.prepare();
				game.display();
				if (live) game.drawRiver(live);
				// The original turns the screen off and stops after 128 x 2048 frames (about 73 minutes)
				// without input (A1F5, $78): the viewer keeps flying, as if a key had just been touched.
				game.g.idleTime = 0;
				game.logic(rr::Input(), [this] { return random.read(192, 13); });   // A205, after scanline 160
				game.vbi([this] { return random.read(248, 30); });
			}
			game.afterLine = nullptr;
			random.nextFrame();
			logoShown = scroller.logoX;   // the display uses the position from before the step
			if (frame >= kFirstScrollerFrame) scroller.step(true);
		}
	};

	const rr::Rom &rom_;
	bool pal_;
	uint32_t seed_;
	uint32_t riverSeed_ = 0;   // -seed, -bridge
	int startBridge_ = 0;
	rr::Charsets charsets_;
	uint8_t panel_[rr::kPanelBytes];
	Machine head_;                       // the present
	std::vector<Machine> snapshots_;     // head_ at frames 0, kSnapshotFrames, 2 * kSnapshotFrames...
	std::optional<Machine> replay_;      // the past, run again from a snapshot
	uint8_t live_[rr::kScreenHeight][rr::kRowWidth] = {};        // the present's river area
	uint8_t replayLive_[rr::kScreenHeight][rr::kRowWidth] = {};  // the replay's
	rr::PokeySound pokey_{kSampleRate};
	std::vector<int16_t> audio_;
	std::vector<uint8_t> level_;   // level of each line (the generator's $60)
	std::vector<int> bridge_;      // number of the bridge that starts each line's section, as on the panel
	int bridgeCount_ = 0;
	bool liveStale_ = false;   // the present's last frame ran without drawing
};

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-shot") || !std::strcmp(argv[i], "-seed") ||
				!std::strcmp(argv[i], "-bridge") || !std::strcmp(argv[i], "-random") ||
				!std::strcmp(argv[i], "-colours") || !std::strcmp(argv[i], "-palette") ||
				!std::strcmp(argv[i], "-filter"))
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
	host::Filter filter = host::Filter::Sharp;   // -filter (L switches)
	bool border = false;       // -border: the whole frame with its black border (O switches)
	bool palColours = true;    // colours of a PAL machine (C switches to NTSC)
	rr::PaletteModel paletteModel = rr::PaletteModel::Altirra;   // V switches
	const char *osPath = nullptr;
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N saves frame N as BMP (.raw: screen buffer) and quits
	long shotFrame = 0;
	uint32_t seed = uint32_t(host::Host::ticksNs() ^ uint64_t(std::time(nullptr)));   // like POKEY at power-on
	uint32_t riverSeed = 0;            // -seed: 0 = the cartridge's river
	int startBridge = 0;               // -bridge: 0 = the cartridge's starting bridges
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = palColours = true;
		else if (!std::strcmp(argv[i], "-border")) border = true;
		else if (!std::strcmp(argv[i], "-filter") && i + 1 < argc) {
			const char *name = argv[++i];
			filter = !std::strcmp(name, "nearest") ? host::Filter::Nearest
				: !std::strcmp(name, "linear") ? host::Filter::Linear : host::Filter::Sharp;
		}
		else if (!std::strcmp(argv[i], "-colours") && i + 1 < argc) palColours = !std::strcmp(argv[++i], "pal");
		else if (!std::strcmp(argv[i], "-palette") && i + 1 < argc)
			paletteModel = !std::strcmp(argv[++i], "atari800") ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
		else if (!std::strcmp(argv[i], "-os") && i + 1 < argc) osPath = argv[++i];
		else if (!std::strcmp(argv[i], "-random") && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 0));
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) riverSeed = uint32_t(std::strtoul(argv[++i], nullptr, 0)) & 0xFFFFFF;
		else if (!std::strcmp(argv[i], "-bridge") && i + 1 < argc) startBridge = std::atoi(argv[++i]);
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
	// the game's picture only, or the whole frame
	auto showBorder = [&]() {
		if (border) window.setView(0, 0, kViewWidth, kViewHeight);
		else window.setView(rr::kPictureLeft - kViewLeft, rr::kPictureTop - kViewTop, rr::kPictureWidth, rr::kPictureHeight);
	};
	showBorder();   // before open(): the window is made in the size of the part shown
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
	// the window title shows where the river is, then the colours and the scaling in use
	std::string colours;
	auto showColours = [&]() {
		colours = std::string(palColours ? "PAL" : "NTSC") + " colours, " + rr::paletteName(paletteModel) + ", " +
			host::filterName(filter) + " scaling";
	};
	window.setFilter(filter);
	showColours();

	Attract attract(rom, osFont, pal, seed);
	attract.setRiver(riverSeed, startBridge);
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
				case Key::L: filter = host::Filter((int(filter) + 1) % 3); window.setFilter(filter); showColours(); break;
				case Key::O: border = !border; showBorder(); break;
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
						// not generated yet: fly on, drawing only the last frame (and give up if the river
						// stops coming, so the window never stops answering)
						long stalled = 0;
						while (next < 0 && stalled < 4096) {
							const long before = attract.lines();
							for (int i = 0; i < 64; i++) attract.frame(true);
							stalled = attract.lines() == before ? stalled + 64 : 0;
							next = attract.bridgeAfter(shown);
						}
						if (next < 0) {
							attract.frame();
							back = 0;
							break;
						}
						while (attract.lines() - 1 < next + kMargin && stalled < 4096) {
							const long before = attract.lines();
							attract.frame(attract.lines() < next + kMargin);
							stalled = attract.lines() == before ? stalled + 1 : 0;
						}
						if (stalled >= 4096) attract.frame();   // draw the present
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
			for (int i = 0; i < 3; i++) {             // let a resized window settle
				for (host::Event e; window.poll(e);) {}
				window.present();
			}
			window.savePicture(shotPath);
		}
		if (shotPath) running = false;
		window.present();

		char buf[256];
		const long shown = attract.lines() - 1 - back;
		// the newest bridge at the top of the screen or below it: the one on screen, or the one
		// last passed (sections are much longer than the screen, so at most one is on it)
		std::snprintf(buf, sizeof buf, "River Raid - bridge %d (level %d) - line %ld - speed x%g%s%s - %s",
			attract.bridge(shown), attract.level(shown), shown, kSpeeds[speedIndex], back ? " - looking back" : "", paused ? " - paused" : "",
			colours.c_str());
		if (title != buf) {
			title = buf;
			window.setTitle(buf);
		}
	}

	return 0;
}
