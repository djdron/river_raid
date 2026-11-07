cmake_minimum_required(VERSION 3.16)
project(river_raid_port CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
	add_compile_options(/W4)
else()
	add_compile_options(-Wall -Wextra)
endif()

# Game logic library: no platform code, reads the user's ROM for tables and graphics.
add_library(rrcore
	src/river_draw.cpp
	src/river_gen.cpp
	src/river_state_io.cpp
)
target_include_directories(rrcore PUBLIC src)

# Byte-exact comparison with traces of the original (oracle/rr_oracle -ram A:B).
add_executable(terrain_check tools/terrain_check.cpp)
target_link_libraries(terrain_check rrcore)

# Generate the river with the C++ generator alone and save it as a PNG.
add_executable(river_map tools/river_map.cpp)
target_link_libraries(river_map rrcore)

# Compare the drawing with the screen of the original (oracle/rr_oracle -ghost -strip 0).
add_executable(render_check tools/render_check.cpp)
target_link_libraries(render_check rrcore)
