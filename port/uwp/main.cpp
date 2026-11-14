// SDL's entry point for a UWP app (WinMain, which starts the CoreApplication and then calls the
// game's main()): SDL_main.h defines it in the one file that includes it without SDL_MAIN_NOIMPL,
// and that file has to be compiled as C++/CX (/ZW, see uwp.cmake). river_raid.cpp has main().
#include <SDL3/SDL_main.h>
