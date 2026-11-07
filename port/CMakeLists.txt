cmake_minimum_required(VERSION 3.16)
project(river_raid_port C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
	set(RR_WARNINGS /W4)
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

# Byte-exact comparison with traces of the original (oracle/rr_oracle -ram A:B).
add_executable(terrain_check tools/terrain_check.cpp)
target_link_libraries(terrain_check rrcore)
target_compile_options(terrain_check PRIVATE ${RR_WARNINGS})

# Generate the river with the C++ generator alone and save it as a PNG.
add_executable(river_map tools/river_map.cpp)
target_link_libraries(river_map rrcore)
target_compile_options(river_map PRIVATE ${RR_WARNINGS})

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

# The game (river_raid) and the river viewer (river_view), SDL3.
# Uses an installed SDL3 if CMake finds one, otherwise downloads and builds SDL3 (static).
option(RR_VIEWER "Build the SDL3 game and river viewer" ON)
if(RR_VIEWER)
	find_package(SDL3 CONFIG QUIET)
	if(NOT SDL3_FOUND)
		include(FetchContent)
		set(SDL_SHARED OFF CACHE BOOL "" FORCE)
		set(SDL_STATIC ON CACHE BOOL "" FORCE)
		set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
		FetchContent_Declare(SDL3
			GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
			GIT_TAG release-3.2.24
			GIT_SHALLOW TRUE
		)
		FetchContent_MakeAvailable(SDL3)
	endif()
	add_executable(river_raid WIN32 game/river_raid.cpp)
	target_link_libraries(river_raid rrcore SDL3::SDL3)
	target_compile_options(river_raid PRIVATE ${RR_WARNINGS})
	add_executable(river_view WIN32 viewer/river_view.cpp)
	target_link_libraries(river_view rrcore SDL3::SDL3)
	target_compile_options(river_view PRIVATE ${RR_WARNINGS})
endif()
