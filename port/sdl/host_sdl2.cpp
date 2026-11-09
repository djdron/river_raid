// host.h with SDL2 (CMake option RR_SDL2).
#include "host.h"

#include <SDL.h>

#include <algorithm>
#include <cstdio>
#include <vector>

namespace host {
namespace {

SDL_Scancode scancode(Key key) {
	switch (key) {
	case Key::Up: return SDL_SCANCODE_UP;
	case Key::Down: return SDL_SCANCODE_DOWN;
	case Key::Left: return SDL_SCANCODE_LEFT;
	case Key::Right: return SDL_SCANCODE_RIGHT;
	case Key::W: return SDL_SCANCODE_W;
	case Key::A: return SDL_SCANCODE_A;
	case Key::S: return SDL_SCANCODE_S;
	case Key::D: return SDL_SCANCODE_D;
	case Key::B: return SDL_SCANCODE_B;
	case Key::C: return SDL_SCANCODE_C;
	case Key::F: return SDL_SCANCODE_F;
	case Key::L: return SDL_SCANCODE_L;
	case Key::O: return SDL_SCANCODE_O;
	case Key::V: return SDL_SCANCODE_V;
	case Key::X: return SDL_SCANCODE_X;
	case Key::Z: return SDL_SCANCODE_Z;
	case Key::LeftCtrl: return SDL_SCANCODE_LCTRL;
	case Key::RightCtrl: return SDL_SCANCODE_RCTRL;
	case Key::Return: return SDL_SCANCODE_RETURN;
	case Key::Space: return SDL_SCANCODE_SPACE;
	case Key::Backspace: return SDL_SCANCODE_BACKSPACE;
	case Key::Escape: return SDL_SCANCODE_ESCAPE;
	case Key::PageUp: return SDL_SCANCODE_PAGEUP;
	case Key::PageDown: return SDL_SCANCODE_PAGEDOWN;
	case Key::Home: return SDL_SCANCODE_HOME;
	case Key::F1: return SDL_SCANCODE_F1;
	case Key::F2: return SDL_SCANCODE_F2;
	case Key::F3: return SDL_SCANCODE_F3;
	case Key::F4: return SDL_SCANCODE_F4;
	case Key::F11: return SDL_SCANCODE_F11;
	case Key::Alt:
	case Key::None: break;
	}
	return SDL_SCANCODE_UNKNOWN;
}

// Keys in events by their meaning in the keyboard layout (letters), not their position.
Key keyFromEvent(SDL_Keycode code) {
	switch (code) {
	case SDLK_UP: return Key::Up;
	case SDLK_DOWN: return Key::Down;
	case SDLK_LEFT: return Key::Left;
	case SDLK_RIGHT: return Key::Right;
	case SDLK_w: return Key::W;
	case SDLK_a: return Key::A;
	case SDLK_s: return Key::S;
	case SDLK_d: return Key::D;
	case SDLK_b: return Key::B;
	case SDLK_c: return Key::C;
	case SDLK_f: return Key::F;
	case SDLK_l: return Key::L;
	case SDLK_o: return Key::O;
	case SDLK_v: return Key::V;
	case SDLK_x: return Key::X;
	case SDLK_z: return Key::Z;
	case SDLK_LCTRL: return Key::LeftCtrl;
	case SDLK_RCTRL: return Key::RightCtrl;
	case SDLK_RETURN: return Key::Return;
	case SDLK_SPACE: return Key::Space;
	case SDLK_BACKSPACE: return Key::Backspace;
	case SDLK_ESCAPE: return Key::Escape;
	case SDLK_PAGEUP: return Key::PageUp;
	case SDLK_PAGEDOWN: return Key::PageDown;
	case SDLK_HOME: return Key::Home;
	case SDLK_F1: return Key::F1;
	case SDLK_F2: return Key::F2;
	case SDLK_F3: return Key::F3;
	case SDLK_F4: return Key::F4;
	case SDLK_F11: return Key::F11;
	default: return Key::None;
	}
}

constexpr int kPointerMoveDistance = 20;   // pixels on the screen that show the hidden pointer again

}  // namespace

struct Host::Impl {
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Texture *texture = nullptr;
	SDL_Texture *sharp = nullptr;      // Filter::Sharp: the picture scaled by whole numbers
	int sharpW = 0, sharpH = 0;
	Filter filter = Filter::Sharp;
	SDL_AudioDeviceID audio = 0;
	int width = 0, height = 0, sampleRate = 0;
	int viewX = 0, viewY = 0, viewW = 0, viewH = 0;   // the part shown
	double pixelAspect = 1;
	std::vector<uint32_t> pixels;
	std::vector<SDL_GameController *> pads;
	int hiddenX = 0, hiddenY = 0;   // where the mouse pointer was hidden, on the screen
	bool initialised = false;
	std::string error;

	void hidePointer() {
		SDL_ShowCursor(SDL_DISABLE);
		SDL_GetGlobalMouseState(&hiddenX, &hiddenY);
	}
	void draw() {
		SDL_UpdateTexture(texture, nullptr, pixels.data(), width * int(sizeof(uint32_t)));
		int outW = 0, outH = 0;
		SDL_GetRendererOutputSize(renderer, &outW, &outH);
		const double imageW = viewW * pixelAspect;
		const double scale = std::min(outW / imageW, double(outH) / viewH);
		SDL_FRect dst;
		dst.w = float(imageW * scale);
		dst.h = float(viewH * scale);
		dst.x = (float(outW) - dst.w) / 2;
		dst.y = (float(outH) - dst.h) / 2;
		const SDL_Rect src{viewX, viewY, viewW, viewH};
		SDL_Texture *picture = texture;
		const SDL_Rect *part = &src;
		SDL_SetTextureScaleMode(texture, filter == Filter::Linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest);
		if (filter == Filter::Sharp) {
			// first scaled by whole numbers, as large as fits, into a texture of its own
			const int w = viewW * std::max(1, int(dst.w / float(viewW)));
			const int h = viewH * std::max(1, int(dst.h / float(viewH)));
			if (!sharp || sharpW != w || sharpH != h) {
				if (sharp) SDL_DestroyTexture(sharp);
				sharp = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_TARGET, w, h);
				if (sharp) SDL_SetTextureScaleMode(sharp, SDL_ScaleModeLinear);
				sharpW = w;
				sharpH = h;
			}
			if (sharp && SDL_SetRenderTarget(renderer, sharp) == 0) {
				SDL_RenderCopyF(renderer, texture, &src, nullptr);
				SDL_SetRenderTarget(renderer, nullptr);
				picture = sharp;
				part = nullptr;
			}
		}
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_RenderCopyF(renderer, picture, part, &dst);
	}
};

Host::Host() : impl_(std::make_unique<Impl>()) {}

Host::~Host() {
	Impl &h = *impl_;
	for (SDL_GameController *p : h.pads) SDL_GameControllerClose(p);
	if (h.audio) SDL_CloseAudioDevice(h.audio);
	if (h.sharp) SDL_DestroyTexture(h.sharp);
	if (h.texture) SDL_DestroyTexture(h.texture);
	if (h.renderer) SDL_DestroyRenderer(h.renderer);
	if (h.window) SDL_DestroyWindow(h.window);
	if (h.initialised) SDL_Quit();
}

bool Host::open(const char *title, int width, int height, double pixelAspect, int scale, int sampleRate) {
	Impl &h = *impl_;
	h.width = width;
	h.height = height;
	h.viewW = width;
	h.viewH = height;
	h.pixelAspect = pixelAspect;
	h.pixels.assign(size_t(width) * size_t(height), 0);
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0 && SDL_Init(SDL_INIT_VIDEO) != 0) {
		h.error = std::string("SDL_Init: ") + SDL_GetError();
		return false;
	}
	h.initialised = true;
	h.window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, int(width * scale * pixelAspect),
		height * scale, SDL_WINDOW_RESIZABLE);
	if (!h.window) {
		h.error = std::string("SDL_CreateWindow: ") + SDL_GetError();
		return false;
	}
	h.renderer = SDL_CreateRenderer(h.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!h.renderer) h.renderer = SDL_CreateRenderer(h.window, -1, 0);
	if (!h.renderer) {
		h.error = std::string("SDL_CreateRenderer: ") + SDL_GetError();
		return false;
	}
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");   // sharp pixels
	h.texture = SDL_CreateTexture(h.renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, width, height);
	if (sampleRate > 0) {
		SDL_AudioSpec want{}, have{};
		want.freq = sampleRate;
		want.format = AUDIO_S16SYS;
		want.channels = 1;
		want.samples = 1024;
		h.audio = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);   // SDL converts to the device's format
		if (h.audio) SDL_PauseAudioDevice(h.audio, 0);
		h.sampleRate = sampleRate;
	}
	return true;
}

const std::string &Host::error() const { return impl_->error; }

std::string Host::info() const {
	const Impl &h = *impl_;
	SDL_version v;
	SDL_GetVersion(&v);
	SDL_RendererInfo r{};
	SDL_GetRendererInfo(h.renderer, &r);
	int w = 0, hh = 0, pw = 0, ph = 0;
	SDL_GetWindowSize(h.window, &w, &hh);
	SDL_GetRendererOutputSize(h.renderer, &pw, &ph);
	char buf[160];
	std::snprintf(buf, sizeof buf, "SDL %d.%d.%d, renderer %s, vsync %s, window %dx%d (%dx%d pixels)", v.major, v.minor,
		v.patch, r.name ? r.name : "?", (r.flags & SDL_RENDERER_PRESENTVSYNC) ? "on" : "off", w, hh, pw, ph);
	return buf;
}

double Host::refreshRate() const {
	SDL_DisplayMode mode;
	if (SDL_GetCurrentDisplayMode(SDL_GetWindowDisplayIndex(impl_->window), &mode) != 0) return 0;
	return mode.refresh_rate;
}

bool Host::poll(Event &out) {
	Impl &h = *impl_;
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		out = Event();
		switch (e.type) {
		case SDL_QUIT:
			out.type = Event::Type::Quit;
			return true;
		case SDL_KEYDOWN:
			out.type = Event::Type::KeyDown;
			out.key = keyFromEvent(e.key.keysym.sym);
			out.repeat = e.key.repeat != 0;
			out.alt = (e.key.keysym.mod & KMOD_ALT) != 0;
			out.shift = (e.key.keysym.mod & KMOD_SHIFT) != 0;
			return true;
		case SDL_MOUSEWHEEL:
			out.type = Event::Type::MouseWheel;
			out.wheel = e.wheel.preciseY;
			return true;
		case SDL_CONTROLLERDEVICEADDED:
			if (SDL_GameController *pad = SDL_GameControllerOpen(e.cdevice.which)) h.pads.push_back(pad);
			break;
		case SDL_CONTROLLERDEVICEREMOVED:
			for (size_t i = 0; i < h.pads.size(); i++)
				if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(h.pads[i])) == e.cdevice.which) {
					SDL_GameControllerClose(h.pads[i]);
					h.pads.erase(h.pads.begin() + long(i));
					break;
				}
			break;
		case SDL_MOUSEBUTTONDOWN:
			h.hidePointer();
			break;
		case SDL_MOUSEMOTION:
			if (SDL_ShowCursor(SDL_QUERY) == SDL_DISABLE) {
				// Going full screen changes the pointer's position in the window, not on the
				// screen; only moving it on the screen, more than a shake, shows it again.
				int x, y;
				SDL_GetGlobalMouseState(&x, &y);
				const int dx = x - h.hiddenX, dy = y - h.hiddenY;
				if (dx * dx + dy * dy >= kPointerMoveDistance * kPointerMoveDistance) SDL_ShowCursor(SDL_ENABLE);
			}
			break;
		default:
			break;
		}
	}
	return false;
}

bool Host::keyHeld(Key key) const {
	const auto *state = SDL_GetKeyboardState(nullptr);
	if (key == Key::Alt) return state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT];
	const SDL_Scancode code = scancode(key);
	return code != SDL_SCANCODE_UNKNOWN && state[code];
}

int Host::gamepadCount() const { return int(impl_->pads.size()); }

Gamepad Host::gamepad(int index) const {
	Gamepad g;
	SDL_GameController *p = impl_->pads[size_t(index)];
	constexpr int kDead = 12000;
	const int ax = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTX);
	const int ay = SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_LEFTY);
	if (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_UP) || ay < -kDead) g.stick &= ~1;
	if (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ay > kDead) g.stick &= ~2;
	if (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_LEFT) || ax < -kDead) g.stick &= ~4;
	if (SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || ax > kDead) g.stick &= ~8;
	g.fire = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_A) || SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_B) ||
		SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_X) || SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_Y) ||
		SDL_GameControllerGetAxis(p, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 16000;
	g.start = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_START);
	g.select = SDL_GameControllerGetButton(p, SDL_CONTROLLER_BUTTON_BACK);
	return g;
}

void Host::setTitle(const std::string &title) { SDL_SetWindowTitle(impl_->window, title.c_str()); }

bool Host::fullscreen() const { return (SDL_GetWindowFlags(impl_->window) & SDL_WINDOW_FULLSCREEN) != 0; }

// Full screen on the desktop's resolution. SDL2 keeps the pointer at the same place in the
// window, which moves it on the screen (towards the top left corner); put it back.
void Host::setFullscreen(bool on) {
	Impl &h = *impl_;
	int x, y;
	SDL_GetGlobalMouseState(&x, &y);
	SDL_SetWindowFullscreen(h.window, on ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	SDL_WarpMouseGlobal(x, y);
	if (on) h.hidePointer();
}

void Host::setView(int x, int y, int width, int height) {
	Impl &h = *impl_;
	h.viewX = x;
	h.viewY = y;
	h.viewW = width;
	h.viewH = height;
	if (fullscreen()) return;
	int w = 0, ht = 0;
	SDL_GetWindowSize(h.window, &w, &ht);
	SDL_SetWindowSize(h.window, int(ht * width * h.pixelAspect / height + 0.5), ht);
	SDL_PumpEvents();   // the renderer learns the new size from the window's events
}

void Host::setFilter(Filter filter) { impl_->filter = filter; }

uint32_t *Host::pixels() { return impl_->pixels.data(); }

void Host::present() {
	impl_->draw();
	SDL_RenderPresent(impl_->renderer);
}

bool Host::savePicture(const char *bmpPath) {
	Impl &h = *impl_;
	h.draw();
	int w = 0, hh = 0;
	SDL_GetRendererOutputSize(h.renderer, &w, &hh);
	SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, w, hh, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!shot) return false;
	bool ok = SDL_RenderReadPixels(h.renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, shot->pixels, shot->pitch) == 0;
	ok = ok && SDL_SaveBMP(shot, bmpPath) == 0;
	SDL_FreeSurface(shot);
	return ok;
}

void Host::playSound(const int16_t *samples, size_t count) {
	Impl &h = *impl_;
	if (!h.audio || count == 0) return;
	if (SDL_GetQueuedAudioSize(h.audio) > sizeof(int16_t) * size_t(h.sampleRate) / 5) SDL_ClearQueuedAudio(h.audio);
	SDL_QueueAudio(h.audio, samples, Uint32(count * sizeof(int16_t)));
}

size_t Host::queuedSound() const {
	return impl_->audio ? SDL_GetQueuedAudioSize(impl_->audio) / sizeof(int16_t) : 0;
}

uint64_t Host::ticksNs() {
	const uint64_t count = SDL_GetPerformanceCounter(), freq = SDL_GetPerformanceFrequency();
	return count / freq * 1000000000ull + count % freq * 1000000000ull / freq;
}

void Host::delayNs(uint64_t ns) { SDL_Delay(Uint32(ns / 1000000)); }

void Host::showError(const char *title, const std::string &message) {
	std::fprintf(stderr, "%s\n", message.c_str());
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
}

}  // namespace host
