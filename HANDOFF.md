# River Raid (Atari 8-bit) → C++ port: project state

English | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | [Italiano](HANDOFF.it.md) | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

This file is for picking the work up on any computer or in a new chat. Start a new session with "read HANDOFF.md and docs/NOTES.md".

## Goal
A C++ port of River Raid (Activision, Atari 800, 1983, by Carol Shaw) for Windows, Linux and macOS:
- game logic identical to the original, readable by an ordinary programmer without 6502 knowledge;
- identical sound: the same sound driver plus a POKEY core;
- a cross-platform layer: SDL3 (SDL2 as an option; UWP on the Xbox on a fork of SDL3 that keeps it, see [port/uwp/README.md](port/uwp/README.md); the Nintendo Switch homebrew on devkitPro's SDL2, see [port/switch/README.md](port/switch/README.md));
- no original assets are distributed; the port reads the user's ROM at run time (decided: no ROM data in the code).

## State
The game is complete and playable: `port/game/river_raid.cpp` (attract mode, games 1–8, one and two players, the plane, fuel, enemies, tanks, bridges, score, lives, high scores, pause, sound). Built and played on Windows, macOS (Apple M2) and Linux. Checked frame by frame against the original's RAM in bot, two-player, idle and long runs; the sound driver writes the same POKEY values. Command line and keys: [port/README.md](port/README.md), "The game". How the game works, for everyone: [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md).

## What you need
- The whole project folder.
- Your own ROM `River_Raid.rom`: 8192 bytes, no header, loads at $A000–$BFFF. It is not in the repository; put it at `rom/River_Raid.rom`.

## Layout
| Path | What |
|---|---|
| `port/` | the C++ port: game, river viewer, check tools; see [port/README.md](port/README.md) (build, options, keys, contents, verification) |
| [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) | how the game works, for everyone: the river, enemies, bridges, collisions, why it gets harder |
| [docs/NOTES.md](docs/NOTES.md) | findings with addresses |
| [docs/DLI_REMOVAL.md](docs/DLI_REMOVAL.md) | experiments: the display without the DLI model (branches `dli_removal`, `dli_removed`) |
| [docs/OOP.md](docs/OOP.md) | the object-oriented rewrite (branch `oop`): rules, checks, steps |
| `disasm/river_raid_a8.asm` | full listing with a header (entry points, frame, variables) |
| `tools/dis6502.py` | disassembler: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py`, `tools/verify_trace.py` | checks of documented rules against reference traces |
| `tools/level_map.py` | pictures of the whole river, section by section |
| `tools/doc_figures.py` | the pictures in [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md) (`docs/img/`), from `port/tools/river_dump` |
| `oracle/` | reference runner on libatari800: `rr_oracle.c`, `build.sh`, `README.md`, input scripts, `-bot` autopilot, `-ghost` invulnerable plane |
| `docs/oracle_demo.png` | frames from a reference run |

## Established facts (all verified)
1. **Handlers.** Main loop A000–AF5A, one pass = one frame (synchronised on VCOUNT $50/$78). VBI $B2A5 = POKEY sound driver and counters. DLI $B500 = per-line sprite multiplexing and collision reading. IRQ $B485 = stub.
2. **River generator.** Software 24-bit LFSR (`sub_B183`, $AA/$AB/$AC), stepped once per line; everything (banks, islands, decorations, objects) comes from it and from the level. It depends neither on the player nor on RANDOM. The river does not repeat for 16.8 million bridges (LFSR period 16 766 977 steps, 993 steps per bridge). C++ (`port/src/river_gen.cpp`) matches byte for byte: 119 758 lines and starts from scratch.
3. **Starting points.** Games 1–8 start at bridges 1/5/20/50 of the same river: the tables $B494–$B4A8 hold the LFSR, bank, level and bridge counter there; running the generator from game 1's start reaches exactly those values. The port fills the same table by running the generator for `-seed N` (another river) and `-bridge N` (start at any bridge).
4. **Difficulty.** The level (= the bridge you fly towards, up to 80, then 79/80 alternating) moves the thresholds the random numbers are compared with: enemies in mid-block, fewer depots up to level 46 (47 on PAL), balloons from 3, jets from 5, bank tanks from 7, missile helicopters from 13, wider bends. Table in [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md).
5. **RANDOM ($D20A).** Game logic reads it only at A205: every 16 frames a random slot (0–10) wakes up (bit $40 of `$42,X`). B362/B3D0: noise for sound. The value depends on the CPU cycle of the read; the port models the counter (`src/pokey_random.h`), seeded from the clock like a real power-on; `-random N` repeats a run.
6. **Collisions** come from the GTIA collision registers, read by the DLI after each object band; whether a touch on the read line counts depends on the cycle. The port runs the DLI with cycle counts (`src/display.cpp`, atari800's non-cycle-exact timing). On the `dli_removed` and `oop` branches there is no DLI: the collisions are plain geometry (below).
7. **PAL/NTSC.** One cartridge for both: the PAL bit ($D014 → $BF) chooses the display list variant ($B77A) and the depot limit ($CB = $2E/$2F). The game logic per frame is the same, so on PAL the whole game runs at 50 frames per second, about 17% slower; only what depends on the CPU time in a frame differs: power-on reaches the attract mode at frame 25 instead of 38, sub $B0F1 clears memory in 8 frames instead of 13, and at a game over the VBI comes before sub $B21A. The port's `-pal` does all of it (checked against `rr_oracle -pal`, see [port/README.md](port/README.md)). The UK cassette version (`.cas`, 2 files: loader at $0A00, game at $1000–$31FF) is the same code relocated to RAM, with a different copy protection and "1982, 1983, 1984" in the scrolling line.
8. **Font** is copied from the OS ROM ($E000/$E100/$E300); the port has AltirraOS's font built in, `-os` loads Atari's.
9. **Reference runner.** atari800 seeds POKEY from the clock; `rr_oracle -seed` (default 0) makes runs byte-identical. The atari800 build fails with `-Werror` on a ULONG redefinition; `build.sh` builds with `CFLAGS="-g -O2 -DNETSIO"`. Tested atari800 commit: 98cc2dc.
10. **Determinism.** The river depends only on the bridge (993 LFSR steps from bridge to bridge; it comes back after 16.8 million bridges), the objects only on the last ~200 frames. So any moment is reached by running the river alone and then a few hundred whole frames; the viewer seeks that way. Details: [docs/NOTES.md](docs/NOTES.md), "Determinism and jumping". Saved states in the game use the same: screen memory is made again from the generator's state (on the `oop` branch, the river's and the objects'), so a saved state is about 0.2–0.6 KB.

## Environment limits
- `oracle/build.sh` builds on Linux, macOS, WSL and MSYS2 (intercepted emulator functions are renamed at compile time, no GNU `--wrap`); the C++ port builds with plain CMake (MSVC, Xcode, gcc).
- The ROM is not in the repository (`.gitignore`); put your dump at `rom/River_Raid.rom`.
- Code is indented with tabs, continuation lines too (`.editorconfig`). Documentation: `X.md` in English, `X.ru.md` in Russian, and `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` writes the language line under each title, the picture labels are in `tools/doc_figures_i18n.py`. A change to a document goes into every language.

## UWP and the Xbox
- Built on a fork of SDL3 that keeps UWP: [djdron/SDL3-uwp](https://github.com/djdron/SDL3-uwp), branch `river-raid` (on XboxEmulationHub/SDL3-uwp, branch `uwp-3.4.20`); `port/CMakeLists.txt` takes it for `CMAKE_SYSTEM_NAME=WindowsStore`. The patches are for this project only: SDL does not accept contributions written with generative AI, so they are not offered upstream. What they change: [docs/NOTES.md](docs/NOTES.md), "UWP and the SDL3 fork". After a change to the fork, delete `_deps/sdl3-*` in the build folder and run CMake again.
- CI: `.github/workflows/uwp.yml` builds the UWP app on windows-latest, without the ROM, on every push that changes `port/`; it only shows that it compiles and packages. A change to the fork alone does not start it.
- Tried with Visual Studio 2026 on Windows 11 (at 175% scaling) and on an Xbox Series S in Dev Mode, on SDL2 and on SDL3.
- On the Xbox: someone must be signed in (otherwise Visual Studio reports 0x8004090a); set App type to Game in Dev Home. A button held while the app starts never reaches it, so PAL is chosen in the game (Back + Start). In Dev Home's Launch with parameters (Use custom protocol) everything after the first space is dropped, so spaces are written as `%20`: `riverraid:-pal%20-bridge%20100`. On a PC the link (Win+R) and Visual Studio's Command Line Arguments take spaces as they are.

## Branch `dli_removed`
This branch (from `dli_removal`) has no DLI: `Game::displayGeometry` places each object at its slot's y and works out the collisions as plain geometry, where the sprites' and the objects' rows touch at all; the bank and the edge by a row's first and last dots against the line's runs. It is the default here (`RR_DISPLAY=geometry`); `RR_DISPLAY=simple` is the DLI's loop with its reads, as the original, `beam` the cycle model, `check` both. The game differs from the original on 0.01–0.25% of frames (a touch the original sees a frame later or not at all), so `game_check` against the original is run with `RR_DISPLAY=simple`; `RR_DISPLAY=simple RR_GEOMCHECK=1` counts the frames where the geometry and the reads differ (`=2` prints each). The ZX Spectrum Next and TS-Conf ports ([river_raid_zx](https://github.com/djdron/river_raid_zx)) are built on this display and checked against it frame by frame. The steps and their checks: [docs/DLI_REMOVAL.md](docs/DLI_REMOVAL.md).

## Branch `oop`
This branch (from `dli_removed`) is the same game written as objects that keep their own state: the plane, its bullet, the river generator (`RiverGen`, the river alone), the objects on the river (`Objects`, a class per kind of behaviour), the tank, its shell and the missile, the players, the panel, the sounds, the river on the screen; `World` runs the river and its objects together, `Game` holds everything and runs the frame. Bytes and BCD as the original keeps them, no floating point; no player-missile memory and none of the DLI models (`RR_DISPLAY` is gone: the geometry of `dli_removed` is the display). It plays exactly as `dli_removed`: `port/tools/trace/compare.sh` compares 17 runs with the reference commit frame by frame (`-quick`: 5). Each class lists its bytes in the original's RAM (`mapRam`), so `game_check` and the ZX ports' checks still work. The steps: [docs/OOP.md](docs/OOP.md).

## Method
A rule is read from the listing, then checked against a reference trace (`tools/verify_trace.py`, then the C++ check tools). Traces alone are only used to find where to look. Changes that should not change behaviour (renaming, restructuring) are checked with `port/tools/equivalence` against an older commit: 0 differences required, plus all checks in [port/README.md](port/README.md), "Verification". The object-oriented rewrite (branch `oop`) is checked with `port/tools/trace/compare.sh` instead: 17 runs of the game, frame by frame, against the reference commit.

## Decisions
- The port is written in C++, not as a wrapper around an emulator.
- Verification compares the port's RAM with the reference `ram.bin` frame by frame; RANDOM values for it come from the reference `random.log`.
- No ROM data in the code: the cartridge's tables are read through named accessors (`port/src/rom.h`).
- Default colours: PAL, Altirra's model; NTSC and atari800's model on keys.
- The window shows the game's picture (364×200 (`-pal`: 364×213) with the whole scrolling line), not the whole frame with its black border (`-border`, O); scaled sharp by whole numbers, then smoothly (`-filter`, L).
- The game-over cycle race (whether the frame counter is reset before or after that frame's VBI depends on the main loop's cycles, see [port/README.md](port/README.md), Verification) is deliberately not reproduced.
- On a 60/120 Hz display the game locks to the display (60.00 instead of 59.92 frames per second; with `-pal` on a 50/100 Hz display, 50.00 instead of 49.86), sound resampled to match.
- PAL/NTSC can also be switched in the game (P; gamepad Back + Start): the machine is switched on again as the other one; on the Xbox the TV goes to 50 Hz and back.

## Open questions
- Power-on itself (A000–A17C, the first 38 frames) is drawn as an empty screen; `Game::powerOn` is not exact.

## Next steps
Nothing planned.
