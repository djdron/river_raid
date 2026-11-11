# River Raid on the Xbox (UWP)

English | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

The game (not the viewer) as a UWP app: it runs on Windows 10/11 and on an Xbox One or Series in Dev Mode. It is built on SDL2, as SDL3 no longer supports UWP. **Not tried yet**: this was written without Windows at hand; if a step does not work as described, please report what happened.

## What you need
- Windows 10/11 with Visual Studio 2022: the workloads "Desktop development with C++" and "Universal Windows Platform development" (with the C++ UWP tools, v143).
- CMake 3.16 or newer (the one in Visual Studio will do) and git: SDL2 is downloaded and built with the game.
- For the Xbox: a developer account and the console in Dev Mode (the "Dev Mode Activation" app on the Xbox).
- Your dump of the cartridge as `rom/River_Raid.rom` (8192 bytes): it is put **into the package**, so the package is for you alone; do not share it.

## Building
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
Open `build-uwp/river_raid_port.sln`, choose Release and x64, and set `river_raid` as the startup project. Run it on the local machine first (Windows): the window should show the game.

The package is signed with the test certificate that comes with CMake (`CN=CMake Test Cert`). Your own: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (the subject of the certificate). Another ROM path: `-DRR_UWP_ROM=...`.

## Onto the Xbox
Either way works:
- **From Visual Studio:** Debug target "Remote Machine", the Xbox's IP address (shown in Dev Home), authentication "Universal (Unencrypted Protocol)"; pair with the PIN Dev Home shows.
- **A package through Device Portal:** in Visual Studio Project → Publish → Create App Packages → Sideloading, x64; then in a browser on the PC open `https://<xbox ip>:11443` (Device Portal, turned on in Dev Home), Add, and choose the `.msix` with the dependencies from its folder (the VCLibs package).

Then in Dev Home select the app, View details, and set App type to **Game**: an app gets less of the console otherwise.

## Controls on the Xbox
| Gamepad | |
|---|---|
| D-pad or left stick | joystick |
| A, B, Y or the right trigger | fire |
| X | the key that pauses (the Xbox has no keyboard) |
| Menu | START |
| View | SELECT |
| RB / LB | save the state / go back to it |

Colours, palette, border and scaling are the defaults (PAL colours, Altirra, the game's picture only, sharp). B is also the system's Back button; the game takes it, so it does not close the app. The Xbox button leaves the game.

## What may need doing
- The TV's safe area: UWP apps on the Xbox may be drawn with a margin from the edges of the screen. The picture is surrounded by black anyway; if the margin is a nuisance, the app can ask for the whole screen (`ApplicationView::SetDesiredBoundsMode`).
- How the files are put together (`uwp/uwp.cmake`, `uwp/Package.appxmanifest.in`): CMake's support for UWP projects and SDL2's WinRT entry point (`SDL_winrt_main_NonXAML.cpp`, compiled as C++/CX) are used as documented, but have not been built here.
