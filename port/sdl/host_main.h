// Include once, in the file with main(): SDL takes over the program's entry point on the
// platforms that need it (Windows without a console, mobile, UWP).
#pragma once

#ifdef RR_SDL2
#include <SDL.h>
#else
#include <SDL3/SDL_main.h>
#endif
