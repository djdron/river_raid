// host.h with SDL3.
#include "host.h"

#include <SDL3/SDL.h>

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
	case SDLK_W: return Key::W;
	case SDLK_A: return Key::A;
	case SDLK_S: return Key::S;
	case SDLK_D: return Key::D;
	case SDLK_B: return Key::B;
	case SDLK_C: return Key::C;
	case SDLK_F: return Key::F;
	case SDLK_O: return Key::O;
	case SDLK_V: return Key::V;
	case SDLK_X: return Key::X;
	case SDLK_Z: return Key::Z;
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

constexpr float kPointerMoveDistance = 20.0f;   // pixels on the screen that show the hidden pointer again

}  // namespace

struct Host::Impl {
	SDL_Window *window = nullptr;
	SDL_Renderer *renderer = nullptr;
	SDL_Texture *texture = nullptr;
	SDL_AudioStream *audio = nullptr;
	int width = 0, height = 0, sampleRate = 0;
	int viewX = 0, viewY = 0, viewW = 0, viewH = 0;   // the part shown
	double pixelAspect = 1;
	bool vsync = false;
	std::vector<uint32_t> pixels;
	std::vector<SDL_Gamepad *> pads;
	float hiddenX = 0, hiddenY = 0;   // where the mouse pointer was hidden, on the screen
	std::string error;

	void hidePointer() {
		SDL_HideCursor();
		SDL_GetGlobalMouseState(&hiddenX, &hiddenY);
	}
	void draw() {
		SDL_UpdateTexture(texture, nullptr, pixels.data(), width * int(sizeof(uint32_t)));
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		int outW = 0, outH = 0;
		SDL_GetRenderOutputSize(renderer, &outW, &outH);
		const double imageW = viewW * pixelAspect;
		const double scale = std::min(outW / imageW, double(outH) / viewH);
		SDL_FRect dst;
		dst.w = float(imageW * scale);
		dst.h = float(viewH * scale);
		dst.x = (float(outW) - dst.w) / 2;
		dst.y = (float(outH) - dst.h) / 2;
		const SDL_FRect src{float(viewX), float(viewY), float(viewW), float(viewH)};
		SDL_RenderTexture(renderer, texture, &src, &dst);
	}
};

Host::Host() : impl_(std::make_unique<Impl>()) {}

Host::~Host() {
	Impl &h = *impl_;
	for (SDL_Gamepad *p : h.pads) SDL_CloseGamepad(p);
	if (h.audio) SDL_DestroyAudioStream(h.audio);
	if (h.texture) SDL_DestroyTexture(h.texture);
	if (h.renderer) SDL_DestroyRenderer(h.renderer);
	if (h.window) SDL_DestroyWindow(h.window);
	SDL_Quit();
}

bool Host::open(const char *title, int width, int height, double pixelAspect, int scale, int sampleRate) {
	Impl &h = *impl_;
	h.width = width;
	h.height = height;
	h.viewW = width;
	h.viewH = height;
	h.pixelAspect = pixelAspect;
	h.pixels.assign(size_t(width) * size_t(height), 0);
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD) && !SDL_Init(SDL_INIT_VIDEO)) {
		h.error = std::string("SDL_Init: ") + SDL_GetError();
		return false;
	}
	if (!SDL_CreateWindowAndRenderer(title, int(width * scale * pixelAspect), height * scale, SDL_WINDOW_RESIZABLE,
			&h.window, &h.renderer)) {
		h.error = std::string("SDL_CreateWindowAndRenderer: ") + SDL_GetError();
		return false;
	}
	h.vsync = SDL_SetRenderVSync(h.renderer, 1);
	h.texture = SDL_CreateTexture(h.renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
	SDL_SetTextureScaleMode(h.texture, SDL_SCALEMODE_NEAREST);
	if (sampleRate > 0) {
		const SDL_AudioSpec spec{SDL_AUDIO_S16, 1, sampleRate};
		h.audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
		if (h.audio) SDL_ResumeAudioStreamDevice(h.audio);
		h.sampleRate = sampleRate;
	}
	return true;
}

const std::string &Host::error() const { return impl_->error; }

std::string Host::info() const {
	const Impl &h = *impl_;
	int w = 0, hh = 0, pw = 0, ph = 0;
	SDL_GetWindowSize(h.window, &w, &hh);
	SDL_GetWindowSizeInPixels(h.window, &pw, &ph);
	char buf[160];
	std::snprintf(buf, sizeof buf, "SDL %d.%d.%d, renderer %s, vsync %s, window %dx%d (%dx%d pixels)",
		SDL_VERSIONNUM_MAJOR(SDL_GetVersion()), SDL_VERSIONNUM_MINOR(SDL_GetVersion()), SDL_VERSIONNUM_MICRO(SDL_GetVersion()),
		SDL_GetRendererName(h.renderer), h.vsync ? "on" : "off", w, hh, pw, ph);
	return buf;
}

double Host::refreshRate() const {
	const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(impl_->window));
	if (!mode) return 0;
	if (mode->refresh_rate_numerator > 0 && mode->refresh_rate_denominator > 0)
		return double(mode->refresh_rate_numerator) / mode->refresh_rate_denominator;
	return mode->refresh_rate;
}

bool Host::poll(Event &out) {
	Impl &h = *impl_;
	SDL_Event e;
	while (SDL_PollEvent(&e)) {
		out = Event();
		switch (e.type) {
		case SDL_EVENT_QUIT:
			out.type = Event::Type::Quit;
			return true;
		case SDL_EVENT_KEY_DOWN:
			out.type = Event::Type::KeyDown;
			out.key = keyFromEvent(e.key.key);
			out.repeat = e.key.repeat;
			out.alt = (e.key.mod & SDL_KMOD_ALT) != 0;
			out.shift = (e.key.mod & SDL_KMOD_SHIFT) != 0;
			return true;
		case SDL_EVENT_MOUSE_WHEEL:
			out.type = Event::Type::MouseWheel;
			out.wheel = e.wheel.y;
			return true;
		case SDL_EVENT_GAMEPAD_ADDED:
			if (SDL_Gamepad *pad = SDL_OpenGamepad(e.gdevice.which)) h.pads.push_back(pad);
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			for (size_t i = 0; i < h.pads.size(); i++)
				if (SDL_GetGamepadID(h.pads[i]) == e.gdevice.which) {
					SDL_CloseGamepad(h.pads[i]);
					h.pads.erase(h.pads.begin() + long(i));
					break;
				}
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
			h.hidePointer();
			break;
		case SDL_EVENT_MOUSE_MOTION:
			if (!SDL_CursorVisible()) {
				// When the window changes size or place (going full screen), the pointer stays
				// where it is on the screen, but its position in the window changes and SDL
				// reports that as motion. Only moving it on the screen, more than a shake of the
				// mouse, shows it again.
				float x, y;
				SDL_GetGlobalMouseState(&x, &y);
				const float dx = x - h.hiddenX, dy = y - h.hiddenY;
				if (dx * dx + dy * dy >= kPointerMoveDistance * kPointerMoveDistance) SDL_ShowCursor();
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
	SDL_Gamepad *p = impl_->pads[size_t(index)];
	constexpr int kDead = 12000;
	const int ax = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTX), ay = SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_LEFTY);
	if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_UP) || ay < -kDead) g.stick &= ~1;
	if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_DOWN) || ay > kDead) g.stick &= ~2;
	if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || ax < -kDead) g.stick &= ~4;
	if (SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || ax > kDead) g.stick &= ~8;
	g.fire = SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_SOUTH) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_EAST) ||
		SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_WEST) || SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_NORTH) ||
		SDL_GetGamepadAxis(p, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16000;
	g.start = SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_START);
	g.select = SDL_GetGamepadButton(p, SDL_GAMEPAD_BUTTON_BACK);
	return g;
}

void Host::setTitle(const std::string &title) { SDL_SetWindowTitle(impl_->window, title.c_str()); }

bool Host::fullscreen() const { return (SDL_GetWindowFlags(impl_->window) & SDL_WINDOW_FULLSCREEN) != 0; }

// Full screen on the desktop's resolution (SDL3 changes the video mode only when asked to).
// The pointer is hidden by the SDL_EVENT_WINDOW_ENTER_FULLSCREEN event.
void Host::setFullscreen(bool on) { SDL_SetWindowFullscreen(impl_->window, on); }

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
	SDL_SyncWindow(h.window);
}

uint32_t *Host::pixels() { return impl_->pixels.data(); }

void Host::present() {
	impl_->draw();
	SDL_RenderPresent(impl_->renderer);
}

bool Host::savePicture(const char *bmpPath) {
	impl_->draw();
	SDL_Surface *shot = SDL_RenderReadPixels(impl_->renderer, nullptr);
	if (!shot) return false;
	const bool ok = SDL_SaveBMP(shot, bmpPath);
	SDL_DestroySurface(shot);
	return ok;
}

void Host::playSound(const int16_t *samples, size_t count) {
	Impl &h = *impl_;
	if (!h.audio || count == 0) return;
	if (SDL_GetAudioStreamQueued(h.audio) > int(sizeof(int16_t)) * h.sampleRate / 5) SDL_ClearAudioStream(h.audio);
	SDL_PutAudioStreamData(h.audio, samples, int(count * sizeof(int16_t)));
}

size_t Host::queuedSound() const {
	return impl_->audio ? size_t(std::max(0, SDL_GetAudioStreamQueued(impl_->audio))) / sizeof(int16_t) : 0;
}

uint64_t Host::ticksNs() { return SDL_GetTicksNS(); }

void Host::delayNs(uint64_t ns) { SDL_DelayNS(ns); }

void Host::showError(const char *title, const std::string &message) {
	std::fprintf(stderr, "%s\n", message.c_str());
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title, message.c_str(), nullptr);
}

}  // namespace host
