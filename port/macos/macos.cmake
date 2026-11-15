# The game as a macOS app bundle (river_raid.app) with its icon and the ROM inside
# (Contents/Resources/rom/River_Raid.rom, where the game looks first), included from
# CMakeLists.txt. A bundle with the ROM is yours alone: do not share it.
set(RR_MACOS_ROM "${CMAKE_CURRENT_SOURCE_DIR}/../rom/River_Raid.rom" CACHE FILEPATH
	"Your dump of the cartridge, put into the app bundle (not in the repository)")

set(RR_MACOS_ICON "${CMAKE_CURRENT_SOURCE_DIR}/macos/river_raid.icns")
set_source_files_properties("${RR_MACOS_ICON}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
target_sources(river_raid PRIVATE "${RR_MACOS_ICON}")
if(EXISTS "${RR_MACOS_ROM}")
	set_source_files_properties("${RR_MACOS_ROM}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources/rom)
	target_sources(river_raid PRIVATE "${RR_MACOS_ROM}")
else()
	message(WARNING "No ROM at ${RR_MACOS_ROM} (RR_MACOS_ROM): the app will look for it next to itself")
endif()

set_target_properties(river_raid PROPERTIES
	MACOSX_BUNDLE TRUE
	MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
	MACOSX_BUNDLE_BUNDLE_NAME "River Raid"
	MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.djdron.riverraid"
	MACOSX_BUNDLE_ICON_FILE "river_raid.icns"
	MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
	MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
)
