// The mouse pointer over the game window: hidden by a click or by going full screen, shown
// again when the mouse moves.
#pragma once

#include <SDL3/SDL.h>

class MousePointer {
public:
	void hide() {
		SDL_HideCursor();
		hiddenAt_ = SDL_GetTicks();
	}
	// Call for every event.
	void handle(const SDL_Event &e) {
		if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) {
			hide();
		} else if (e.type == SDL_EVENT_MOUSE_MOTION && !SDL_CursorVisible()) {
			// The window changing size (going full screen) moves the pointer relative to it;
			// only a later movement is the user's.
			if (SDL_GetTicks() - hiddenAt_ > kSettleMs) SDL_ShowCursor();
		}
	}

private:
	static constexpr Uint64 kSettleMs = 500;
	Uint64 hiddenAt_ = 0;
};
