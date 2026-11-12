// The window, sound and input of the game and the viewer: a thin layer over SDL, so that the
// programs don't depend on its version. host_sdl3.cpp implements it with SDL3 (the default),
// host_sdl2.cpp with SDL2 (CMake option RR_SDL2, for platforms SDL3 has dropped, such as UWP
// on the Xbox).
//
// It also does the things both programs want from their window: the picture keeps its
// proportions, centred, scaled as chosen (setFilter); a mouse click or going full screen hides
// the mouse pointer until the mouse moves (more than a few pixels); gamepads are opened as
// they are connected.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace host {

enum class Key {
	None,
	Up, Down, Left, Right,
	W, A, S, D, B, C, F, L, M, O, V, X, Z,
	LeftCtrl, RightCtrl, Alt, Return, Space, Backspace, Escape,   // Alt: either Alt key (keyHeld only)
	PageUp, PageDown, Home,
	F1, F2, F3, F4, F7, F8, F11,
};

struct Event {
	enum class Type { None, Quit, KeyDown, MouseWheel };
	Type type = Type::None;
	Key key = Key::None;      // KeyDown
	bool repeat = false;      // KeyDown: held down, repeated
	bool alt = false, shift = false, ctrl = false;
	float wheel = 0;          // MouseWheel: steps, positive away from the user
};

// How the picture is scaled to the window.
enum class Filter {
	Nearest,   // sharp square pixels; at a scale that is not a whole number some columns and
	           // rows come out one screen pixel wider than others
	Linear,    // smooth, blurred
	Sharp,     // scaled by whole numbers with sharp pixels, then smoothly to the window: sharp,
	           // and all pixels the same size (only their edges are blended)
};
inline const char *filterName(Filter f) {
	return f == Filter::Nearest ? "nearest" : f == Filter::Linear ? "linear" : "sharp";
}

// A gamepad as an Atari joystick.
struct Gamepad {
	uint8_t stick = 0x0F;     // the D-pad: bit 0 up, 1 down, 2 left, 3 right; 0 = pressed
	float stickX = 0, stickY = 0;   // the left stick, -1..1 (y down)
	bool fire = false;        // A, B, Y or the right trigger
	bool x = false;           // X: the key that pauses
	bool start = false;       // Start
	bool select = false;      // Back
	bool leftShoulder = false, rightShoulder = false;   // LB, RB
};

class Host {
public:
	Host();
	~Host();
	Host(const Host &) = delete;
	Host &operator=(const Host &) = delete;

	// The window: a picture of width x height pixels, each pixelAspect as wide as it is high,
	// first at scale times its size, or smaller if that would not fit on the screen. sampleRate: mono 16-bit sound, 0 for none. false on
	// failure (see error()).
	bool open(const char *title, int width, int height, double pixelAspect, int scale, int sampleRate);
	const std::string &error() const;
	std::string info() const;             // renderer, vsync, window size
	double refreshRate() const;           // of the display the window is on, 0 if unknown

	bool poll(Event &e);                  // the next event, false when there are none
	bool keyHeld(Key key) const;
	int gamepadCount() const;
	Gamepad gamepad(int index) const;

	void setTitle(const std::string &title);
	// The part of the picture shown in the window (by default all of it). In a window (not
	// full screen) the window keeps its width and takes the height of the new proportions;
	// also on leaving full screen. Called before open(), it sets the window's first size.
	void setView(int x, int y, int width, int height);
	void setFilter(Filter filter);        // Filter::Sharp by default
	bool fullscreen() const;
	void setFullscreen(bool on);

	// The picture: width x height pixels, 0xRRGGBB, one row after another. Fill it, then
	// present() (which waits for the display's refresh).
	uint32_t *pixels();
	void present();
	bool savePicture(const char *bmpPath);   // the window's picture as it would be presented

	// Sound to play after what is queued; more than 1/5 s queued (after a stall) is dropped.
	void playSound(const int16_t *samples, size_t count);
	size_t queuedSound() const;           // samples not played yet

	static uint64_t ticksNs();
	static void delayNs(uint64_t ns);
	static void showError(const char *title, const std::string &message);
	// The folder of the program (on UWP: the app's installed package), ending in a separator;
	// empty if unknown.
	static std::string basePath();

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

}  // namespace host
