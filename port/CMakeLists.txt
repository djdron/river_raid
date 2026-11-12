cmake_minimum_required(VERSION 3.16)
project(river_raid_port C CXX)

set(CMAKE_CXX_STANDARD 17)
# UWP (Windows 10 apps, the Xbox in Dev Mode): -DCMAKE_SYSTEM_NAME=WindowsStore
# -DCMAKE_SYSTEM_VERSION=10.0 with a Visual Studio generator. Only the game is built, on SDL2
# (SDL3 has dropped UWP); see uwp/README.md.
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
	set(RR_UWP ON)
	set(RR_SDL2 ON CACHE BOOL "Use SDL2 instead of SDL3" FORCE)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
	set(RR_WARNINGS /W4)
	# fopen and the like are fine here; UWP projects turn on /sdl, which makes their warning an error
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	set(RR_WARNINGS -Wall -Wextra)
endif()

# Game logic library: no platform code, reads the user's ROM for tables and graphics.
add_library(rrcore
	src/display.cpp
	src/game.cpp
	src/game.h
	src/game_ram.cpp
	src/os_font.cpp
	src/os_font.h
	src/palette.cpp
	src/palette.h
	src/pokey_random.h
	src/pokey_sound.cpp
	src/pokey_sound.h
	src/river_draw.cpp
	src/river_draw.h
	src/river_gen.cpp
	src/river_gen.h
	src/river_state_io.cpp
	src/rom.h
	src/status_bar.cpp
	src/status_bar.h
)
target_include_directories(rrcore PUBLIC src)
target_compile_options(rrcore PRIVATE ${RR_WARNINGS})

if(NOT RR_UWP)   # the checks and tools: command line programs, not for UWP
# Byte-exact comparison with traces of the original (oracle/rr_oracle -ram A:B).
add_executable(terrain_check tools/terrain_check.cpp)
target_link_libraries(terrain_check rrcore)
target_compile_options(terrain_check PRIVATE ${RR_WARNINGS})

# Generate the river with the C++ generator alone and save it as a PNG.
add_executable(river_map tools/river_map.cpp)
target_link_libraries(river_map rrcore)
target_compile_options(river_map PRIVATE ${RR_WARNINGS})
add_executable(river_dump tools/river_dump.cpp)
target_link_libraries(river_dump rrcore)
target_compile_options(river_dump PRIVATE ${RR_WARNINGS})

# Compare the drawing with the screen of the original (oracle/rr_oracle -ghost -strip 20).
add_executable(render_check tools/render_check.cpp)
target_link_libraries(render_check rrcore)
target_compile_options(render_check PRIVATE ${RR_WARNINGS})

# Compare the game frame (object movement, tank, collisions) with a RAM trace of the original.
add_executable(game_check tools/game_check.cpp)
target_link_libraries(game_check rrcore)
target_compile_options(game_check PRIVATE ${RR_WARNINGS})

# Compare the sound driver with the POKEY writes of the original (rr_oracle -vbi).
add_executable(sound_check tools/sound_check.cpp)
target_link_libraries(sound_check rrcore)
target_compile_options(sound_check PRIVATE ${RR_WARNINGS})

# Render a POKEY write log (rr_oracle pokey.log) to WAV, to compare with the emulator's sound.
add_executable(sound_render tools/sound_render.cpp)
target_link_libraries(sound_render rrcore)
target_compile_options(sound_render PRIVATE ${RR_WARNINGS})

# Compare the status panel and scrolling line with screens of the original (rr_oracle -screens).
add_executable(status_check tools/status_check.cpp)
target_link_libraries(status_check rrcore)
target_compile_options(status_check PRIVATE ${RR_WARNINGS})
endif()

# The game (river_raid) and the river viewer (river_view): window, sound and input through
# SDL (sdl/host.h). SDL3 by default; RR_SDL2=ON uses SDL2 instead (for platforms SDL3 has
# dropped, such as UWP on the Xbox). Uses an installed SDL if CMake finds one, otherwise
# downloads and builds it (static).
option(RR_VIEWER "Build the SDL game and river viewer" ON)
option(RR_SDL2 "Use SDL2 instead of SDL3" OFF)
if(RR_VIEWER)
	include(FetchContent)
	if(RR_SDL2)
		if(NOT RR_UWP)   # UWP: built here, as its WinRT entry point comes from SDL's sources
			find_package(SDL2 2.0.18 CONFIG QUIET)
		endif()
		if(NOT SDL2_FOUND)
			set(SDL_SHARED OFF CACHE BOOL "" FORCE)
			set(SDL_STATIC ON CACHE BOOL "" FORCE)
			set(SDL_TEST OFF CACHE BOOL "" FORCE)
			FetchContent_Declare(SDL2
				GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
				GIT_TAG release-2.32.10
				GIT_SHALLOW TRUE
			)
			FetchContent_MakeAvailable(SDL2)
		endif()
		if(TARGET SDL2::SDL2-static)
			set(RR_SDL_LIBS SDL2::SDL2-static)
		else()
			set(RR_SDL_LIBS SDL2::SDL2)
		endif()
		if(TARGET SDL2::SDL2main)
			list(PREPEND RR_SDL_LIBS SDL2::SDL2main)
		endif()
		set(RR_HOST sdl/host_sdl2.cpp)
	else()
		# 3.4 or newer: older versions move the mouse pointer when switching full screen
		find_package(SDL3 3.4 CONFIG QUIET)
		if(NOT SDL3_FOUND)
			set(SDL_SHARED OFF CACHE BOOL "" FORCE)
			set(SDL_STATIC ON CACHE BOOL "" FORCE)
			set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
			FetchContent_Declare(SDL3
				GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
				GIT_TAG release-3.4.20
				GIT_SHALLOW TRUE
			)
			FetchContent_MakeAvailable(SDL3)
		endif()
		set(RR_SDL_LIBS SDL3::SDL3)
		set(RR_HOST sdl/host_sdl3.cpp)
	endif()
	add_library(rrhost STATIC sdl/host.h sdl/host_main.h ${RR_HOST})
	target_include_directories(rrhost PUBLIC sdl)
	target_link_libraries(rrhost PUBLIC ${RR_SDL_LIBS})
	target_compile_options(rrhost PRIVATE ${RR_WARNINGS})
	if(RR_SDL2)
		target_compile_definitions(rrhost PUBLIC RR_SDL2)
	endif()
	if(RR_UWP)
		include(uwp/uwp.cmake)
	else()
		add_executable(river_raid WIN32 game/river_raid.cpp)
		target_link_libraries(river_raid rrcore rrhost)
		target_compile_options(river_raid PRIVATE ${RR_WARNINGS})
		add_executable(river_view WIN32 viewer/river_view.cpp)
		target_link_libraries(river_view rrcore rrhost)
		target_compile_options(river_view PRIVATE ${RR_WARNINGS})
	endif()
endif()
