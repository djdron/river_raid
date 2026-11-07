# River Raid C++ port

English | [Русский](README.ru.md)

So far this is only the river generator. It is the first part of the game ported to C++, and it produces the same result as the original, byte for byte.

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
No dependencies, only a C++17 compiler. Builds on Linux, macOS and Windows.

## Contents
| File | What |
|---|---|
| `src/river_gen.h/.cpp` | the generator: `RiverGen::startGame(n)`, then `nextLine(line)` — one screen line per call |
| `src/river_state_io.cpp` | mapping of fields to the original's RAM addresses, loading and comparison (for verification) |
| `tools/river_map.cpp` | generates the river and saves a PNG: `river_map <rom> <game 1..8> <lines> out.png [-pal]` |
| `tools/terrain_check.cpp` | comparison with a reference trace (`oracle/rr_oracle -ram A:B`) |

The generator reads tables and graphics from the user's ROM (`rom/River_Raid.rom`); there is no game data in the code.

## How the generator works
Each `nextLine()` call produces a new top line of the screen:
1. `structureAndWidth`: line counting (a block is 32 lines, a section 2 blocks, 16 sections to a bridge), LFSR step, a new width target at the start of a section;
2. `smoothIsland`: the island and the water width move smoothly toward the target;
3. `moveBank`: the bank's random walk;
4. `startDecoration`: a house or mountain on the bank;
5. `drawLine`: the 48 bytes of the line (fills, edge pieces from the table at `$B800`, a decoration row);
6. `scrollObjects` / `placeObject`: object slots shift, a new object is placed every 16 lines.

Generation depends neither on the player nor on `RANDOM`. This was checked on the reference runner: different POKEY seeds, and a bot run vs. flying without input, give the same state on every line.

## Verification
- Starting from the reference state: 119 758 lines of the whole river (120 000 frames), 0 differences in state and in bitmap lines.
- From scratch (`startGame(1)`, no emulator): after the 165 lines that fill the screen the state matches the original, then 29 757 lines without differences.
- Bot game with bridges destroyed and the flash effect: differences only in respawn frames. Those come from the restart code, not ported yet.
- The check catches injected bugs: replacing one condition of the walk gives 998 frames with differences.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

## Not done yet
- The code is readable but in places follows the original down to the carry flag. Next step: rewrite it more clearly while keeping the match (the check will confirm it).
- Restart after a death in the middle of a section (`sub_B1C0`: going back to the previous checkpoint, decrementing `$60`).
- Colours: on screen they are set per line by the DLI. `river_map` uses a fixed palette for now.
