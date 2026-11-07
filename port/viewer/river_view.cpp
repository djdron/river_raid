// River viewer: the river from the C++ generator, scrolling in a window (SDL3).
//
// usage: river_view [rom] [-game N] [-pal]
//   rom: path to the cartridge image; by default rom/River_Raid.rom is looked for in the
//   current folder and up to three folders above it.
//
// Keys:
//   Space          pause / continue
//   Up / Down      faster / slower (also backwards); the game itself scrolls 1 line per frame
//   mouse wheel    scroll by hand
//   PgUp / PgDn    one screen forward / back
//   B / Shift+B    next / previous bridge
//   Home           back to the start
//   1..8           start game 1..8 (games 3-8 start at bridges 5, 20 and 50)
//   F              full screen
//   Esc            quit
//
// Lines are generated as the view moves forward and kept, so going back is free. Each line is
// drawn as the game shows it when the line enters the screen (river_draw.h); objects stay where
// the generator placed them, since there is no game logic yet.
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "river_draw.h"
#include "river_gen.h"

namespace {

constexpr int kWidth = rr::kVisibleLast + 1 - rr::kVisibleFirst;   // 336 columns
constexpr int kHeight = 192;                                        // lines on screen
constexpr int kScale = 3;                                           // initial window scale
// Width of a column relative to the height of a line on an NTSC TV: a column is half a
// colour clock, which is a little narrower than a scanline is high.
constexpr double kPixelAspect = 0.86;

// All lines generated so far, drawn as Atari colour codes.
class River {
public:
	explicit River(const rr::Rom &rom) : rom_(rom), gen_(rom) {}

	void start(int game, bool pal) {
		game_ = game;
		gen_.s = rr::RiverState();
		gen_.startGame(game, pal);
		rows_.clear();
		level_.clear();
		bridges_.clear();
	}
	int game() const { return game_; }

	// Makes sure lines 0..n-1 exist.
	void ensure(long n) {
		uint8_t line[rr::kLineBytes];
		uint8_t row[rr::kRowWidth];
		while (long(level_.size()) < n) {
			const uint8_t before = gen_.s.level;
			gen_.nextLine(line);
			rr::drawScreenLine(rom_, gen_.s, line, row);
			rows_.insert(rows_.end(), row + rr::kVisibleFirst, row + rr::kVisibleFirst + kWidth);
			level_.push_back(gen_.s.level);
			if (gen_.s.level != before) bridges_.push_back(long(level_.size()) - 1);
		}
	}
	const uint8_t *row(long i) const { return &rows_[size_t(i) * kWidth]; }
	int level(long i) const { return level_[size_t(i)]; }
	// Line where the next bridge after line i is generated (generating ahead if needed).
	long nextBridge(long i) {
		for (;;) {
			auto it = std::upper_bound(bridges_.begin(), bridges_.end(), i);
			if (it != bridges_.end()) return *it;
			ensure(long(level_.size()) + 1024);
		}
	}
	long prevBridge(long i) const {
		auto it = std::lower_bound(bridges_.begin(), bridges_.end(), i);
		return it == bridges_.begin() ? 0 : *(it - 1);
	}

private:
	const rr::Rom &rom_;
	rr::RiverGen gen_;
	int game_ = 1;
	std::vector<uint8_t> rows_;
	std::vector<uint8_t> level_;
	std::vector<long> bridges_;
};

std::string findRom(int argc, char **argv) {
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			if (!std::strcmp(argv[i], "-game") || !std::strcmp(argv[i], "-shot")) i++;
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

}  // namespace

int main(int argc, char **argv) {
	int game = 1;
	bool pal = false;
	const char *shotPath = nullptr;   // testing: save the first frame as BMP and quit
	for (int i = 1; i < argc; i++) {
		if (!std::strcmp(argv[i], "-pal")) pal = true;
		else if (!std::strcmp(argv[i], "-game") && i + 1 < argc) game = std::clamp(std::atoi(argv[++i]), 1, 8);
		else if (!std::strcmp(argv[i], "-shot") && i + 1 < argc) shotPath = argv[++i];
	}

	const std::string romPath = findRom(argc, argv);
	rr::Rom rom;
	if (!rom.load(romPath)) {
		const std::string msg = "Cannot read the ROM: " + romPath +
		                        "\nPass the path to the River Raid cartridge image as the first argument.";
		std::fprintf(stderr, "%s\n", msg.c_str());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "River Raid viewer", msg.c_str(), nullptr);
		return 1;
	}

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	if (!SDL_CreateWindowAndRenderer("River Raid", int(kWidth * kScale * kPixelAspect), kHeight * kScale, SDL_WINDOW_RESIZABLE, &window,
	                                 &renderer)) {
		std::fprintf(stderr, "SDL_CreateWindowAndRenderer: %s\n", SDL_GetError());
		return 1;
	}
	SDL_SetRenderVSync(renderer, 1);
	SDL_Texture *texture =
	    SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, kWidth, kHeight);
	SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);

	uint32_t palette[256];
	for (int c = 0; c < 256; c++) {
		uint8_t rgb[3];
		rr::atariRgb(uint8_t(c), rgb);
		palette[c] = (uint32_t(rgb[0]) << 16) | (uint32_t(rgb[1]) << 8) | rgb[2];
	}

	River river(rom);
	river.start(game, pal);
	double pos = 0;        // line shown in the bottom screen row
	double speed = 1.0;    // lines per 1/60 s
	bool paused = false;
	uint64_t last = SDL_GetTicksNS();
	std::string title;

	for (bool running = true; running;) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) running = false;
			else if (e.type == SDL_EVENT_MOUSE_WHEEL) pos += e.wheel.y * 16;
			else if (e.type == SDL_EVENT_KEY_DOWN) {
				const bool shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
				switch (e.key.key) {
				case SDLK_ESCAPE: running = false; break;
				case SDLK_SPACE: paused = !paused; break;
				case SDLK_UP: speed = std::min(speed + 0.5, 16.0); break;
				case SDLK_DOWN: speed = std::max(speed - 0.5, -16.0); break;
				case SDLK_PAGEUP: pos += kHeight; break;
				case SDLK_PAGEDOWN: pos -= kHeight; break;
				case SDLK_HOME: pos = 0; break;
				case SDLK_B: pos = double(shift ? river.prevBridge(long(pos)) : river.nextBridge(long(pos))); break;
				case SDLK_F:
					SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN));
					break;
				default:
					if (e.key.key >= SDLK_1 && e.key.key <= SDLK_8) {
						river.start(int(e.key.key - SDLK_1) + 1, pal);
						pos = 0;
					}
				}
			}
		}

		const uint64_t now = SDL_GetTicksNS();
		const double dt = std::min(double(now - last) / 1e9, 0.1);
		last = now;
		if (!paused) pos += speed * dt * 60.0;
		pos = std::max(pos, 0.0);

		const long bottom = long(std::floor(pos));
		river.ensure(bottom + kHeight);

		void *pixels;
		int pitch;
		if (SDL_LockTexture(texture, nullptr, &pixels, &pitch)) {
			for (int y = 0; y < kHeight; y++) {
				const uint8_t *src = river.row(bottom + kHeight - 1 - y);   // newest line at the top
				uint32_t *dst = reinterpret_cast<uint32_t *>(static_cast<uint8_t *>(pixels) + y * pitch);
				for (int x = 0; x < kWidth; x++) dst[x] = palette[src[x]];
			}
			SDL_UnlockTexture(texture);
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		int outW = 0, outH = 0;
		SDL_GetRenderOutputSize(renderer, &outW, &outH);
		const double imageW = kWidth * kPixelAspect;   // keep the TV proportions, centred
		const double scale = std::min(outW / imageW, double(outH) / kHeight);
		SDL_FRect dst;
		dst.w = float(imageW * scale);
		dst.h = float(kHeight * scale);
		dst.x = (float(outW) - dst.w) / 2;
		dst.y = (float(outH) - dst.h) / 2;
		SDL_RenderTexture(renderer, texture, nullptr, &dst);

		if (shotPath) {
			SDL_Surface *shot = SDL_RenderReadPixels(renderer, nullptr);
			if (shot) {
				SDL_SaveBMP(shot, shotPath);
				SDL_DestroySurface(shot);
			}
			running = false;
		}
		SDL_RenderPresent(renderer);

		char buf[160];
		std::snprintf(buf, sizeof buf, "River Raid - game %d - bridge %d - line %ld - speed %+.1f%s", river.game(),
		              river.level(bottom), bottom, speed, paused ? " - paused" : "");
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
