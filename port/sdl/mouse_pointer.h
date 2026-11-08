// The mouse pointer over the game window: hidden by a click or by going full screen, shown
// again when the mouse moves.
#pragma once

#include <cmath>

#include <SDL3/SDL.h>

class MousePointer {
public:
	void hide() {
		SDL_HideCursor();
		SDL_GetGlobalMouseState(&hiddenX_, &hiddenY_);
	}
	// Call for every event.
	void handle(const SDL_Event &e) {
		if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN) {
			hide();
		} else if (e.type == SDL_EVENT_MOUSE_MOTION && !SDL_CursorVisible()) {
			// When the window changes size or place (going full screen), the pointer stays where it
			// is on the screen, but its position in the window changes and SDL reports that as
			// motion. Only a change of the position on the screen is the user moving the mouse.
			float x, y;
			SDL_GetGlobalMouseState(&x, &y);
			if (std::fabs(x - hiddenX_) >= 1.0f || std::fabs(y - hiddenY_) >= 1.0f) SDL_ShowCursor();
		}
	}

private:
	float hiddenX_ = 0, hiddenY_ = 0;
};
