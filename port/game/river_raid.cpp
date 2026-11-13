// River Raid (Activision, 1982, Atari 8-bit), the game, C++ port (window, sound and input through
// SDL: sdl/host.h).
//
// usage: river_raid [rom] [-os file] [-pal] [-colours ntsc|pal] [-palette altirra|atari800] [-border] [-filter sharp|nearest|linear] [-stats] [-nosound] [-seed N] [-bridge N] [-random N]
//   rom: the cartridge image; by default rom/River_Raid.rom is looked for in the current
//   folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -pal: a PAL machine (Europe): 49.86 frames per second instead of 59.92 (the game about 17%
//   slower), its panel layout, sound clock and fuel depot odds, and PAL colours (src/tv.h)
//   -seed N: another river (and objects): game 1's river starts from generator state N
//            (1..16777215); games 3-8 start at its bridges 5, 20, 50; difficulty grows as usual
//   -bridge N: every game starts at bridge N (1..99999) of the river, at that bridge's level;
//            past 9999 the bridge counter shows its last 4 digits, as the original's goes round
//   -random N: start of POKEY's random counter (otherwise taken from the clock, as on the real
//   machine, where it depends on the moment of power-on).
//   -border: show the whole frame with its black border, not just the game's picture
//   -filter: how the picture is scaled to the window: sharp (default: whole-number scale, then
//   smoothly to fit), nearest (blocky, pixels of slightly uneven size) or linear (blurred)
//
// The whole game runs as the original does (src/game.cpp): after power-on the attract mode
// flies over the river; START begins a game, SELECT (or OPTION) chooses games 1-8 (even
// numbers are two-player games, the players taking turns; 3-4, 5-6, 7-8 start at bridges 5,
// 20 and 50); any key of the Atari keyboard pauses.
//
// Controls (keyboard drives both joysticks, so one person can also play a two-player game):
//   arrows or W A S D        joystick: left/right steer, up faster, down slower
//   Left/Right Ctrl, Z, X     fire
//   F4 or Enter               START
//   F3                        SELECT
//   F2                        OPTION
//   Space                     a key of the Atari keyboard: pause
//   F, F11 or Alt+Enter       full screen
//   C / V                     NTSC / PAL colours; Altirra / atari800 colour model
//   O                         the game's picture only / the whole frame with its border
//   L                         scaling: sharp / nearest / linear
//   F7 / F8                   save the game's state / go back to it (in memory, until quitting)
//   Esc                       leave full screen
// A mouse click or going full screen hides the mouse pointer until the mouse moves. Closing the window (or Alt+F4,
// Cmd+Q) quits.
// Gamepads: the first one is joystick 1 (and 2 while there is no second), the second one
// joystick 2. D-pad or left stick: joystick; A, B, Y or the right trigger: fire; X: the key
// that pauses; Start: START; Back: SELECT; RB / LB: save the state / go back to it (as F7 / F8);
// LT: the next scaling (as L).
// The left stick goes by its angle (see AnalogStick).
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

#include "game.h"
#include "os_font.h"
#include "pokey_random.h"
#include "pokey_sound.h"
#include "status_bar.h"

#include "host.h"
#include "host_main.h"
#ifdef RR_UWP
#include "display_mode.h"
#endif

namespace {

// Part of the 384 x 240 screen buffer shown in the window: everything that is ever drawn.
constexpr int kViewLeft = 10, kViewWidth = 364;
constexpr int kViewTop = 0, kViewHeight = rr::kScreenHeight;
constexpr int kScale = 6;               // initial window scale (less if the screen is smaller)

// Frames of the game per second, and how many display refreshes show one frame. On a display
// whose refresh rate is a multiple of the Atari's (60, 120 Hz... for NTSC's 59.92; 50, 100 Hz...
// for PAL's 49.86) the game runs at exactly that rate divided by the multiple, so that every
// frame stays on screen the same number of refreshes (at 59.92 against 60 Hz, one frame in 750
// would be shown twice); otherwise at the Atari's own rate (frameRate).
struct Pace {
	double rate = 0;
	int refreshesPerFrame = 0;   // 0: not locked to the display
};
Pace paceFor(double refreshRate, double frameRate) {
	Pace p;
	p.rate = frameRate;
	if (refreshRate <= 0) return p;
	const int n = int(refreshRate / frameRate + 0.5);
	if (n >= 1 && std::abs(refreshRate / n - frameRate) < frameRate * 0.01) {
		p.rate = refreshRate / n;
		p.refreshesPerFrame = n;
	}
	return p;
}
constexpr int kSampleRate = 48000;

// The Atari with the River Raid cartridge: the game, its random counter and sound chip, and
// the picture it produces.
class Atari {
public:
	Atari(const rr::Rom &rom, const uint8_t osFont[rr::kOsFontBytes], bool pal, uint32_t seed)
		: rom_(rom), game_(rom), pal_(pal), tv_(rr::tvSystem(pal)), random_(seed, tv_), pokey_(kSampleRate, tv_.clock),
		charsets_(rr::buildCharsets(rom, osFont)) {
		game_.keepLineStates = true;   // for small saved states
		rr::initialPanel(rom, powerOnPanel_);
		std::memset(screen_, 0, sizeof screen_);
		game_.afterDisplay = [this] {
			std::memset(screen_, 0, size_t(rr::kRiverTop + rr::kRiverRows) * rr::kRowWidth);
			game_.drawRiver(screen_);
		};
	}

	// One frame (1/60 s, 1/50 s on PAL) of the machine.
	void frame(const rr::Input &input) {
		frame_++;
		const long gameStart = rr::Game::attractStartFrame(pal_);
		if (frame_ < gameStart) {
			// power-on: memory cleared, the attract mode about to start; only the scrolling
			// line moves (from frame 11, 9 on PAL)
			powerOnLogo_ = powerOnScroller_.logoX;
			if (frame_ >= rr::Game::scrollerStartFrame(pal_)) powerOnScroller_.step(true);
			rr::drawStatusBar(rom_, charsets_, powerOnPanel_, 0xFF, powerOnScroller_, powerOnLogo_, screen_, pal_);
		} else {
			if (frame_ == gameStart) {
				game_.startAttract(pal_);
			} else {
				// RANDOM is read in the main loop around scanline 192 (A205; 191, earlier in the
				// line, on PAL) and in the VBI (249)
				int reads = 0;
				game_.frame(input, [this, &reads] {
					return reads++ ? random_.read(249, 13) : pal_ ? random_.read(191, 24) : random_.read(192, 13);
				});
			}
			const rr::GameState &g = game_.g;
			if (g.idleTime & 0x80) {
				std::memset(screen_, 0, sizeof screen_);   // screen off after a long time unused
			} else {
				rr::drawStatusBar(rom_, charsets_, g.panel, g.fuel, g.scroller, g.logoShown, screen_, pal_);
			}
		}
		random_.nextFrame();
		// sound: the registers the VBI set at the end of the previous frame play for one frame
		pokey_.run(tv_.cyclesPerFrame(), audio_);
		for (int c = 0; c < 4; c++) {
			pokey_.write(2 * c, game_.g.pokey.audf[c]);
			pokey_.write(2 * c + 1, game_.g.pokey.audc[c]);
		}
	}

	const uint8_t (*screen() const)[rr::kScreenWidth] { return screen_; }
	std::vector<int16_t> &audio() { return audio_; }
	void setSoundRate(double rate) { pokey_.setSampleRate(rate); }
	const rr::GameState &state() const { return game_.g; }

	void setRiver(uint32_t seed, int bridge) { game_.gen.setRiver(seed, bridge); }

	// Saved state (F7 / F8): the whole machine, in memory: the game's own (Game::save, only what
	// it cannot make again), POKEY's random counter and sound, the power-on screen's scrolling
	// line. The picture is drawn again by the next frame.
	void saveState() { saved_.emplace(*this); }
	size_t savedBytes() const { return saved_ ? sizeof(Saved) - sizeof(std::vector<uint8_t>) + saved_->game.size() : 0; }
	bool loadState() {
		if (!saved_) return false;
		game_.load(saved_->game);
		random_ = saved_->random;
		pokey_ = saved_->pokey;
		powerOnScroller_ = saved_->powerOnScroller;
		powerOnLogo_ = saved_->powerOnLogo;
		frame_ = saved_->frame;
		return true;
	}

private:
	const rr::Rom &rom_;
	rr::Game game_;
	bool pal_;
	rr::TvSystem tv_;
	rr::PokeyRandom random_;
	rr::PokeySound pokey_;
	rr::Charsets charsets_;
	uint8_t powerOnPanel_[rr::kPanelBytes];
	rr::Scroller powerOnScroller_;
	uint8_t powerOnLogo_ = 0;
	long frame_ = 0;
	uint8_t screen_[rr::kScreenHeight][rr::kScreenWidth];
	std::vector<int16_t> audio_;
	struct Saved {
		std::vector<uint8_t> game;
		rr::PokeyRandom random;
		rr::PokeySound pokey;
		rr::Scroller powerOnScroller;
		uint8_t powerOnLogo;
		long frame;
		explicit Saved(const Atari &a)
			: game(a.game_.save()), random(a.random_), pokey(a.pokey_), powerOnScroller(a.powerOnScroller_),
			  powerOnLogo(a.powerOnLogo_), frame(a.frame_) {}
	};
	std::optional<Saved> saved_;
};

// A gamepad's left stick as the Atari's digital joystick. The direction comes from the stick's
// angle: up, down, left and right get 60 degrees each, the diagonals 30, so pulling back to slow
// down with the stick a little to one side stays straight back. Up and down count once the stick
// is out past kOn, left and right (which turn the plane at once) only further out, past kOnSide;
// each until the stick is back inside kOff / kOffSide.
class AnalogStick {
public:
	static constexpr float kOn = 0.30f, kOff = 0.22f, kOnSide = 0.55f, kOffSide = 0.45f;
	// The joystick (PORTA bits, 0 = pressed) for the stick at x, y (-1..1, y down).
	uint8_t read(float x, float y) {
		const float r = std::sqrt(x * x + y * y);
		uint8_t bits = 0x0F;
		if (r >= ((last_ & 0x03) != 0x03 ? kOff : kOn) || r >= ((last_ & 0x0C) != 0x0C ? kOffSide : kOnSide)) {
			// the angle from the right, counter-clockwise (y points down), 0..360
			float a = std::atan2(-y, x) * 57.29578f;
			if (a < 0) a += 360;
			// 8 sectors: right at 0, up-right at 45 ... a cardinal is +-30, a diagonal +-15
			static const uint8_t kDirs[8] = {0x07, 0x06, 0x0E, 0x0A, 0x0B, 0x09, 0x0D, 0x05};
			for (int k = 0; k < 8; k++) {
				float d = std::fabs(a - k * 45.0f);
				if (d > 180) d = 360 - d;
				if (d <= ((k & 1) ? 15.0f : 30.0f)) { bits = kDirs[k]; break; }
			}
			// each part only past its own threshold
			if (r < ((last_ & 0x03) != 0x03 ? kOff : kOn)) bits |= 0x03;
			if (r < ((last_ & 0x0C) != 0x0C ? kOffSide : kOnSide)) bits |= 0x0C;
		}
		last_ = bits;
		return bits;
	}

private:
	uint8_t last_ = 0x0F;   // the last result: which parts are on, for the hysteresis
};

// Joystick and console keys from the keyboard and up to two gamepads.
rr::Input readControls(const host::Host &h, AnalogStick sticks[2]) {
	using host::Key;
	rr::Input in;
	uint8_t keys = 0x0F;   // PORTA bits: 0 up, 1 down, 2 left, 3 right; 0 = pressed
	if (h.keyHeld(Key::Up) || h.keyHeld(Key::W)) keys &= ~1;
	if (h.keyHeld(Key::Down) || h.keyHeld(Key::S)) keys &= ~2;
	if (h.keyHeld(Key::Left) || h.keyHeld(Key::A)) keys &= ~4;
	if (h.keyHeld(Key::Right) || h.keyHeld(Key::D)) keys &= ~8;
	const bool keyFire = h.keyHeld(Key::LeftCtrl) || h.keyHeld(Key::RightCtrl) || h.keyHeld(Key::Z) || h.keyHeld(Key::X);
	for (int j = 0; j < 2; j++) {
		in.stick[j] = keys;
		in.trigger[j] = keyFire;
	}
	in.start = h.keyHeld(Key::F4) || (h.keyHeld(Key::Return) && !h.keyHeld(Key::Alt));   // (Alt+Enter: full screen)
	in.select = h.keyHeld(Key::F3);
	in.option = h.keyHeld(Key::F2);
	in.key = h.keyHeld(Key::Space);
	const int pads = std::min(h.gamepadCount(), 2);
	for (int i = 0; i < pads; i++) {
		const host::Gamepad pad = h.gamepad(i);
		const int last = pads == 1 ? 1 : i;   // one pad: both joysticks
		const uint8_t stick = pad.stick & sticks[i].read(pad.stickX, pad.stickY);
		for (int j = i; j <= last; j++) {
			in.stick[j] &= stick;
			in.trigger[j] = in.trigger[j] || pad.fire;
		}
		in.key = in.key || pad.x;   // X: the key that pauses (the Xbox has no keyboard)
		in.start = in.start || pad.start;
		in.select = in.select || pad.select;
	}
	return in;
}

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-seed") || !std::strcmp(argv[i], "-bridge") ||
				!std::strcmp(argv[i], "-random") ||
				!std::strcmp(argv[i], "-shot") ||
				!std::strcmp(argv[i], "-colours") || !std::strcmp(argv[i], "-palette") ||
				!std::strcmp(argv[i], "-filter"))
				i++;
			continue;
		}
		return argv[i];
	}
	// next to the program (on UWP: in the app's package), then the current folder and up to 3
	// above it
	std::vector<std::string> places = {host::Host::basePath()};
	std::string up;
	for (int d = 0; d < 4; d++, up += "../") places.push_back(up);
	for (const std::string &place : places) {
		const std::string p = place + "rom/River_Raid.rom";
		if (FILE *f = std::fopen(p.c_str(), "rb")) { std::fclose(f); return p; }
	}
	return "rom/River_Raid.rom";
}

void fail(const std::string &msg) { host::Host::showError("River Raid", msg); }

}  // namespace

int main(int argc, char **argv) {
	bool pal = false;
	host::Filter filter = host::Filter::Sharp;   // -filter (L switches)
	bool border = false;       // -border: the whole frame with its black border (O switches)
	bool palColours = true;    // colours of a PAL machine (C switches to NTSC)
	rr::PaletteModel paletteModel = rr::PaletteModel::Altirra;   // V switches
	const char *osPath = nullptr;
	bool showStats = false;            // -stats: renderer, frame rate and timings once a second
	bool sound = true;                 // -nosound: no sound output (to measure its cost)
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N[:script] saves frame N and quits
	long shotFrame = 0;
	std::string shotScript;
	uint32_t seed = uint32_t(uint64_t(std::time(nullptr)) ^ host::Host::ticksNs());
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
		else if (!std::strcmp(argv[i], "-stats")) showStats = true;
		else if (!std::strcmp(argv[i], "-nosound")) sound = false;
		else if (!std::strcmp(argv[i], "-shot") && i + 1 < argc) {
			static std::string path;
			path = argv[++i];
			const size_t c1 = path.find(':');
			if (c1 != std::string::npos) {
				const size_t c2 = path.find(':', c1 + 1);
				shotFrame = std::atol(path.c_str() + c1 + 1);
				if (c2 != std::string::npos) shotScript = path.substr(c2 + 1);
				path.resize(c1);
			}
			shotPath = path.c_str();
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

	rr::TvSystem tv = rr::tvSystem(pal);
	host::Host window;
	// the game's picture only, or the whole frame
	auto showBorder = [&]() {
		if (border) window.setView(0, 0, kViewWidth, kViewHeight);
		else window.setView(rr::kPictureLeft - kViewLeft, rr::kPictureTop - kViewTop, rr::kPictureWidth, rr::pictureHeight(pal));
	};
	showBorder();   // before open(): the window is made in the size of the part shown
	if (!window.open("River Raid", kViewWidth, kViewHeight, tv.pixelAspect(), kScale, shotPath || !sound ? 0 : kSampleRate)) {
		fail(window.error());
		return 1;
	}
	bool startLt = false;   // LT held at the start (UWP: PAL)
#ifdef RR_UWP
	// The Xbox has no command line: LT held while the game starts is -pal, with the TV switched
	// to 50 Hz if the console allows it (Settings - TV & display options - Video modes - Allow
	// 50 Hz). The gamepads show up a moment after the start: wait for one (up to 1.5 s), then
	// let its state come in.
	{
		const uint64_t start = host::Host::ticksNs();
		uint64_t seen = 0;
		for (;;) {
			host::Event e;
			while (window.poll(e)) {}
			const uint64_t now = host::Host::ticksNs();
			if (window.gamepadCount() > 0 && !seen) seen = now;
			for (int i = 0; i < window.gamepadCount(); i++) startLt = startLt || window.gamepad(i).leftTrigger;
			if (startLt || (seen && now - seen > 200000000) || now - start > 1500000000) break;
			host::Host::delayNs(10000000);
		}
	}
	if (startLt && !pal) {
		pal = true;
		tv = rr::kPal;
		window.setPixelAspect(tv.pixelAspect());
		showBorder();
		uwp::requestRefreshRate(50);
	}
#endif
	// the display's refresh rate (on the Xbox the TV's mode, which SDL does not know; asked
	// for twice a second, as it changes a moment after requestRefreshRate())
#ifdef RR_UWP
	double tvRate = 0;
	uint64_t tvRateAt = 0;
#endif
	auto displayRate = [&]() {
#ifdef RR_UWP
		const uint64_t now = host::Host::ticksNs();
		if (!tvRateAt || now - tvRateAt > 500000000) {
			tvRate = uwp::displayRefreshRate();
			tvRateAt = now;
		}
		if (tvRate > 0) return tvRate;
#endif
		return window.refreshRate();
	};
	if (showStats) {
		const Pace pace = paceFor(displayRate(), tv.frameRate());
		std::fprintf(stderr, "%s, display %.2f Hz, game %.3f frames/s%s\n", window.info().c_str(), displayRate(),
			pace.rate, pace.refreshesPerFrame ? " (locked to the display)" : "");
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
	std::string stateNote;   // shown in the title after F7 / F8
	auto showColours = [&]() {
		const std::string title = std::string("River Raid - ") + (palColours ? "PAL" : "NTSC") + " colours, " +
			rr::paletteName(paletteModel) + ", " + host::filterName(filter) + " scaling" + stateNote;
		window.setTitle(title);
	};
	window.setFilter(filter);
	showColours();

	Atari atari(rom, osFont, pal, seed);
	atari.setRiver(riverSeed, startBridge);
	double pending = 0;   // original frames still to run
	bool presentWaited = false;   // the last present() waited for the display's refresh
	struct Stats {
		uint64_t since = 0, emulation = 0, present = 0;
		long frames = 0, pictures = 0;
	} stats;
	stats.since = host::Host::ticksNs();
	uint64_t last = stats.since;

	if (shotPath) {
		// testing: run N frames with a fixed input ("start@F" presses START at frame F,
		// "fire" holds fire and up from then on), then save the picture
		long startAt = -1;
		if (shotScript.rfind("start@", 0) == 0) startAt = std::atol(shotScript.c_str() + 6);
		for (long f = 1; f <= shotFrame; f++) {
			rr::Input in;
			if (f >= startAt && f < startAt + 10) in.start = true;
			if (startAt >= 0 && f > startAt + 200) { in.stick[0] = 0x0E; in.trigger[0] = (f / 8) & 1; }
			atari.frame(in);
		}
		atari.audio().clear();
	}

	auto saveState = [&]() {
		atari.saveState();
		stateNote = " - state saved";
		showColours();
	};
	auto nextFilter = [&]() {
		filter = host::Filter((int(filter) + 1) % 3);
		window.setFilter(filter);
		showColours();
	};
	auto loadState = [&]() {
		stateNote = atari.loadState() ? " - state loaded" : " - no state saved (F7)";
		showColours();
	};
	bool padHeld[3] = {false, false, startLt};   // RB, LB, LT on any gamepad, last pass
	AnalogStick sticks[2];       // the gamepads' left sticks

	for (bool running = true; running;) {
		{
			// gamepad: RB saves the state, LB goes back to it, LT the next scaling (on pressing)
			bool held[3] = {};
			for (int i = 0; i < window.gamepadCount(); i++) {
				const host::Gamepad pad = window.gamepad(i);
				held[0] = held[0] || pad.rightShoulder;
				held[1] = held[1] || pad.leftShoulder;
				held[2] = held[2] || pad.leftTrigger;
			}
			if (held[0] && !padHeld[0]) saveState();
			if (held[1] && !padHeld[1]) loadState();
			if (held[2] && !padHeld[2]) nextFilter();
			for (int i = 0; i < 3; i++) padHeld[i] = held[i];
		}
		host::Event e;
		while (window.poll(e)) {
			using host::Key;
			if (e.type == host::Event::Type::Quit) running = false;
			if (e.type != host::Event::Type::KeyDown || e.repeat) continue;
			if (e.key == Key::F || e.key == Key::F11 || (e.alt && e.key == Key::Return)) {
				window.setFullscreen(!window.fullscreen());
			} else if (e.key == Key::Escape && window.fullscreen()) {
				window.setFullscreen(false);
			} else if (e.key == Key::C) {
				palColours = !palColours;
				makePalette();
				showColours();
			} else if (e.key == Key::L) {
				nextFilter();
			} else if (e.key == Key::O) {
				border = !border;
				showBorder();
			} else if (e.key == Key::F7) {
				saveState();
			} else if (e.key == Key::F8) {
				loadState();
			} else if (e.key == Key::V) {
				paletteModel = paletteModel == rr::PaletteModel::Altirra ? rr::PaletteModel::Atari800 : rr::PaletteModel::Altirra;
				makePalette();
				showColours();
			}
		}

		const uint64_t now = host::Host::ticksNs();
		const double dt = std::min(double(now - last) / 1e9, 0.1);
		last = now;
		if (!shotPath) {
			const Pace pace = paceFor(displayRate(), tv.frameRate());
			pending += dt * pace.rate;
			const bool vsyncPaced = pace.refreshesPerFrame && presentWaited;
			if (vsyncPaced) {
				// The last picture waited for the display's refresh (full screen; in a window the
				// system's compositor usually takes it at once): the refreshes pace the loop, one
				// pass each. Round small timing jitter to whole refreshes. (Without the wait the
				// sleep below paces the loop, and rounding would lose its oversleep.)
				const double step = 1.0 / pace.refreshesPerFrame;
				const double steps = std::round(pending / step);
				if (std::abs(pending - steps * step) < step / 4) pending = steps * step;
			}
			if (pending < 1 && !vsyncPaced) {
				// No new frame yet (a display faster than 60 Hz, or no vsync): wait for it instead
				// of drawing the same picture again.
				host::Host::delayNs(uint64_t((1 - pending) / pace.rate * 1e9));
				continue;
			}
			if (pending >= 1) {
				const uint64_t t0 = host::Host::ticksNs();
				const rr::Input input = readControls(window, sticks);
				for (; pending >= 1; pending--) {
					atari.frame(input);
					stats.frames++;
				}
				stats.emulation += host::Host::ticksNs() - t0;
			}
		}
		std::vector<int16_t> &samples = atari.audio();
		window.playSound(samples.data(), samples.size());
		if (!shotPath && sound) {
			// The sound for the next frames: as many samples per second as the output plays.
			// When the game runs at the display's rate rather than the Atari's, each frame's
			// sound is resampled to its real length (the pitch follows the speed, 0.13% at
			// 60 Hz); and the queue is kept near 50 ms, so the sound card's clock drifting
			// against the display's neither starves nor fills it (at most 0.5% either way).
			const double target = kSampleRate * 0.05;
			const double queued = double(window.queuedSound());
			const double correction = 1 + 0.005 * std::clamp((target - queued) / target, -1.0, 1.0);
			atari.setSoundRate(kSampleRate * tv.frameRate() / paceFor(displayRate(), tv.frameRate()).rate * correction);
		}
		samples.clear();

		uint32_t *pixels = window.pixels();
		const auto *scr = atari.screen();
		for (int y = 0; y < kViewHeight; y++)
			for (int x = 0; x < kViewWidth; x++) pixels[y * kViewWidth + x] = palette[scr[y][kViewLeft + x]];
		if (shotPath) {
			for (int i = 0; i < 3; i++) {             // let a resized window settle
				for (host::Event e; window.poll(e);) {}
				window.present();
			}
			window.savePicture(shotPath);
			running = false;
		}
		const uint64_t t1 = host::Host::ticksNs();
		window.present();
		const uint64_t presentNs = host::Host::ticksNs() - t1;
		const double refresh = displayRate();
		presentWaited = refresh > 0 && double(presentNs) > 0.25e9 / refresh;
		stats.present += presentNs;
		stats.pictures++;
		if (showStats && now - stats.since >= 1000000000) {
			const double sec = double(now - stats.since) / 1e9;
			std::fprintf(stderr, "%.1f frames/s, %.1f pictures/s; emulation %.2f ms, present (incl. vsync wait) %.2f ms per picture; sound queued %.0f ms\n",
				stats.frames / sec, stats.pictures / sec, stats.emulation / 1e6 / std::max(1L, stats.pictures),
				stats.present / 1e6 / std::max(1L, stats.pictures), 1000.0 * double(window.queuedSound()) / kSampleRate);
			stats = Stats();
			stats.since = now;
		}
	}

#ifdef RR_UWP
	uwp::restoreDisplayMode();
#endif
	return 0;
}
