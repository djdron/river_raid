# River Raid C++ port

English | [Русский](README.ru.md)

So far this is the river generator and the drawing of the river as the game shows it. The generator produces the same result as the original, byte for byte; the drawing matches the original screen row for row (see Verification).

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The core and the tools need only a C++17 compiler. The viewer `river_view` needs SDL3: CMake uses an installed SDL3 if it finds one, otherwise it downloads SDL 3.2.24 from GitHub during configuration (needs git and internet) and links it statically. On Linux, building SDL from source needs the X11/Wayland development packages (see SDL's `docs/README-linux.md`). Without the viewer: `cmake -S . -B build -DRR_VIEWER=OFF`.

On Windows with Visual Studio: `cmake --build build --config Release`, the programs end up in `build\Release\`. Tested on Linux (gcc); macOS and Windows not tested yet.

## River viewer
`river_view [rom] [-game N] [-pal]` shows the generated river scrolling in a window at the game's speed (1 line per frame). The ROM is looked for in `rom/River_Raid.rom` of the current folder and up to three folders above, so running it from the repository or from `port/build/Release` works without arguments.

| Key | Action |
|---|---|
| Space | pause |
| Up / Down | faster / slower, down to backwards |
| mouse wheel, PgUp / PgDn | scroll by hand, by a screen |
| B / Shift+B | next / previous bridge |
| Home | back to the start |
| 1..8 | start game 1..8 |
| F | full screen |
| Esc | quit |

The title shows the game, the bridge number and the line. Objects stand where the generator placed them: there is no game logic yet.

## Contents
| File | What |
|---|---|
| `src/river_gen.h/.cpp` | the generator: `RiverGen::startGame(n)`, then `nextLine(line)` — one screen line per call |
| `src/river_draw.h/.cpp` | drawing a line as on screen: bank shades, roads at bridges, object sprites (`drawScreenLine`), Atari colours (`atariRgb`) |
| `src/river_state_io.cpp` | mapping of fields to the original's RAM addresses, loading and comparison (for verification) |
| `viewer/river_view.cpp` | the river viewer (SDL3), see below |
| `tools/river_map.cpp` | generates the river and saves a PNG as the game shows it: `river_map <rom> <game 1..8> <lines> out.png [-pal] [-flat]` (`-flat`: bitmap only, fixed colours) |
| `tools/render_check.cpp` | compares the drawing with the original screen (`oracle/rr_oracle -ghost -strip 20`) |
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
- The check catches injected bugs: each of four deliberate breakages (walk condition, water width, drawing, fill colour) produced differences.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -ram 0:30000
port/build/terrain_check rom/River_Raid.rom out/ram.bin 0 240 29760 5 -fresh 1 145
```

Drawing: every frame where a new line enters screen row 20, the row of the original is compared with `drawScreenLine()` of the line from the C++ generator (started from scratch).
- With the objects where the generator placed them: 96.8% of rows identical over 29 000 frames (95.3% over 120 000). The others differ where the game has moved an object.
- With object positions taken from the running game (`-ram`, slot x, flags, kind): 98.1% identical; every remaining row differs only while the tank on the road (`$38`) is active, which the game logic draws. 0 unexplained rows in 29 600.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 30001 out -ghost -strip 20 -ram 0:30000
port/build/render_check rom/River_Raid.rom out/strip.bin out/frames.csv -ram out/ram.bin
```

## Not done yet
- The code is readable but in places follows the original down to the carry flag. Next step: rewrite it more clearly while keeping the match (the check will confirm it).
- Restart after a death in the middle of a section (`sub_B1C0`: going back to the previous checkpoint, decrementing `$60`).
- The tank on the road (`$38`–`$3A`), enemy movement and explosions: game logic, not ported yet.
- `atariRgb` is an approximate NTSC palette (YIQ model), not a measured one.
