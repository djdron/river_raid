# The game as a Nintendo Switch homebrew (.nro), included from CMakeLists.txt with devkitPro's
# toolchain. The ROM goes into the .nro's RomFS (romfs:/rom/River_Raid.rom), so that .nro is
# yours alone; without it the game looks for the ROM on the SD card (switch/README.md).
set(RR_SWITCH_ROM "${CMAKE_CURRENT_SOURCE_DIR}/../rom/River_Raid.rom" CACHE FILEPATH
	"The River Raid ROM (8192 bytes) to put into the .nro")

add_executable(river_raid game/river_raid.cpp)
target_link_libraries(river_raid rrcore rrhost)
target_compile_options(river_raid PRIVATE ${RR_WARNINGS})

set(RR_SWITCH_ROMFS "${CMAKE_CURRENT_BINARY_DIR}/romfs")
file(MAKE_DIRECTORY "${RR_SWITCH_ROMFS}/rom")
if(EXISTS "${RR_SWITCH_ROM}")
	configure_file("${RR_SWITCH_ROM}" "${RR_SWITCH_ROMFS}/rom/River_Raid.rom" COPYONLY)
else()
	file(REMOVE "${RR_SWITCH_ROMFS}/rom/River_Raid.rom")
	message(WARNING "No ROM at ${RR_SWITCH_ROM} (RR_SWITCH_ROM): the .nro will look for it on the SD card")
endif()

nx_generate_nacp(river_raid.nacp
	NAME "River Raid (port)"
	AUTHOR "djdron"
	VERSION "1.0"
)
nx_create_nro(river_raid
	NACP river_raid.nacp
	ICON "${CMAKE_CURRENT_SOURCE_DIR}/switch/icon.jpg"
	ROMFS "${RR_SWITCH_ROMFS}"
)
