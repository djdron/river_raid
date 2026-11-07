# River Raid (Atari 8-bit) → C++ port: project state

English | [Русский](HANDOFF.ru.md)

This file is for picking the work up on any computer or in a new chat. Start a new session with "read HANDOFF.md and docs/NOTES.md".

## Goal
A C++ port of River Raid (Activision, Atari 800, 1983, by Carol Shaw) for Windows, Linux and macOS:
- game logic identical to the original, readable by an ordinary programmer without 6502 knowledge;
- identical sound: the same sound driver plus a POKEY core;
- a cross-platform layer (SDL3 planned);
- no original assets are distributed; the port reads the user's ROM.

## What you need
- The whole project folder.
- Your own ROM `River_Raid.rom`: 8192 bytes, no header, loads at $A000–$BFFF. It is not in the repository; put it at `rom/River_Raid.rom`.

## Layout
| Path | What |
|---|---|
| `disasm/river_raid_a8.asm` | full listing with a header (entry points, frame, variables) |
| `tools/dis6502.py` | disassembler: `python dis6502.py <rom> <out.asm> B500 B2A5 B485` |
| `tools/check_lfsr.py` | checks the level LFSR model against a trace |
| `tools/verify_trace.py` | re-checks every documented rule against a reference trace |
| `oracle/` | reference runner on libatari800: `rr_oracle.c`, `build.sh`, `README.md`, input scripts, `-bot` autopilot |
| `oracle/scripts/bot_run_8000.txt` | long recorded run (2 bridges, 5 deaths, game over) |
| `port/` | C++ port: so far the river generator (`river_gen`) and drawing (`river_draw`), attract-mode viewer `river_view` (SDL3) with the status panel (`status_bar`), tools `river_map`, `terrain_check`, `render_check`, see `port/README.md` |
| `docs/NOTES.md` | all findings with addresses |
| `docs/oracle_demo.png` | frames from a reference run |

## Established facts (all verified)
1. **Handlers.** Main loop A000–AF5A, one pass = one frame (synchronised on VCOUNT $50/$78). VBI $B2A5 = POKEY sound driver and counters. DLI $B500 = per-line sprite multiplexing. IRQ $B485 = stub.
2. **Level generator.** Software 24-bit LFSR in `sub_B183` on $AA/$AB/$AC; start value $E3/$D4/$F7 is kept in $66–$68 and restored at start and after a death. Model checked against a trace: 265 steps + 3 reloads out of 268 changes, no mismatches. The level is always the same.
3. **RANDOM ($D20A).** Read by game logic only at A205: every 16 frames a random enemy slot (0–10) gets bit $40 in `$42,X`. B362 and B3D0 use it for sound noise. With different seeds over 1000 frames only $42–$4C, $07 and the stack differ.
4. **Modes.** PAL/NTSC is detected from $D014 and kept in $BF. The font is copied from the OS ROM ($E000/$E100/$E300); the reference runner uses the built-in AltirraOS.
5. **Variables.** Table with status in `docs/NOTES.md`, section "Variables". Checked by script: plane X $57, fuel $5B:$5C (burn $20 per frame regardless of speed, +2 to $5B per frame over a depot, capped at $FFFF, reset on respawn), score $63–$65 (BCD ÷10) from the points table $BB0B[type], object types $050B+X, lives $6A, bridge number $61/$62.
6. **Terrain.** The bank is a random walk of `$52` (direction `$C3`) driven by the LFSR nibble `$AC & 15`, with limits `$C4/$C5` and avoidance of objects; the island `$54` and water width `$53` move smoothly toward the target `$34`. Both models are checked on every line of the traces (`verify_trace.py`); the rest of the generator (bridge approach, corridor `$052D`, line drawing, objects) is verified through C++, see item 7. Details: `docs/NOTES.md`, "Terrain generation".
7. **C++ generator.** `port/src/river_gen.cpp` reproduces the river generation byte for byte: 119 758 lines with no differences; a start from scratch matches too. Generation depends neither on the player nor on `RANDOM`. The game number selects a starting point on the same river (bridges 1/5/20/50). The river differs slightly between PAL and NTSC (`$CB`).
8. **Reference runner.** atari800 seeds POKEY from the clock; `rr_oracle -seed` (default 0) makes runs byte-identical. The game starts with the START key. The atari800 build fails with `-Werror` on a ULONG redefinition; `build.sh` builds with `CFLAGS="-g -O2 -DNETSIO"`. Tested atari800 commit: 98cc2dc.

## Environment limits
- `oracle/build.sh` builds on Linux, macOS, WSL and MSYS2 (intercepted emulator functions are renamed at compile time, no GNU `--wrap`). The reference runner currently runs in Claude's cloud session.
- The ROM is not in the repository (`.gitignore`); put your dump at `rom/River_Raid.rom`.
- On Windows the reference runner builds only under WSL or MSYS2; the C++ part (`port/`) builds with plain CMake + MSVC.
- Code is indented with tabs (`.editorconfig`). Documentation is bilingual: `X.md` in English, `X.ru.md` in Russian.

## Method
A rule is read from the listing, then checked against a reference trace with `tools/verify_trace.py`. Traces alone are only used to find where to look.

## Decisions
- The port is written in C++, not as a wrapper around an emulator.
- Verification compares the port's RAM with the reference `ram.bin` frame by frame.
- In verification mode the gameplay RANDOM values come from the reference `random.log` (scanlines other than ~249). In normal play the port uses its own RNG with the same distribution (0–15, values ≥11 minus 5).

## Open questions
- Slot flags $42+X; full list of states $0500+X; what $0C+X is exactly (vertical?); $50, $60, $24, $29.
- What does bit $40 do, and when does enemy movement switch on ($60 ≥ 2, check at A297)?
- Collision checks $1B/$1E/$21/$50 at A424–A434.
- Purpose of tables $BAAF, $BAC5, $BB24 and the copies at $1000/$06C0/$0818/$08D0/$07D0.

## Next steps
1. Make the generator code more readable while keeping it byte-exact (`terrain_check`).
2. The game is complete and playable (`port/game/river_raid.cpp`), checked frame by frame against bot and two-player runs; the DLI runs with cycle counts; remaining: the game-over timing race in `port/README.md` (would need a cycle count of the main loop), the exact power-on sequence (`Game::powerOn`), PAL.
3. Test the game on Windows and macOS (only built and run headless on Linux so far); then make the game code more readable while keeping `game_check` at zero differences.
