// River Raid (Activision, 1982, Atari 8-bit), the game, C++ port with SDL3.
//
// usage: river_raid [rom] [-os file] [-pal] [-colours ntsc|pal] [-seed N]
//   rom: the cartridge image; by default rom/River_Raid.rom is looked for in the current
//   folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -seed N: start of POKEY's random counter (otherwise taken from the clock, as on the real
//   machine, where it depends on the moment of power-on).
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
//   Esc                       quit
// Gamepads: the first one is joystick 1 (and 2 while there is no second), the second one
// joystick 2. D-pad or left stick: joystick; A, B, X, Y or the right trigger: fire;
// Start: START; Back: SELECT.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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
#include "status_bar.h"

namespace {

// Part of the 384 x 240 screen buffer shown in the window: everything that is ever drawn.
constexpr int kViewLeft = 10, kViewWidth = 364;
constexpr int kViewHeight = rr::kScreenHeight;
constexpr int kScale = 3;               // initial window scale
constexpr double kPixelAspect = 0.86;   // width of a column / height of a line on an NTSC TV
constexpr double kFrameRate = 59.92;    // NTSC Atari
constexpr int kSampleRate = 48000;

// The Atari with the River Raid cartridge: the game, its random counter and sound chip, and
// the picture it produces.
class Atari {
public:
	Atari(const rr::Rom &rom, const uint8_t osFont[rr::kOsFontBytes], bool pal, uint32_t seed)
	    : rom_(rom), game_(rom), pal_(pal), random_(seed), pokey_(kSampleRate),
	      charsets_(rr::buildCharsets(rom, osFont)) {
		rr::initialPanel(rom, powerOnPanel_);
		std::memset(screen_, 0, sizeof screen_);
		game_.afterDisplay = [this] {
			std::memset(screen_, 0, size_t(rr::kRiverTop + rr::kRiverRows) * rr::kRowWidth);
			game_.drawRiver(screen_);
		};
	}

	// One frame (1/60 s) of the machine.
	void frame(const rr::Input &input) {
		frame_++;
		if (frame_ < kGameStartFrame) {
			// power-on: memory cleared, the attract mode about to start; only the scrolling
			// line moves (from frame 11)
			powerOnLogo_ = powerOnScroller_.logoX;
			if (frame_ >= 11) powerOnScroller_.step(true);
			rr::drawStatusBar(rom_, charsets_, powerOnPanel_, 0xFF, powerOnScroller_, powerOnLogo_, screen_);
		} else {
			if (frame_ == kGameStartFrame) {
				game_.startAttract(pal_);
			} else {
				// RANDOM is read in the main loop around scanline 192 (A205) and in the VBI (249)
				int reads = 0;
				game_.frame(input, [this, &reads] { return random_.read(reads++ ? 249 : 192, 13); });
			}
			const rr::GameState &g = game_.g;
			if (g.idle & 0x80) {
				std::memset(screen_, 0, sizeof screen_);   // screen off after a long time unused
			} else {
				rr::drawStatusBar(rom_, charsets_, g.panel, g.fuel, g.scroller, g.logoShown, screen_);
			}
		}
		random_.nextFrame();
		// sound: the registers the VBI set at the end of the previous frame play for one frame
		pokey_.run(rr::PokeyRandom::kCyclesPerLine * rr::PokeyRandom::kLinesPerFrame, audio_);
		for (int c = 0; c < 4; c++) {
			pokey_.write(2 * c, game_.g.pokey.audf[c]);
			pokey_.write(2 * c + 1, game_.g.pokey.audc[c]);
		}
	}

	const uint8_t (*screen() const)[rr::kScreenWidth] { return screen_; }
	std::vector<int16_t> &audio() { return audio_; }
	const rr::GameState &state() const { return game_.g; }

private:
	static constexpr long kGameStartFrame = 38;   // the attract mode's river starts
	const rr::Rom &rom_;
	rr::Game game_;
	bool pal_;
	rr::PokeyRandom random_;
	rr::PokeySound pokey_;
	rr::Charsets charsets_;
	uint8_t powerOnPanel_[rr::kPanelBytes];
	rr::Scroller powerOnScroller_;
	uint8_t powerOnLogo_ = 0;
	long frame_ = 0;
	uint8_t screen_[rr::kScreenHeight][rr::kScreenWidth];
	std::vector<int16_t> audio_;
};

// Joystick and console keys from the keyboard and up to two gamepads.
class Controls {
public:
	void addGamepad(SDL_JoystickID id) {
		if (SDL_Gamepad *pad = SDL_OpenGamepad(id)) pads_.push_back(pad);
	}
	void removeGamepad(SDL_JoystickID id) {
		for (size_t i = 0; i < pads_.size(); i++)
			if (SDL_GetGamepadID(pads_[i]) == id) {
				SDL_CloseGamepad(pads_[i]);
				pads_.erase(pads_.begin() + long(i));
				return;
			}
	}
	~Controls() {
		for (SDL_Gamepad *p : pads_) SDL_CloseGamepad(p);
	}

	rr::Input read() const {
		rr::Input in;
		const bool *k = SDL_GetKeyboardState(nullptr);
		uint8_t keys = 0x0F;   // PORTA bits: 0 up, 1 down, 2 left, 3 right; 0 = pressed
		if (k[SDL_SCANCODE_UP] || k[SDL_SCANCODE_W]) keys &= ~1;
		if (k[SDL_SCANCODE_DOWN] || k[SDL_SCANCODE_S]) keys &= ~2;
		if (k[SDL_SCANCODE_LEFT] || k[SDL_SCANCODE_A]) keys &= ~4;
		if (k[SDL_SCANCODE_RIGHT] || k[SDL_SCANCODE_D]) keys &= ~8;
		const bool keyFire = k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL] || k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_X];
		for (int j = 0; j < 2; j++) {
			in.stick[j] = keys;
			in.trigger[j] = keyFire;
		}
		in.start = k[SDL_SCANCODE_F4] || (k[SDL_SCANCODE_RETURN] && !(SDL_GetModState() & SDL_KMOD_ALT));
		in.select = k[SDL_SCANCODE_F3];
		in.option = k[SDL_SCANCODE_F2];
		in.key = k[SDL_SCANCODE_SPACE];
		for (size_t i = 0; i < pads_.size() && i < 2; i++) {
			uint8_t stick = 0x0F;
			bool fire = false;
			padState(pads_[i], stick, fire, in);
			const int first = int(i), last = pads_.size() == 1 ? 1 : int(i);   // one pad: both joysticks
			for (int j = first; j <= last; j++) {
				in.stick[j] &= stick;
				in.trigger[j] = in.trigger[j] || fire;
			}
		}
		return in;
	}

private:
	std::vector<SDL_Gamepad *> pads_;

	static void padState(SDL_Gamepad *p, uint8_t &stick, bool &fire, rr::Input &in) {
		constexpr int kDead = 12000;
		const int ax = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX), ay = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY);
		if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -kDead) stick &= ~1;
		if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > kDead) stick &= ~2;
		if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -kDead) stick &= ~4;
		if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > kDead) stick &= ~8;
		fire = SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_EAST) ||
		       SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_WEST) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_NORTH) ||
		       SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000;
		in.start = in.start || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_START);
		in.select = in.select || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_BACK);
	}
};

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-seed") || !std::strcmp(argv[i], "-shot") ||
				!std::strcmp(argv[i], "-colours"))
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

void fail(const std::string &msg) {
	std::fprintf(stderr, "%s\n", msg.c_str());
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "River Raid", msg.c_str(), nullptr);
}

}  // namespace

int main(int argc, char **argv) {
	bool pal = false;
	bool palColours = false;   // colours of a PAL machine (C switches)
	const char *osPath = nullptr;
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N[:script] saves frame N and quits
	long shotFrame = 0;
	std::string shotScript;
	uint32_t seed = uint32_t(uint64_t(std::time(nullptr)) ^ uint64_t(SDL_GetPerformanceCounter()));
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = palColours = true;
		else if (!std::strcmp(argv[i], "-colours") && i + 1 < argc) palColours = !std::strcmp(argv[++i], "pal");
		else if (!std::strcmp(argv[i], "-os") && i + 1 < argc) osPath = argv[++i];
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 0));
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

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) && !SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	if (!SDL_CreateWindowAndRenderer("River Raid", int(kViewWidth * kScale * kPixelAspect), kViewHeight * kScale,
	                                 SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		std::fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRenderVSync(renderer, 1);
	SDL_Texture *texture =
	    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kViewWidth, kViewHeight);
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
	SDL_AudioSpec spec{SDL_AUDIO_S16, 1, kSampleRate};
	SDL_AudioStream *audio =
	    shotPath ? nullptr : SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
	if (audio) SDL_ResumeAudioStreamDevice(audio);

	uint32_t palette[256];
	auto makePalette = [&]() {
		for (int c = 0; c < 256; c++) {
			uint8_t rgb[3];
			rr::atariRgb(uint8_t(c), rgb, palColours);
			palette[c] = (uint32_t(rgb[0]) << 16) | (uint32_t(rgb[1]) << 8) | rgb[2];
		}
	};
	makePalette();

	Atari atari(rom, osFont, pal, seed);
	Controls controls;
	double pending = 0;   // original frames still to run
	uint64_t last = SDL_GetTicksNS();

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

	for (bool running = true; running;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) running = false;
			else if (e.type == SDL_EVENT_GAMEPAD_ADDED) controls.addGamepad(e.gdevice.which);
			else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) controls.removeGamepad(e.gdevice.which);
			else if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
				const bool alt = (e.key.mod & SDL_KMOD_ALT) != 0;
				if (e.key.key == SDLK_ESCAPE) running = false;
				else if (e.key.key == SDLK_F || e.key.key == SDLK_F11 || (alt && e.key.key == SDLK_RETURN))
					SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
				else if (e.key.key == SDLK_C) {
					palColours = !palColours;
					makePalette();
				}
			}
		}

		const uint64_t now = SDL_GetTicksNS();
		const double dt = std::min(double(now - last) / 1e9, 0.1);
		last = now;
		if (!shotPath) {
			pending += dt * kFrameRate;
			const rr::Input input = controls.read();
			for (; pending >= 1; pending--) atari.frame(input);
		}
		{
			std::vector<int16_t> &samples = atari.audio();
			if (audio && !samples.empty()) {
				// keep the delay short: after a stall, drop what is queued
				if (SDL_GetAudioStreamQueued(audio) > int(sizeof(int16_t)) * kSampleRate / 5) SDL_ClearAudioStream(audio);
				SDL_PutAudioStreamData(audio, samples.data(), int(samples.size() * sizeof(int16_t)));
			}
			samples.clear();
		}

		void *pixels;
		int pitch;
		if (SDL_LockTexture(texture, nullptr, &pixels, &pitch)) {
			const auto *scr = atari.screen();
			for (int y = 0; y < kViewHeight; y++) {
				const uint8_t *src = scr[y] + kViewLeft;
				uint32_t *dst = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(pixels) + y * pitch);
				for (int x = 0; x < kViewWidth; x++) dst[x] = palette[src[x]];
			}
			SDL_UnlockTexture(texture);
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		int outW = 0, outH = 0;
		SDL_GetRenderOutputSize(renderer, &outW, &outH);
		const double imageW = kViewWidth * kPixelAspect;   // keep the TV proportions, centred
		const double scale = std::min(outW / imageW, double(outH) / kViewHeight);
		SDL_FRect dst;
		dst.w = float(imageW * scale);
		dst.h = float(kViewHeight * scale);
		dst.x = (float(outW) - dst.w) / 2;
		dst.y = (float(outH) - dst.h) / 2;
		SDL_RenderTexture(renderer, texture, nullptr, &dst);
		if (shotPath) {
			if (SDL_Surface *shot = SDL_RenderReadPixels(renderer, nullptr)) {
				SDL_SaveBMP(shot, shotPath);
				SDL_DestroySurface(shot);
			}
			running = false;
		}
		SDL_RenderPresent(renderer);
	}

	if (audio) SDL_DestroyAudioStream(audio);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
