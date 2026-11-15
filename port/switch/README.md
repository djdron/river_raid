# River Raid on the Nintendo Switch (homebrew)

English | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

The game (without the viewer) as a homebrew app, an `.nro` for a Switch that runs homebrew through the Homebrew Launcher. Built with devkitPro and its SDL2 (SDL itself does not support the Switch). It builds; not yet tried on a console.

## What you need
- devkitPro with the Switch packages `switch-dev` and `switch-sdl2` (installed with `dkp-pacman`), or its Docker image `devkitpro/devkita64`.
- Your own dump of the cartridge, `River_Raid.rom` (8192 bytes). It is not put into the `.nro`, so the `.nro` itself can be shared.

## Build
From the repository's root:
```
cmake -S port -B build-switch -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-switch
```
The result is `build-switch/river_raid.nro`. The CI (`.github/workflows/switch.yml`) builds it on every push that changes `port/` and keeps it as the run's artifact.

## On the console
Copy to the SD card:
```
/switch/river_raid/river_raid.nro
/switch/river_raid/rom/River_Raid.rom
```
and start River Raid (port) from the Homebrew Launcher. Without the ROM the game says so and waits for +.

The controls are those of a gamepad on a PC (see [../README.md](../README.md)): the left stick or the D-pad steers, A, B, Y or ZR fire, X pauses, R saves the state and L goes back to it, ZL switches the scaling; − is SELECT and + is START, both together switch between NTSC and PAL. Leave the game with the HOME button (close the software). The Switch's screen and its TV output run at 60 Hz only, so a PAL machine runs at its own 49.86 frames per second, not locked to the screen.
