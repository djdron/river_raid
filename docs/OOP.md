# The object-oriented rewrite (branch `oop`)

English | [Русский](OOP.ru.md) | [Українська](OOP.uk.md) | [Deutsch](OOP.de.md) | [Polski](OOP.pl.md) | [Italiano](OOP.it.md) | [Español](OOP.es.md) | [中文](OOP.zh.md) | [日本語](OOP.ja.md)

Branch `oop`, from `dli_removed`. The goal: the port's game as objects with names (the river,
the plane, its bullet, the tank and its shell, the helicopter's missile, the ships, helicopters,
jets, balloons, fuel depots, bridges and explosions in their slots, the players and the panel,
the sound), instead of the original's RAM laid out by address, while the game plays exactly as
on `dli_removed`, frame by frame and bit by bit. The ZX Spectrum ports follow `dli_removed` and
are checked against it; `oop` is a rewrite of the same game, not a new one.

## The rules

- **The same game.** Every frame the same state, the same sound registers, the same picture as
  `dli_removed`. No float: positions, speeds, fuel and timers stay bytes that wrap round, with
  their fraction bytes; score, bridges and high scores stay BCD. The quirks stay too: the order
  of things in a frame, the slots and their order, an object turning into another, how many
  times RANDOM is read and when, the reset frames, the two players' swap.
- **Checked after every step** with `tools/trace/compare.sh` (below): all runs the same, or the
  step is not done.
- **One thing at a time**, each its own commit: a new type or class first wraps the state as it
  is, then the code moves into it, then the old state goes.
- **The original's RAM stays reachable**: `saveToRam` / `loadFromRam` / `diffWithRam`
  (`ram_map.h`, `game_ram.cpp`) keep the original's layout, for `game_check` and for the ZX
  ports' checks (`nxcheck`, `tscheck`), whatever the classes keep inside.

## The checks

`port/tools/game_trace` writes one line per frame of what the game does, by name: the plane,
bullet, tank, shell, missile, every slot, score, lives, the other player, high scores, the river
generator, the sound registers, the collision results and a hash of the picture (river and
panel). It plays with the autopilot of `display_bench`; without `-ghost` the plane crashes and
runs out of fuel, so lives, respawns and game overs are in the runs.

`port/tools/trace/runs.txt` lists 17 runs, 700 000 frames: the attract mode, games 1-8 on NTSC
and PAL, two players, `-wild` (steering at random, over the banks too), long `-ghost` runs from
bridges 60 and 100. `golden.txt` holds the hash of each run's lines as `dli_removed` plays them,
made at the commit in `REFERENCE`.

```
port/tools/trace/compare.sh <rom> [build dir] [runs...]
```

plays them (~2 minutes) and compares. On a difference it builds the `REFERENCE` commit in a
temporary worktree, plays the run there too and prints the first frame that differs, both
lines. `tools/fixed_check` tests the number types (below) on every input against the code they
replace.

## Done

1. `game_trace`, the runs, the reference.
2. `PokeyRandom` without its ~1 MB of tables: a jump of n cycles of the counter is linear over
   GF(2), so it is made of 17 maps (2^k steps, 1.2 KB), and the lengths the game uses are kept
   as maps read 4 bits at a time, as the ZX Spectrum ports do (5 lookups a read). Checked
   against the old class on 6 million reads.
3. The display only as geometry: the DLI's models (`displayBeam`, `displaySimple`, the
   `RR_DISPLAY` modes, `RR_GEOMCHECK`, the `Beam` class, the experiments' switches) are gone
   from this branch; they stay on `dli_removal`. `display.cpp`: 276 lines instead of 1331.
4. `src/fixed.h`: `Fixed8` (a byte and its fraction byte, add / sub with the carry), `Bcd<N>`
   (BCD counters: add with the carry up, the original's comparison), `addBcd`.
5. The bullet (`bullet.h`) and the plane (`plane.h`: steering, speed, fuel) as their own types.
6. No player-missile memory: the sprites' rows come from their state (`Game::spriteRow`: each
   shape from the ROM at its row, the missile one row, the bullet 8), shown from each frame's
   top until a reset clears them. The original's sharing of that memory (the bullet's rows
   clearing the missile's) is undone by the next frame's copy before anything reads it, so the
   game is the same. `saveToRam` still writes the memory as the original has it.
7. The tank, its shell and the helicopter's missile (`tank.h`, `tank.cpp`): one struct each,
   kept by the river (they scroll with it; from step 13 by `Objects`), with their moves
   (`Tank::drive`, `Shell::move`, `Missile::move`).
8. The objects in the slots (`objects.h`, `objects.cpp`): a class per kind of behaviour
   (`Explosion`, `Jet`, `Helicopter`, `Balloon`, `Ship`, `Bridge`, `FuelDepot`). A slot keeps
   the original's bytes; its kind is also its animation frame and changes when it is hit, so
   `objectKind(kind)` gives each frame the object of the class that knows what that kind does:
   its animation, its move (the jet across the screen; helicopters, balloons and ships to and
   fro once woken, turning round at the edge), whether the display tests it against the edge.
9. The players (`players.h`): `PlayerData` (the other player) with `Bcd<2>` bridges and
   `Bcd<3>` score, the same types for the active player, `HighScores` (the table and its check
   bytes); the panel (`panel.h`): its 60 characters with `print` (BCD digits), `showLives`,
   `markPlayers`, `showGameNumber`.
10. The sound (`sound.h`, `sound.cpp`): `Sounds` keeps what the game asks for (the start beep,
    the event sound and its time, explosion, fire, bonus, missile, the crash's falling pitch)
    and its `drive()` is the driver (B2A5-B3D8), given the plane, the pause, the attract mode
    and the shell by a `SoundContext`; `PokeyRegs` moved there too.
11. The river on the screen (`river.h`, `river.cpp`): its lines, the same without decoration,
    their runs, the top line's address in the original's screen memory and the generator's
    states for small saved states; `push()` adds the generator's next line, `rebuild()` makes
    them again when a state is loaded.
12. No `GameState`: its fields are the `Game`'s own members (`eachState()` lists them for saving,
    loading and the reset).
13. The river apart from the objects (`world.h`, `world.cpp`): `RiverGen` makes only the river
    (`RiverState`: banks, islands, sections, the LFSR); the slots, the tank, its shell and the
    missile are `Objects`, which places the new objects after each line (`afterLine`, $ABE1-
    $AE93) and tells the generator where the object being drawn in is (`BankObstacle`), so the
    bank keeps clear of it. `World` holds both and runs them line by line; its `State` is what
    the saved states and the river's line states keep.
14. The RAM export from the classes (`ram_map.h`, `ram_map.cpp`): each class lists its own bytes
    with their addresses in its `mapRam(RamMap &)` (the plane, the bullet, the sounds, the
    collisions, the players, the high scores, the panel, the scroller, the river, the slots, the
    tank, its shell and the missile, the screen's top line); `World` and `Game` put the lists
    together, and `RamMap` loads, writes and compares them. What is worked out rather than kept
    (the player-missile memory, the screen lines from the display list) stays in
    `game_ram.cpp` and `River`.
15. The documents: the READMEs' file tables and their Code, generator and verification parts,
    HANDOFF (the branch), NOTES (saved states), HOW_IT_WORKS (the collisions), in all languages;
    the source comments that still spoke of the DLI or of the river keeping the objects.

16. The end: all 17 runs the same (700 000 frames), also on a build with AddressSanitizer,
    UndefinedBehaviorSanitizer and the standard library's assertions, without a report; this
    document in every language.
17. `game.cpp` cut into files by subject, the class untouched: `game.cpp` (the frame in the
    original's order), `game_plane.cpp` (the plane, explosions, points), `game_players.cpp`
    (games and players, the panel), `game_reset.cpp` (power-on, the attract mode, a new game,
    the reset, the next plane), `game_state.cpp` (saved states); what they share is in
    `game_internal.h`.
18. The match (`match.h`, `match.cpp`): `Match` keeps the game number, whose turn it is, both
    players' bridges, score and lives, `respawned` and the high scores, with `setUp` (sub
    $AF80), `selectNext`, `swapPlayers` (given the river's state: the level and the restart
    point change hands too), `updateHighScore`, `addPoints` (A547: points, the bonus plane, the
    end past 999 999) and the panel's numbers (not shown in the attract mode, which `Game` tells
    it). Its bytes come in the order the fields had, so saved states stay the same. Found on the
    way: `World::State` had two bytes of padding, saved as whatever was in memory; they are now
    explicit zero bytes, and the same state is always saved as the same bytes.

## Next

`Game` stays what it is, the conductor of the frame; around it: the fields closed behind const
getters and one structure of what to show and play, the console's modes (`Console`: frame
counter, attract mode, pause, SELECT) where they come apart without reordering anything, and the
main loop as named steps in the same order. Each step checked with `compare.sh -quick`, all 17
runs at the end.
