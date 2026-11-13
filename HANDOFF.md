# River Raid (Atari 8-bit) → C++ port: project state

English | [Русский](HANDOFF.ru.md) | [Українська](HANDOFF.uk.md) | [Deutsch](HANDOFF.de.md) | [Polski](HANDOFF.pl.md) | [Italiano](HANDOFF.it.md) | [Español](HANDOFF.es.md) | [中文](HANDOFF.zh.md) | [日本語](HANDOFF.ja.md)

This file is for picking the work up on any computer or in a new chat. Start a new session with "read HANDOFF.md and docs/NOTES.md".

## Goal
A C++ port of River Raid (Activision, Atari 800, 1983, by Carol Shaw) for Windows, Linux and macOS:
- game logic identical to the original, readable by an ordinary programmer without 6502 knowledge;
- identical sound: the same sound driver plus a POKEY core;
- a cross-platform layer: SDL3 (SDL2 as an option, for UWP on the Xbox);
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
6. **Collisions** come from the GTIA collision registers, read by the DLI after each object band; whether a touch on the read line counts depends on the cycle. The port runs the DLI with cycle counts (`src/display.cpp`, atari800's non-cycle-exact timing).
7. **PAL/NTSC.** One cartridge for both: the PAL bit ($D014 → $BF) chooses the display list variant ($B77A) and the depot limit ($CB = $2E/$2F). The game logic per frame is the same, so on PAL the whole game runs at 50 frames per second, about 17% slower; only what depends on the CPU time in a frame differs: power-on reaches the attract mode at frame 25 instead of 38, sub $B0F1 clears memory in 8 frames instead of 13, and at a game over the VBI comes before sub $B21A. The port's `-pal` does all of it (checked against `rr_oracle -pal`, see [port/README.md](port/README.md)). The UK cassette version (`.cas`, 2 files: loader at $0A00, game at $1000–$31FF) is the same code relocated to RAM, with a different copy protection and "1982, 1983, 1984" in the scrolling line.
8. **Font** is copied from the OS ROM ($E000/$E100/$E300); the port has AltirraOS's font built in, `-os` loads Atari's.
9. **Reference runner.** atari800 seeds POKEY from the clock; `rr_oracle -seed` (default 0) makes runs byte-identical. The atari800 build fails with `-Werror` on a ULONG redefinition; `build.sh` builds with `CFLAGS="-g -O2 -DNETSIO"`. Tested atari800 commit: 98cc2dc.
10. **Determinism.** The river depends only on the bridge (993 LFSR steps from bridge to bridge; it comes back after 16.8 million bridges), the objects only on the last ~200 frames. So any moment is reached by running the river alone and then a few hundred whole frames; the viewer seeks that way. Details: [docs/NOTES.md](docs/NOTES.md), "Determinism and jumping". Saved states in the game use the same: screen memory is made again from the generator's state, so a saved state is about 0.5 KB.

## Environment limits
- `oracle/build.sh` builds on Linux, macOS, WSL and MSYS2 (intercepted emulator functions are renamed at compile time, no GNU `--wrap`); the C++ port builds with plain CMake (MSVC, Xcode, gcc).
- The ROM is not in the repository (`.gitignore`); put your dump at `rom/River_Raid.rom`.
- Code is indented with tabs, continuation lines too (`.editorconfig`). Documentation: `X.md` in English, `X.ru.md` in Russian, and `X.uk.md`, `X.de.md`, `X.pl.md`, `X.it.md`, `X.es.md`, `X.zh.md`, `X.ja.md`; `tools/doc_languages.py` writes the language line under each title, the picture labels are in `tools/doc_figures_i18n.py`. A change to a document goes into every language.

## Method
A rule is read from the listing, then checked against a reference trace (`tools/verify_trace.py`, then the C++ check tools). Traces alone are only used to find where to look. Changes that should not change behaviour (renaming, restructuring) are checked with `port/tools/equivalence` against an older commit: 0 differences required, plus all checks in [port/README.md](port/README.md), "Verification".

## Decisions
- The port is written in C++, not as a wrapper around an emulator.
- Verification compares the port's RAM with the reference `ram.bin` frame by frame; RANDOM values for it come from the reference `random.log`.
- No ROM data in the code: the cartridge's tables are read through named accessors (`port/src/rom.h`).
- Default colours: PAL, Altirra's model; NTSC and atari800's model on keys.
- The window shows the game's picture (364×200 (`-pal`: 364×213) with the whole scrolling line), not the whole frame with its black border (`-border`, O); scaled sharp by whole numbers, then smoothly (`-filter`, L).
- The game-over cycle race (whether the frame counter is reset before or after that frame's VBI depends on the main loop's cycles, see [port/README.md](port/README.md), Verification) is deliberately not reproduced.
- On a 60/120 Hz display the game locks to the display (60.00 instead of 59.92 frames per second; with `-pal` on a 50/100 Hz display, 50.00 instead of 49.86), sound resampled to match.

## Open questions
- Power-on itself (A000–A17C, the first 38 frames) is drawn as an empty screen; `Game::powerOn` is not exact.

## Next steps
Nothing planned.
