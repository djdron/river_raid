# River Raid on the Xbox (UWP)

English | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

The game (not the viewer) as a UWP app: it runs on Windows 10/11 and on an Xbox One or Series in Dev Mode. It is built on SDL2, as SDL3 no longer supports UWP. Tried on Windows 11 and on an Xbox Series S, built with Visual Studio 2026.

## What you need
- Windows 10/11 with Visual Studio 2022 or 2026 and the C++ tools for UWP. In 2022: the workloads "Desktop development with C++" and "Universal Windows Platform development" (with the C++ UWP tools). In 2026: "Desktop development with C++" and, in "WinUI application development", the component "C++ Universal Windows Platform tools".
- CMake 3.16 or newer (the one in Visual Studio will do) and git: SDL2 is downloaded and built with the game.
- For the Xbox: a developer account and the console in Dev Mode (the "Dev Mode Activation" app on the Xbox).
- Your dump of the cartridge as `rom/River_Raid.rom` (8192 bytes): it is put **into the package**, so the package is for you alone; do not share it.

## Building
```
cmake -S port -B build-uwp -G "Visual Studio 17 2022" -A x64 -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0
```
With Visual Studio 2026 the generator is `-G "Visual Studio 18 2026"` (CMake 4.2 or newer, such as the one that comes with it).

Open `build-uwp/river_raid_port.sln`, choose Release and x64, and set `river_raid` as the startup project. Run it on the local machine first (Windows): the window should show the game.

The package is signed with the test certificate that comes with CMake (`CN=CMake Test Cert`). Your own: `-DRR_UWP_CERTIFICATE=my.pfx -DRR_UWP_PUBLISHER="CN=..."` (the subject of the certificate). Another ROM path: `-DRR_UWP_ROM=...`.

## Onto the Xbox
Either way works:
- **From Visual Studio:** Debug target "Remote Machine", the Xbox's IP address (shown in Dev Home), authentication "Universal (Unencrypted Protocol)"; pair with the PIN Dev Home shows.
- **A package through Device Portal:** in Visual Studio Project → Publish → Create App Packages → Sideloading, x64; then in a browser on the PC open `https://<xbox ip>:11443` (Device Portal, turned on in Dev Home), Add, and choose the `.msix` with the dependencies from its folder (the VCLibs package).

Then in Dev Home select the app, View details, and set App type to **Game**: an app gets less of the console otherwise. Someone has to be signed in on the Xbox: otherwise the app does not start (Visual Studio: "Unable to activate Windows Store app … 0x8004090a").

The gamepad works as on a PC (see [../README.md](../README.md)), X pauses. With no keyboard the other settings stay at their defaults: PAL colours, Altirra, the game's picture only, sharp scaling (LT switches it). B is also the system's Back button; the game takes it, so it does not close the app. The Xbox button leaves the game.

Hold LT while the game starts to play as a PAL machine (as `-pal`: 50 frames per second, about 17% slower): the game then switches the TV to 50 Hz, if the console allows it (Settings → TV & display options → Video modes → Allow 50 Hz), and back when it ends. Without 50 Hz it still runs as PAL, not locked to the TV.
