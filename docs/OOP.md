# The object-oriented rewrite (branch `oop`)

English | [Русский](OOP.ru.md)

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
  (`game_ram.cpp`) keep the original's layout, for `game_check` and for the ZX ports' checks
  (`nxcheck`, `tscheck`), whatever the classes keep inside.

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
   kept by the river (they scroll with it), with their moves (`Tank::drive`, `Shell::move`,
   `Missile::move`).
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

## Next

10. The sound driver.
11. The river: the generator, screen lines, runs.
12. `GameState` and `RiverState` gone, the RAM export written from the classes; the documents.
