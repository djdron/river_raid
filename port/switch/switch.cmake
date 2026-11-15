# The game as a Nintendo Switch homebrew (.nro), included from CMakeLists.txt with devkitPro's
# toolchain. The ROM is not inside: it is read from the SD card (switch/README.md).
add_executable(river_raid game/river_raid.cpp)
target_link_libraries(river_raid rrcore rrhost)
target_compile_options(river_raid PRIVATE ${RR_WARNINGS})
target_compile_definitions(river_raid PRIVATE RR_SWITCH)
target_compile_definitions(rrhost PRIVATE RR_SWITCH)

nx_generate_nacp(river_raid.nacp
	NAME "River Raid (port)"
	AUTHOR "djdron"
	VERSION "1.0"
)
nx_create_nro(river_raid
	NACP river_raid.nacp
	ICON "${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg"
)
