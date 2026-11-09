# River Raid C++ port

English | [Русский](README.ru.md) | [Українська](README.uk.md) | [Deutsch](README.de.md) | [Polski](README.pl.md) | [Italiano](README.it.md) | [Español](README.es.md) | [中文](README.zh.md) | [日本語](README.ja.md)

An unofficial fan project for educational purposes, not affiliated with Activision or Microsoft; no part of the game is included (see the disclaimer in `../README.md`).

The whole game: `river_raid` plays River Raid as the original does — attract mode, game selection, one and two players, the plane, fuel, enemies, bridges, score, lives, high scores, pause and sound. The river generator and the game logic produce the same RAM state as the original frame by frame, the drawing the same screen (see Verification).

## Build
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```
The core and the tools need only a C++17 compiler. The game `river_raid` and the viewer `river_view` need SDL3: CMake uses an installed SDL3 (3.4 or newer) if it finds one, otherwise it downloads SDL 3.4.20 from GitHub during configuration (needs git and internet) and links it statically. On Linux, building SDL from source needs the X11/Wayland development packages (see SDL's `docs/README-linux.md`). SDL 3.4 stops with an error when such a package is missing and names the option that builds without it (for example `-DSDL_X11_XCURSOR=OFF`). Without SDL at all: `cmake -S . -B build -DRR_VIEWER=OFF`.

With SDL2 instead of SDL3 (for platforms SDL3 no longer supports, such as UWP on the Xbox): `cmake -S . -B build -DRR_SDL2=ON`. CMake then uses an installed SDL2 (2.0.18 or newer) or downloads SDL 2.32.10. The programs talk to SDL only through `sdl/host.h`, implemented once for each version; both give the same picture.

On Windows with Visual Studio: `cmake --build build --config Release`, the programs end up in `build\Release\`. Built and played on Windows (MSVC), macOS (Xcode, Apple M2) and Linux (gcc).

## The game
```
river_raid [rom] [options]
```
Without arguments it finds the ROM by itself (see `rom`) and starts like the cartridge: attract mode, then START.

| Option | What it does |
|---|---|
| `rom` | the cartridge image (8192 bytes). By default `rom/River_Raid.rom` is looked for in the current folder and up to three folders above, so it starts without arguments from the repository or from `port/build/Release` |
| `-os file` | letters and digits from Atari's own OS: an OS ROM image (10 KB or 16 KB) or a 1 KB font. Without it the built-in AltirraOS font is used |
| `-seed N` | another river, with other enemies and depots. N is the state the river generator starts from, 1 to 16777215. Game 1 starts at the beginning of that river, games 3-8 at its bridges 5, 20 and 50; each game keeps its usual level, so the difficulty grows as usual. The cartridge's own river is `-seed 16241891` |
| `-bridge N` | every game (1-8) starts at bridge N, 1 to 9999: with N on the bridge counter and that bridge's level (after 80 the level alternates 79/80, as in the original). Works with `-seed` too. The start is found by running the river generator up to it (under a second even for 9999) |
| `-random N` | the starting value of POKEY's RANDOM counter, which decides which enemy wakes up when and the noise of explosions. By default it is taken from the clock, as the real machine's depends on the moment of power-on, so every run differs; with the same N and the same input a run repeats exactly |
| `-pal` | play as a PAL machine: as on a real PAL Atari, fuel depots keep getting rarer up to level 47 instead of 46; PAL colours. The display timing stays NTSC (see Not done yet) |
| `-colours ntsc\|pal` | the colours of an NTSC or a PAL machine; PAL by default. C switches while playing |
| `-palette altirra\|atari800` | the colour model: `altirra` (default) or `atari800` (darker middle tones). V switches while playing |
| `-border` | shows the whole frame with its black border (384×240 less 10 columns at each side), as emulators do. By default the window shows only the game's picture with the whole scrolling line, 364×200, so in full screen it is about 20% larger, and a window is that much lower. O switches while playing |
| `-filter sharp\|nearest\|linear` | how the picture is scaled to the window or screen: `sharp` (default) scales by whole numbers with sharp pixels and then smoothly to the final size, so it stays sharp and every pixel is the same size; `nearest` is plain blocks, but at a scale that is not a whole number (always so across, as Atari pixels are not square) some columns come out one screen pixel wider; `linear` is smooth and blurred. L switches while playing |
| `-stats` | prints the renderer and display rate, then once a second the frame rate, time per frame and the sound queue |
| `-nosound` | no sound |

The window title shows the colours and the scaling in use. On a display at 60 Hz (or 120 Hz...) the game runs at exactly the display's rate (60 instead of the Atari's 59.92 frames per second), so every frame stays on screen equally long; on other displays at the Atari's rate.

Examples: `river_raid -bridge 30` starts at bridge 30; `river_raid -seed 12345` is a different river; `river_raid -seed 12345 -bridge 80` is that river at its hardest.

After power-on the attract mode flies over the river. START begins a game; SELECT (or OPTION) chooses games 1–8: even numbers are two-player games (the players take turns), games 3–4, 5–6 and 7–8 start at bridges 5, 20 and 50. Any key of the Atari keyboard pauses. After a game over the screen stays until the attract mode starts again (about 35 s). The game and the viewer quit the usual way: closing the window, Alt+F4, Cmd+Q.

| Key | Atari |
|---|---|
| arrows or W A S D | joystick: left/right steer, up faster, down slower; any direction launches the plane at the bridge |
| Left/Right Ctrl, Z, X | fire |
| F4 or Enter | START |
| F3 | SELECT |
| F2 | OPTION |
| Space | a key of the keyboard: pause |
| F, F11 or Alt+Enter | full screen |
| C | NTSC / PAL colours |
| V | Altirra / atari800 colour model |
| O | the game's picture only / the whole frame with its border |
| L | scaling: sharp / nearest / linear |
| Esc | leave full screen |
| mouse click | hides the mouse pointer until the mouse moves (so does going full screen) |

The keyboard drives both joysticks, so one person can play a two-player game too. Gamepads: the first is joystick 1 (and 2 while there is no second), the second is joystick 2; D-pad or left stick, A/B/X/Y or the right trigger to fire, Start = START, Back = SELECT.

## River viewer
```
river_view [rom] [options]
```
It shows the original's attract mode, the flight over the river it plays when nobody presses START: after power-on the screen fills with the river of game 1, which then scrolls 1 line per frame; ships and helicopters wake up and move, tanks drive to the bridges and fire, helicopters fire missiles (the ported game logic, `src/game.cpp`); below are the status panel and the scrolling line with the credits, "Press START key" and the rainbow Activision logo. The options are those of the game: `rom`, `-os`, `-seed`, `-bridge` (the attract mode flies from that bridge), `-random`, `-pal`, `-colours`, `-palette`, `-border`, `-filter`.

Letters and digits 1–9 come from the OS font, which is not in the cartridge. The port has the font of AltirraOS built in: the free replacement OS that ships with atari800 (licence: copying and modification permitted with the copyright notice kept, see `src/os_font.cpp`); the reference traces were made with it too. `-os file` takes the font of Atari's own OS from an OS ROM image (10 KB or 16 KB) or a 1 KB font file.

| Key | Action |
|---|---|
| Space | pause |
| Up / Down | faster / slower, through a stop to flying backwards (x1/4 to x16 either way) |
| mouse wheel, PgUp / PgDn | scroll along the river by hand, by a screen |
| Backspace | back to the newest line at normal speed |
| B / Shift+B | next / previous bridge |
| Home | start again |
| F1 / F2 / F3 | sound test: explosion / firing / score sound through the game's sound driver (the attract mode itself is silent, as in the original) |
| F | full screen |
| C | NTSC / PAL colours |
| V | Altirra / atari800 colour model |
| O | the game's picture only / the whole frame with its border |
| L | scaling: sharp / nearest / linear |
| Esc | leave full screen |
| mouse click | hides the mouse pointer until the mouse moves (so does going full screen) |

Compared with the original's screens (attract mode, frames 1000–2300, with the original's RANDOM values): the river area and the status panel match pixel for pixel, the scrolling line too apart from its outer columns (left of 24, right of 359), where the emulator handles fine scrolling at the edge differently.

Which object wakes up when is decided by POKEY's RANDOM register, a hardware counter read at a moment that depends on the whole machine's timing. The port models the counter (`src/pokey_random.h`) and starts it from the clock, as the real machine starts it at power-on, so each run differs, like on the real machine; `-random N` repeats a run.

## Contents
| File | What |
|---|---|
| `src/rom.h` | the cartridge image and named accessors for every table the port reads from it (shapes, colours, points, generator tables, panel text...); object types |
| `src/river_gen.h/.cpp` | the generator: `RiverGen::startGame(n)`, then `nextLine(line)` — one screen line per call |
| `src/river_draw.h/.cpp` | drawing a line as on screen: bank shades, roads at bridges, object sprites (`drawScreenLine`) |
| `src/palette.h/.cpp` | Atari colours of NTSC and PAL machines (`atariRgb`): the models of Altirra and atari800 |
| `src/game.h/.cpp` | the game: the original's main loop and VBI frame by frame — attract mode, console keys, the plane, fuel, collisions, explosions, score, lives, players, death and restart, game over, high scores, sound driver |
| `src/display.cpp` | the river display as the DLI drives it: one sprite re-used for every object, collisions per object, drawing of the river area |
| `src/pokey_random.h` | POKEY's RANDOM counter |
| `src/pokey_sound.h/.cpp` | POKEY sound: four channels, dividers and noise counters, generated cycle by cycle |
| `src/status_bar.h/.cpp` | status panel and scrolling line: character sets, panel, scroller step, drawing |
| `src/os_font.h/.cpp` | OS font: AltirraOS built in, or loaded from an OS ROM |
| `src/river_state_io.cpp`, `src/game_ram.cpp` | mapping of the generator's and the game's fields to the original's RAM addresses: loading, writing and comparison (for verification) |
| `game/river_raid.cpp` | the game (SDL3): picture, sound, keyboard and gamepads |
| `viewer/river_view.cpp` | the river viewer (SDL3), see below |
| `sdl/host.h`, `host_sdl3.cpp`, `host_sdl2.cpp` | window, picture, sound, keyboard and gamepads for the game and the viewer, over SDL3 or SDL2; hiding the mouse pointer |
| `tools/river_map.cpp` | generates the river and saves a PNG as the game shows it: `river_map <rom> <game 1..8> <lines> out.png [-pal] [-flat]` (`-flat`: bitmap only, fixed colours) |
| `tools/river_dump.cpp` | writes the generated river as text (colour of every dot, objects placed) for `tools/doc_figures.py`, which draws the pictures in `docs/HOW_IT_WORKS.md`: `python tools/doc_figures.py rom/River_Raid.rom` (needs matplotlib) |
| `tools/sound_check.cpp` | compares the sound driver with the original's POKEY writes (`rr_oracle -vbi`) |
| `tools/sound_render.cpp` | renders a POKEY write log to WAV with the port's POKEY, to compare with the emulator's sound (`rr_oracle -wav`) |
| `tools/game_check.cpp` | compares the game with a RAM trace of the original frame by frame, with the original's input and RANDOM values (and the river area with its screens) |
| `tools/status_check.cpp` | compares the panel and scrolling line with full screens of the original (`rr_oracle -screens A:B -ram A:B`) |
| `tools/render_check.cpp` | compares the drawing with the original screen (`oracle/rr_oracle -ghost -strip 20`) |
| `tools/terrain_check.cpp` | comparison with a reference trace (`oracle/rr_oracle -ram A:B`) |
| `tools/equivalence/` | refactoring check: an older commit of the port against the working tree, frame by frame (see Code) |

The generator reads tables and graphics from the user's ROM (`rom/River_Raid.rom`); there is no game data in the code.

## Code
The port is written to be read without the original's listing at hand:
- state fields, functions and constants are named by what they mean (`planeState`, `shellTimer`, `enteringSlot`, `positionObject`, `kFacingLeft`); each field's comment gives the original's RAM address and each function the code addresses it comes from, so the listing (`disasm/`) and the traces can still be followed;
- the cartridge's tables are read through named accessors in `src/rom.h` (`rom.points(type)`, `rom.objectHeight(type)`, `rom.shellRise(t)`...), not by address;
- values stay 8-bit with wrap-around where the original's do (`lo()`), because positions wrap round the screen; where the original carries the 6502's carry flag from one sum into the next, `addWithCarry`/`subtractWithBorrow` say so explicitly;
- the display list interrupt (`display.cpp`) is the one part that stays close to the instructions: its timing within the scanline decides which collisions are seen, so it is executed with cycle counts.

Changes that should not change behaviour are checked with `tools/equivalence`: it builds an older commit (from 06ffc73 on) next to the working tree and runs both on 150 000 river lines of every game (NTSC and PAL), on random play from power-on and from 194 states of the original's traces, comparing the whole state, the screen and the sound registers after every step. The renaming and restructuring of the generator, the game and the display were checked this way (0 differences), as well as with all the checks below.

```
port/tools/equivalence/build.sh 06ffc73
port/build/eq/eq
```

## How the generator works
Each `nextLine()` call produces a new top line of the screen:
1. `countLines`: line counting (a block is 32 lines, a section 2 blocks, 16 sections to a bridge), LFSR step, a new width target at the start of a section (`chooseSectionWidth`: islands open and close);
2. `changeIsland`: the island and the water width move smoothly toward the target;
3. `moveBank`: the bank's random walk, turning round at its limits and before the new object (`bankWouldHitObject`);
4. `startDecoration`: a house or mountain on the bank;
5. `drawLine`: the 48 bytes of the line (fills, edge pieces from the table at `$B800`, a decoration row);
6. `scrollObjects` / `placeObject`: object slots shift, a new object is placed every 16 lines (`chooseEnemy`, `positionObject`, `placeTank`).

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
Status panel and scrolling line: the character sets built from the OS font and the cartridge equal the original's RAM, the scroller state ($3FB3/$26/$27) matches every frame, and the drawn rows 180–239 are identical in all 1299 frames, except the outer columns of the scrolling line (see River viewer).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 2300 out -screens 1000:2299 -ram 1000:2299
port/build/status_check rom/River_Raid.rom out/screens.bin out/ram.bin
```
Game frame in the attract mode (`game_check`): started from the power-on state (`Game::startAttract`) and given the original's RANDOM values, the port runs 19 900 frames (up to bridge 20) with no difference from the original's RAM in any compared byte: generator state, object slots (position, direction, kind, collision bit), tank, its shell, helicopter missile, collision results, player-missile memory, the river lines on screen. The river area drawn from it equals the original's screens in all 1299 compared frames.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/attract_idle.txt 20000 out -strip 20
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 19900 -attract
```

The game with a player (`game_check` on runs with input). From the attract mode's state the port is given the original's input and RANDOM values and runs on its own, never reloaded; every frame its RAM is compared with the original's (generator, slots, plane, fuel, bullet, collisions, score, lives, both players, high scores and their check bytes, panel, scrolling line, player-missile memory, screen lines):
- one-player bot game (`start_only.txt -bot -seed 3`, 8 950 frames: attract mode, START, 4 planes, bridges, deaths and restarts, game over, attract mode again): identical in every compared frame;
- two-player game with SELECT and the pause key (`two_players.txt`, up to the game over at frame 3 007): identical up to the game over (one frame, see below);
- the river area drawn by the port (plane, bullet, enemies, explosions, tank, shells) equals the original's screens in all 1 565 compared frames of a bot game.

Where the original's result depends on CPU cycles:
- The DLI (`display.cpp`) runs with cycle counts, as the reference emulator times a scanline: player/missile and display list fetches at the start, the CPU up to cycle 28, then the whole line is drawn (collisions included), then screen data fetches, WSYNC at cycle 106. So it is known whether a collision register read already sees the current scanline: a bullet touching an object only on the scanline of the read counts on a bridge (that path reads after cycle 28) and not elsewhere, as in the original.
- The RAM snapshot is taken at the end of the frame; normally the original's main loop is then waiting for the next frame (AE99–AE9E). Right after the start of a game, while the river is refilled at 2 lines per frame, it is sometimes still drawing the second line (1 frame per refill). `game_check` reports such frames separately and does not compare them; the next frame is identical again.
- Not modelled: the game over. The main loop's game logic starts at scanline 160 and shares the CPU with the DLI and the VBI (scanline 248). In the game-over frame the high-score check and its check bytes (sub $B261, $B298) take thousands of cycles, so `sub_B21A` sets the frame counter to $FF before the VBI, after it, or (two-player game) only in the next frame, depending on the cycles of everything before it in that frame. The port does it before the VBI; then the frame counter can differ by one, which shifts the following attract mode by a frame. Reproducing it would need the cycle count of the whole main loop for that frame (including the OS interrupt handler).

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 9000 out -bot -seed 3
port/build/game_check rom/River_Raid.rom out/ram.bin out/random.log 38 8950 -attract
```

Sound. The sound driver (VBI B2A5–B3D8, `Game::soundDriver`) was run on the RAM the original had at the start of each VBI of a bot game (2989 frames, 2199 with sound: engine, low-fuel warning, explosions, firing, score, tank shell): every AUDF/AUDC value equals what the original wrote. The POKEY model (`src/pokey_sound.cpp`) was compared with atari800's sound for the same register writes: the same loudness over time (envelope correlation 0.95), the same pure tones at the same pitch, noise with the same spectrum shape (smoothed spectra cosine 0.94); the noise's sample-by-sample pattern differs, as it depends on where the noise counters happen to be.

```
oracle/rr_oracle rom/River_Raid.rom oracle/scripts/start_only.txt 3000 out -bot -wav -vbi
port/build/sound_check rom/River_Raid.rom out/vbi.bin out/pokey.log out/random.log
port/build/sound_render out/pokey.log port.wav
```

## Not done yet
- Power-on itself (A000–A17C: the first 38 frames before the attract mode) is drawn as an empty screen; `Game::powerOn` is not exact yet.
- The PAL machine (`-pal`): the generator knows it, the display list and timing do not.
- The colours follow the emulators' models of the GTIA output (Altirra by default, atari800 on V; both checked byte for byte against the emulators' own palette code with their default settings, atari800 stretched from its video levels 16–235 to 0–255), not a palette measured on a real TV; how bright it looks depends on the TV's settings as much as on the machine.
