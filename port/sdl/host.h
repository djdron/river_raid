// The window, sound and input of the game and the viewer: a thin layer over SDL, so that the
// programs don't depend on its version. host_sdl3.cpp implements it with SDL3 (the default),
// host_sdl2.cpp with SDL2 (CMake option RR_SDL2, for platforms SDL3 has dropped, such as UWP
// on the Xbox).
//
// It also does the things both programs want from their window: the picture keeps its
// proportions, centred, scaled with sharp pixels; a mouse click or going full screen hides
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
	W, A, S, D, B, C, F, V, X, Z,
	LeftCtrl, RightCtrl, Alt, Return, Space, Backspace, Escape,   // Alt: either Alt key (keyHeld only)
	PageUp, PageDown, Home,
	F1, F2, F3, F4, F11,
};

struct Event {
	enum class Type { None, Quit, KeyDown, MouseWheel };
	Type type = Type::None;
	Key key = Key::None;      // KeyDown
	bool repeat = false;      // KeyDown: held down, repeated
	bool alt = false, shift = false;
	float wheel = 0;          // MouseWheel: steps, positive away from the user
};

// A gamepad as an Atari joystick.
struct Gamepad {
	uint8_t stick = 0x0F;     // bit 0 up, 1 down, 2 left, 3 right; 0 = pressed
	bool fire = false;        // A, B, X, Y or the right trigger
	bool start = false;       // Start
	bool select = false;      // Back
};

class Host {
public:
	Host();
	~Host();
	Host(const Host &) = delete;
	Host &operator=(const Host &) = delete;

	// The window: a picture of width x height pixels, each pixelAspect as wide as it is high,
	// first at scale times its size. sampleRate: mono 16-bit sound, 0 for none. false on
	// failure (see error()).
	bool open(const char *title, int width, int height, double pixelAspect, int scale, int sampleRate);
	const std::string &error() const;
	std::string info() const;             // renderer, vsync, window size

	bool poll(Event &e);                  // the next event, false when there are none
	bool keyHeld(Key key) const;
	int gamepadCount() const;
	Gamepad gamepad(int index) const;

	void setTitle(const std::string &title);
	bool fullscreen() const;
	void setFullscreen(bool on);

	// The picture: width x height pixels, 0xRRGGBB, one row after another. Fill it, then
	// present() (which waits for the display's refresh).
	uint32_t *pixels();
	void present();
	bool savePicture(const char *bmpPath);   // the window's picture as it would be presented

	// Sound to play after what is queued; more than 1/5 s queued (after a stall) is dropped.
	void playSound(const int16_t *samples, size_t count);

	static uint64_t ticksNs();
	static void delayNs(uint64_t ns);
	static void showError(const char *title, const std::string &message);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

}  // namespace host
