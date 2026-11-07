// River viewer (SDL3): the game's attract mode, the flight over the river that the original
// shows when nobody presses START.
//
// usage: river_view [rom] [-os file] [-pal] [-seed N]
//   rom: path to the cartridge image; by default rom/River_Raid.rom is looked for in the
//   current folder and up to three folders above it.
//   -os file: an Atari OS ROM (10 KB or 16 KB) or its 1 KB font, for Atari's own letters;
//   without it the AltirraOS font is used (os_font.h).
//   -seed N: start of POKEY's random counter (otherwise taken from the clock, as on the real
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
//   F              full screen
//   Esc            quit
//
// Looking back shows the river from a map of the lines as they entered the screen, with the
// objects where they were placed; flying forward again replays it up to the present, where the
// game continues. The game, and the scrolling line, only run in the present.
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
#include "river_draw.h"
#include "river_gen.h"
#include "status_bar.h"

namespace {

// Part of the 384 x 240 screen buffer shown in the window: everything that is ever drawn.
constexpr int kViewLeft = 10, kViewWidth = 364;
constexpr int kViewTop = 0, kViewHeight = rr::kScreenHeight;
constexpr int kScale = 3;   // initial window scale
// Width of a column relative to the height of a line on an NTSC TV: a column is half a
// colour clock, which is a little narrower than a scanline is high.
constexpr double kPixelAspect = 0.86;
constexpr double kFrameRate = 59.92;   // NTSC Atari

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
			game_.logic([this] { return random_.read(192, 13); });   // A205, after scanline 160
			game_.vbi([this] { return random_.read(248, 30); });
		}
		random_.nextFrame();
		logoShown_ = scroller_.logoX;   // the display uses the position from before the step
		if (frame_ >= kFirstScrollerFrame) scroller_.step(true);
	}

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

private:
	const rr::Rom &rom_;
	rr::Game game_;
	bool pal_;
	uint32_t seed_;
	rr::PokeyRandom random_;
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
			if (!std::strcmp(argv[i], "-os") || !std::strcmp(argv[i], "-shot") || !std::strcmp(argv[i], "-seed")) i++;
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
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "River Raid viewer", msg.c_str(), nullptr);
}

}  // namespace

int main(int argc, char **argv) {
	bool pal = false;
	const char *osPath = nullptr;
	const char *shotPath = nullptr;   // testing: -shot file.bmp:N saves frame N as BMP (.raw: screen buffer) and quits
	long shotFrame = 0;
	uint32_t seed = uint32_t(SDL_GetTicks() ^ uint64_t(std::time(nullptr)));   // like POKEY at power-on
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-os") && i + 1 < argc) osPath = argv[++i];
		else if (!std::strcmp(argv[i], "-seed") && i + 1 < argc) seed = uint32_t(std::strtoul(argv[++i], nullptr, 0));
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

	if (!SDL_Init(SDL_INIT_VIDEO)) {
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

	uint32_t palette[256];
	for (int c = 0; c < 256; c++) {
		uint8_t rgb[3];
		rr::atariRgb(uint8_t(c), rgb);
		palette[c] = (uint32_t(rgb[0]) << 16) | (uint32_t(rgb[1]) << 8) | rgb[2];
	}

	Attract attract(rom, osFont, pal, seed);
	static uint8_t screen[rr::kScreenHeight][rr::kScreenWidth];
	// Speed in original frames per real frame; negative = backwards.
	static const double kSpeeds[] = {-16, -8, -4, -2, -1, -0.5, -0.25, 0, 0.25, 0.5, 1, 2, 4, 8, 16};
	const int kSpeedCount = int(sizeof kSpeeds / sizeof kSpeeds[0]);
	int speedIndex = 10;   // x1
	double pending = 0;    // original frames still to run
	bool paused = false;
	long back = 0;         // look-back in lines
	uint64_t last = SDL_GetTicksNS();
	std::string title;

	if (shotPath)
		for (long f = 0; f < shotFrame; f++) attract.frame();

	for (bool running = true; running;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) running = false;
			else if (e.type == SDL_EVENT_MOUSE_WHEEL) back += long(e.wheel.y * 16);
			else if (e.type == SDL_EVENT_KEY_DOWN) {
				switch (e.key.key) {
				case SDLK_ESCAPE: running = false; break;
				case SDLK_SPACE: paused = !paused; break;
				case SDLK_UP: speedIndex = std::min(speedIndex + 1, kSpeedCount - 1); break;
				case SDLK_DOWN: speedIndex = std::max(speedIndex - 1, 0); break;
				case SDLK_PAGEUP: back += rr::kRiverRows; break;
				case SDLK_PAGEDOWN: back -= rr::kRiverRows; break;
				case SDLK_BACKSPACE: back = 0; speedIndex = 10; break;
				case SDLK_HOME: attract.restart(); back = 0; break;
				case SDLK_B: {
					// the bridge line is shown a little above the bottom of the river area; Shift+B just
					// after a bridge goes to the one before it
					constexpr long kMargin = 100;
					const long shown = attract.lines() - 1 - back - kMargin;
					if (e.key.mod & SDL_KMOD_SHIFT) {
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
				case SDLK_F:
					SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
					break;
				default: break;
				}
			}
		}

		const uint64_t now = SDL_GetTicksNS();
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
		if (attract.lines() > 0 && back == attract.lines() - 1 && kSpeeds[speedIndex] < 0) pending = 0;

		attract.draw(back, screen);
		void *pixels;
		int pitch;
		if (SDL_LockTexture(texture, nullptr, &pixels, &pitch)) {
			for (int y = 0; y < kViewHeight; y++) {
				const uint8_t *src = screen[kViewTop + y] + kViewLeft;
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

		if (shotPath && std::strstr(shotPath, ".raw")) {   // screen buffer: 240 rows of 384 colour codes
			if (FILE *f = std::fopen(shotPath, "wb")) {
				std::fwrite(screen, 1, sizeof screen, f);
				std::fclose(f);
			}
		} else if (shotPath) {
			if (SDL_Surface *shot = SDL_RenderReadPixels(renderer, nullptr)) {
				SDL_SaveBMP(shot, shotPath);
				SDL_DestroySurface(shot);
			}
		}
		if (shotPath) running = false;
		SDL_RenderPresent(renderer);

		char buf[160];
		const long shown = attract.lines() - 1 - back;
		std::snprintf(buf, sizeof buf, "River Raid - bridge %d - line %ld - speed x%g%s%s", attract.level(shown), shown,
		              kSpeeds[speedIndex], back ? " - looking back" : "", paused ? " - paused" : "");
		if (title != buf) {
			title = buf;
			SDL_SetWindowTitle(window, buf);
		}
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
