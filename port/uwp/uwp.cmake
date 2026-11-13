# The game as a UWP app: Windows 10 and the Xbox in Dev Mode (see uwp/README.md). Included from
# CMakeLists.txt when CMAKE_SYSTEM_NAME is WindowsStore, after SDL2 (built from source here,
# as the app's WinRT entry point comes from SDL's sources).

set(RR_UWP_ROM "${CMAKE_CURRENT_SOURCE_DIR}/../rom/River_Raid.rom" CACHE FILEPATH
	"Your dump of the cartridge, put into the app package (not in the repository)")
# The package is signed: by default with the test certificate that comes with CMake, whose
# subject the manifest's Publisher has to be.
set(RR_UWP_CERTIFICATE "${CMAKE_ROOT}/Templates/Windows/Windows_TemporaryKey.pfx" CACHE FILEPATH
	"Certificate (.pfx, no password) to sign the package")
set(RR_UWP_PUBLISHER "CN=CMake Test Cert" CACHE STRING "Subject of the signing certificate")
set(RR_UWP_VERSION "1.0.0.0" CACHE STRING "Package version")

configure_file(uwp/Package.appxmanifest.in "${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest" @ONLY)
configure_file("${RR_UWP_CERTIFICATE}" "${CMAKE_CURRENT_BINARY_DIR}/river_raid.pfx" COPYONLY)

set(RR_UWP_ASSETS
	uwp/Assets/StoreLogo.png
	uwp/Assets/Square44x44Logo.png
	uwp/Assets/Square150x150Logo.png
	uwp/Assets/Wide310x150Logo.png
	uwp/Assets/SplashScreen.png
)
set_property(SOURCE ${RR_UWP_ASSETS} PROPERTY VS_DEPLOYMENT_CONTENT 1)
set_property(SOURCE ${RR_UWP_ASSETS} PROPERTY VS_DEPLOYMENT_LOCATION "Assets")

set(RR_UWP_CONTENT ${RR_UWP_ASSETS})
if(EXISTS "${RR_UWP_ROM}")
	# found by the game in the package's folder (Host::basePath), as rom/River_Raid.rom
	set_property(SOURCE "${RR_UWP_ROM}" PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE "${RR_UWP_ROM}" PROPERTY VS_DEPLOYMENT_LOCATION "rom")
	list(APPEND RR_UWP_CONTENT "${RR_UWP_ROM}")
else()
	message(WARNING "No ROM at ${RR_UWP_ROM} (RR_UWP_ROM): the package will not run without it")
endif()

# SDL2's entry point for WinRT apps, and what the game takes from UWP (uwp/app.h): C++/CX
set(RR_UWP_MAIN "${sdl2_SOURCE_DIR}/src/main/winrt/SDL_winrt_main_NonXAML.cpp")
set_property(SOURCE "${RR_UWP_MAIN}" uwp/app.cpp PROPERTY COMPILE_OPTIONS /ZW /EHsc)

add_executable(river_raid WIN32
	game/river_raid.cpp
	uwp/app.cpp
	"${RR_UWP_MAIN}"
	"${CMAKE_CURRENT_BINARY_DIR}/Package.appxmanifest"
	"${CMAKE_CURRENT_BINARY_DIR}/river_raid.pfx"
	${RR_UWP_CONTENT}
)
target_link_libraries(river_raid rrcore rrhost)
target_compile_options(river_raid PRIVATE ${RR_WARNINGS})
target_compile_definitions(river_raid PRIVATE RR_UWP)   # river_raid.cpp: launch options, PAL
target_include_directories(river_raid PRIVATE uwp)
# as SDL is built for UWP: its headers then see a WinRT app (__WINRT__, SDL_main)
target_compile_definitions(rrhost PUBLIC WINAPI_FAMILY=WINAPI_FAMILY_APP)
