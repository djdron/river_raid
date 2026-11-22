# Experiments: the game without the DLI model

English | [Русский](DLI_REMOVAL.ru.md) | [Українська](DLI_REMOVAL.uk.md) | [Deutsch](DLI_REMOVAL.de.md) | [Polski](DLI_REMOVAL.pl.md) | [Italiano](DLI_REMOVAL.it.md) | [Español](DLI_REMOVAL.es.md) | [中文](DLI_REMOVAL.zh.md)

Branches `dli_removal` and `dli_removed`. The goal is a simpler display for ports to slow
machines (the ZX Spectrum Next, TS-Conf): sprites drawn as ready pictures at their positions,
collisions worked out per object, no scanline-by-scanline model of the display list interrupt.
On `dli_removal` the game stays the same as the original's frame by frame, and each step is
checked against the original (`rr_oracle` on libatari800, `game_check`). `dli_removed` goes one
step further: no DLI at all, the collisions as plain geometry, which differs from the original
on a few frames in a thousand (the last part, "Branch `dli_removed`"). The ZX ports
([river_raid_zx](https://github.com/djdron/river_raid_zx)) follow `dli_removed`.

## Step 1: collisions with the bank on lines without decoration

The DLI reads the playfield collisions of: the plane (P2PF, the bank colour only), the objects
(P3PF, the edge colour only: ships and helicopters turn on it), the tank (P1PF, the edge), the
bullet and the helicopter's missile (M2PF, M0PF: bank or edge). Houses and roads (the
decoration, `decoRow`) are drawn in the edge colour and the background colour inside the bank.

`RiverGen::nextLine(line, bank)` gives a second line, the same without its decoration (the
bank fill where it is drawn); `Game::bankLines` keeps them next to `screen`, and the
collisions look at them (`bankRuns`, on by default; `RR_BANK_RUNS=0` the screen as before).
Without decoration every line of the river is one of four shapes (480 000 lines of games 1-8):

| shape (runs of colour) | lines | boundaries |
|---|---|---|
| bank, edge, water, edge, bank | 398 644 | 4 |
| the same with an island | 80 324 | 8 |
| with a narrow island (edge colour only) | 1 016 | 6 |
| a hit bridge's lines (the bank in the edge colour) | 16 | 2 |

So a line is described by 2-8 numbers.

Checked with `game_check` against runs of the original: the attract mode (19 900 frames), a
bot game (8 950), a PAL bot game (5 974), game 7 from bridge 50 on NTSC (2 runs, up to the
game over) and PAL (8 950 frames): no difference, apart from one bit nothing reads. Jets fly
over the land and touch the decoration: their slot's "touching the edge" bit differs (in the
attract mode on 341 frames), but moveObjects never reads it for a jet. `game_check` does not
compare that bit for jets when `bankRuns` is on. `RR_BANK_DEBUG=1` prints each line where the
two kinds of line give different collisions (all were jets, kind 7, player 3).

Runs of game 7 from power-on: `oracle/scripts/start_game7.txt` with `-bot`. On NTSC the
comparison stops at the first game over, which the port does not model (README).

## Step 2: collisions by spans

`collideSpans` (the default; `RR_COLLIDE=masks` for the pixel masks of the whole line as
before): each sprite on the scanline is its graphics bits and the colour clocks from its first
to its last set bit. Against the playfield it is tested run by run (the line without
decoration, 5-9 runs of one colour); a run that reaches past the first or last set bit is hit,
one strictly between them is looked at bit by bit. Two sprites: their spans first, bit by bit
only where they meet. Still per scanline, inside the beam: what each line holds and when the
DLI reads the registers is the beam's.

The same runs as above: no difference from the original. `RR_STATS=1` counts the cases;
`RR_ROUGH_BANK=1` and `RR_ROUGH_PAIRS=1` drop the bit-by-bit step (a hit wherever the spans
meet), to see whether it is needed:

| run | lines with sprites | a run between the first and last bit (in a hole) | spans met (not the bits) |
|---|---|---|---|
| attract mode | 1 164 849 | 86 228 (0) | 26 862 (70) |
| PAL bot | 278 257 | 9 201 (96) | 4 108 (0) |
| game 7, PAL | 536 802 | 24 494 (96) | 7 693 (0) |

- The bank: spans are enough. With `RR_ROUGH_BANK` all runs stay identical to the original;
  the runs that fell into a hole never changed a bit the DLI reads.
- Sprites with each other: the bits are needed. With `RR_ROUGH_PAIRS` the bullet and the
  tank's shell "hit" objects through their holes: the attract mode differs on 3 frames, the
  NTSC bot run on 712 (an object destroyed that the original's bullet passed), game 7 on 2-4.
  Two sprites' spans meet rarely (a few thousand lines in a run), so the bit-by-bit step costs
  little.

## Step 3: the display without the DLI's cycles

`Game::displaySimple` (`port/src/display.cpp`) is now the default; `RR_DISPLAY=beam` runs
the cycle-by-cycle model as before, `RR_DISPLAY=check` runs both from the same state every
frame and prints any difference in the collision results, the objects' flags, player-missile
memory or what is drawn on each scanline.

It follows the DLI's loop (B565-B664) in scanlines, not cycles. The 6502's X register counts
scanlines: every STA WSYNC is followed by one INX, so while X = c the scanline being drawn
is c - 69. For each slot:

| what | scanline (S: X when the object's wait ends, E: X after its last row) |
|---|---|
| HPOSP3 = x | from S - 68 |
| two blank rows in player 3 memory | S - 67, S - 66 |
| SIZEP3 | from S - 66 |
| shape row k (from the top row down), its COLPM3, the bridge's road colour | S - 65 + k |
| the bank colour back (bridge) | from E - 68 |
| read M2PL (bullet - object) | sees up to E - 70 (a bridge: E - 69) |
| read P3PL (object - shell), P2PL (plane - shell, object) | up to E - 69 |
| read P3PF (the object's "touching the edge" bit) | up to E - 68 |
| read M0PL (missile - plane), M0PF (missile - bank) | up to E - 68 (M0PF: see below) |
| read P1PF (tank), P2PF (plane - bank), M2PF (bullet - bank) | up to E - 67 |
| HITCLR | the scanlines from E - 66 on count for the next object (see below) |

An object whose rows are all above the scanline where its wait ends (it started too close
under the one before) is skipped with no reads and no HITCLR. When X wraps to 0 the river
ends; the panel part reads the collisions of the object in progress up to scanlines 188-190.

Where the CPU is after a WSYNC (cycle 106) is fixed by the code, so every access falls on a
known side of a scanline's draw (cycle 28) except two, decided by the cycles the next
scanline's start takes (DMA, a display list instruction with or without an address) and by
the hits stored just before them:

- M0PF (B631) is read 28 cycles after the WSYNC, 30 when M0PL was a hit: after the draw on
  a line whose start takes 8 cycles (with an address), so it sees one scanline more. On the
  NTSC bot run: 1 499 times in 68 365.
- HITCLR (B65F) comes 23 cycles after the WSYNC, 2 more for each of the three hits stored
  before it: with all three, on a line with an address, it comes after the draw and that
  scanline's collisions are lost. Never seen in the runs (0 of 285 623).

Collisions: each sprite's box (the scanlines it has graphics on and the clocks they cover;
player 3 a box per object, cut where its position or size changes). The playfield: each row
of player 1 (tank), 2 (plane), 3 (objects) and missiles 0 and 2 against the runs of its
scanline (step 2). The sprites with each other: only the pairs the DLI reads (bullet - object,
object - shell, plane - shell, plane - object, missile - plane) whose boxes meet, bit by bit
on the scanlines they share. What a read sees is the OR of the scanlines since the last
HITCLR.

Checked: `RR_DISPLAY=check` on all the runs above (attract mode, NTSC and PAL bots, game 7 on
NTSC and PAL, two players up to frame 2900): no difference from the beam in any frame.
`RR_DISPLAY=simple` against the original (`game_check`): the same results as the beam (no
differences, game 7 on NTSC up to its game over). The river area drawn from it against the
original's screens (`-screens`, 1289 frames of the attract mode, 1272 of game 7 on PAL from
bridge 50): identical.

### Speed (C++)

`RR_TIME=1` prints the time `display()` takes a frame. Release build, best of 5 runs, and
the instructions counted by callgrind (game 7 on PAL, 2000 frames):

| | attract mode | game 7, PAL (from bridge 50) | instructions a frame |
|---|---|---|---|
| the beam (`RR_DISPLAY=beam`) | 8.5 us | 10.1 us | 101 500 |
| displaySimple, boxes found in player-missile memory (`RR_BOXES=scan`) | 4.0 us | 4.0 us | 40 600 |
| displaySimple, boxes from the game's positions | 3.1 us | 3.7 us | 34 200 |

About 3 times fewer instructions, 2.5-3 times faster. Both look at the playfield's runs
made once as each line enters (`Game::runLines`, `makeRuns`), not at the line's pixels. Of
the beam's time two thirds went to the collisions it works out on every scanline, one third
to its cycles (`settle`). The sprites' boxes come from where the game put them (the default; `RR_BOXES=scan` looks
down player-missile memory as before): the shell 16 rows from `shellY`, the tank 11 from
`tankY`, the plane 14 from row $AA, the helicopter's missile the row `missileY`, the bullet 8
rows below `bulletY` (from row $1C), each object's rows as the DLI's loop wrote them; the bits
of each scanline in a box are still read from player-missile memory. Same as the beam on
every frame of every run: nothing outside these boxes (rows left in player 3's memory from
earlier frames, say) ever made a collision the DLI reads.

### Long runs: `display_bench`

`port/build/display_bench <rom> <frames> [-pal] [-game N] [-seed N]` plays with an
autopilot (it steers to the middle of the water ahead and keeps firing; the plane cannot crash
and never runs out of fuel, so one game goes on for the whole run: 100 000 frames reach bridge
98 in game 1, bridge 147 in game 7). With `RR_DISPLAY=check` it compares the two models on
every frame, with `RR_TIME=1` it times `display()`.

These runs found what the shorter ones never showed: the DLI sometimes falls a scanline
behind. From one WSYNC to the next the code must fit before cycle 106 of the next scanline
with that scanline's start and draw taken out (on a river line 6 or 8 + 57 cycles); three
stretches can be longer:

- B5B0-B5D1 (to the first row): 49 cycles, 50 when the first row's shape byte is on the next
  page: too long on a line with a screen address.
- the last row of a bridge to B5F3: 46 cycles, +1 for each table read across a page, +2 for
  a bullet hit.
- B5F3-B61F: 44 cycles (bridge 43), +2 for each of the hits stored there (shell, plane with
  shell, plane with object).

When it is too long, the STA WSYNC waits for the end of the next scanline: everything after
it in that frame happens a scanline later (X = scanline + 68), the river's end and the panel's
reads included, and a row of player 3 written on or after the scanline it is for shows there
only in the next frame (GTIA has fetched the old one). displaySimple counts these skips
(`RR_STATS`): 0-10 in 100 000 frames, all with the plane hitting things it would have crashed
on; in a real game once in a long while. With them modelled, `RR_DISPLAY=check` gives no
difference in 6 runs of 100 000 frames (games 1, 2, 3, 5, 7, 8, NTSC and PAL).

Time of `display()`, best of 3, and instructions a frame (callgrind, 5000 frames):

| | game 1, NTSC, 50 000 / 100 000 frames | game 7, PAL, 50 000 / 100 000 frames | instructions (game 1 / game 7) |
|---|---|---|---|
| the beam | 10.8 / 11.1 us | 10.5 / 9.5 us | 120 000 / 129 000 |
| displaySimple | 5.1 / 4.8 us | 4.8 / 5.0 us | 65 000 / 69 000 |

About twice as fast. Less than on the runs of the original above (3 times) because here the
plane flies through objects and banks without crashing: far more collisions to work out a
frame than a real game has.

Longer still: 12 runs of 300 000 frames (3.6 million), `-bridge 100` (the game starts at
bridge 100; up to bridge 392), NTSC and PAL, games 1, 2 (two players) and 7, and with
`-wild` (the autopilot steers at random, over the banks too: up to bridge 70-159). No
difference in any frame. Scanlines skipped: 0-8 a run. The late HITCLR (all three hits stored
before it, on a line with a screen address) never happened, in these runs or any other: it
is there from the code's cycles alone, not seen at work.

### Only the objects that turn at the edge

The "touching the edge" bit (P3PF) is read by moveObjects only for helicopters ($08-$0B),
balloons ($0C) and ships ($0D): they turn round at the first contact. Jets fly straight,
explosions stand still, bridges and depots stay put. `displaySimple` tests only those kinds'
rows against the edge (`noEdge` marks the others' scanlines); the others' bit stays clear, and
`game_check` does not compare it for them (`bankRuns`). Checked against the original: the
attract mode (NTSC 2 262 and 5 961 frames, PAL 2 975), bot games on NTSC (up to their first
game over, which the port does not model on NTSC), on PAL (5 975 and two players 7 775) and
game 7 from bridge 50 on PAL (5 975): no difference.

### The plane against an object: by boxes and bits

P2PL's bit for player 3 is read after an object's rows (B60A) or at the river's end, and sees
the scanlines since the HITCLR before them: only that object's rows, at its position and size.
So "the plane touched the object" is worked out without the scanlines: the plane's 14 rows
from $AA against the object's rows where the loop wrote them, bit by bit (`planeTouches`), at
the same two places (the store's cycles count as before); the pair P2-P3 is gone from the
scanlines' collisions. Measured first beside the DLI's value (`RR_GEO_CHECK`, since removed):
the same in every frame of the original's runs above (~47 000 frames, 420 with the hit) and of
`display_bench` (200 000 frames each with `-wild` on NTSC and game 7 on PAL, 300 000 from bridge
100 on NTSC and on PAL with `-wild`: 61 382 hits). Then `game_check` against the original: no
difference; `RR_DISPLAY=check` (60 000 frames `-wild`, 60 000 from bridge 100 on PAL): nothing
but the edge bit of the kinds that do not turn (above).

### All the collisions without the scanlines' registers

The rest of the reads go the same way. Each sprite's rows are where the game put them (the
boxes above: nothing of them is anywhere else), so the scanlines where each collision the DLI
reads happens are worked out once a frame: the tank's rows against the edge (P1PF), the
plane's against the bank (P2PF), the missile's and the bullet's against either (M0PF, M2PF),
the plane against the shell (P2PL) and the missile against the plane (M0PL), bit by bit. An
object's collisions are worked out at its reads, against its rows where the loop wrote them:
the bullet (M2PL, up to the scanline before the last row, a bridge's included), the shell
(P3PL), the plane (P2PL), the edge (P3PF, only the kinds that turn). A read counts a
collision if its scanline is between the last HITCLR's and its own, as the registers did. The
DLI's loop, its cycles and HITCLR's scanlines stay (they decide those windows); the
scanlines' registers (`compute`, `seen`, the boxes by scanline) are gone. One case differs:
when the DLI falls a scanline behind, the rows written late are not fetched and the old ones
show; the collisions are now with the new rows. That happened in none of the runs here.
Checked: `game_check` against the original (all the runs above): no difference;
`RR_DISPLAY=check` (60 000 frames `-wild`, 60 000 from bridge 100 on PAL, 300 000 from bridge
100 on NTSC and 300 000 on PAL with `-wild`): nothing but the edge bit of the kinds that do
not turn.

### The objects drawn from their rows

Nothing reads player 3's rows but GTIA: the game never does. So `displaySimple` no longer
writes them, nor player 3's registers by scanline (HPOSP3, SIZEP3, the colours, the two blank
rows above each object). What it leaves for drawing is `drawn`: each object's first scanline,
position, size, rows and their colours, as the loop writes them, and the bank's colour by
scanline (`scan`'s `bankColour`; its player 3 fields stay 0); `drawRiver` draws the objects
from it. The loop itself stays as the schedule: which rows each object shows, where each read
and HITCLR comes, the cycles. `diffWithRam` does not compare player 3's rows ($0F00-$0FBF)
with `simpleDisplay`; `RR_DISPLAY=check` compares the two pictures instead of the scanlines.
When the DLI falls behind, the original shows last frame's rows where the late ones go; the
new rows are drawn here (in none of the runs). Checked: `game_check` against the original's
state (all the runs above): no difference; against its screens (`-screens`, 1300 frames of
the attract mode, 1500 of game 7 on PAL from bridge 50): the river area identical;
`RR_DISPLAY=check` (60 000 frames `-wild`, 60 000 from bridge 100 on PAL): the pictures the
same, nothing but the edge bit of the kinds that do not turn.

### RR_GEOMCHECK: plain geometry beside the reads

Before going further, a measurement: with `RR_GEOMCHECK` set, `displaySimple` also works out
each frame's collisions without the DLI's reads, with no window from a HITCLR to a read and no
cycles. An object is hit if its rows touch the sprite's at all (the last such object in the
loop's order, as the stores overwrite each other); the other collisions count if they happen
anywhere on the sprites' rows; an object's edge bit comes from all its rows. The reads'
results are kept; the frames where the geometry decides otherwise are counted by field and
printed at exit. `RR_GEOMCHECK=2` also prints each such frame: its number, what differs, the
bullet's, plane's and tank's places and each object's rows.

The result: 0.01-0.25% of frames differ, depending on the run, and always the same way: a
touch the reads see a frame later or not at all. The bullet on an object's last row (M2PL is
read before that row is drawn), the plane's or the tank's rows below the last read of the
frame. Nothing the geometry sees is a touch that did not happen on the screen.

## Branch `dli_removed`: `displayGeometry`

On this branch `Game::displayGeometry` (`port/src/display.cpp`) is the default
(`RR_DISPLAY=geometry`); `RR_DISPLAY=simple`, `beam` and `check` stay as above. It has no DLI
in it: no loop over the slots in scanlines, no cycles, no HITCLR, no reads. It is what the ZX
ports do, in C++, and the reference they are checked against frame by frame.

### The collisions as plain geometry

Each collision counts where the sprites' and the objects' rows touch at all, boxes first,
then the dots (the RR_GEOMCHECK rule above). An object hit is the last such object down the
screen, as the original's stores overwrite each other; the shell is not tested against a
bridge. The bank, edge, plane-shell and missile-plane collisions hold slot 0 when they happen
anywhere on the sprites' rows. The sprites' rows are the boxes from the game's positions
(step 3: the shell 16 rows from `shellY`, the tank 11 from `tankY`, the plane 14 from $AA, the
missile the row `missileY`, the bullet 8 rows below `bulletY`).

So the game is no longer the original's in every frame: on the frames RR_GEOMCHECK counts
(0.01-0.25%) a bullet hits an object or a plane hits the bank a frame earlier, or a touch the
original missed counts. `game_check` against the original is for `RR_DISPLAY=simple`; with the
default it reports those frames.

### Each object at its slot's y

The objects are placed straight from their slots, without the DLI's counter walk: rows from
scanline y - 65 down, where the DLI drew them (the bottom row of the shape table first, as the
tables keep them bottom up), cut at the river's top (scanline 28: the DLI's first wait was
X = $5D) and at its end (X past 255). The bridge's road colours come from those rows, the section's bank
colour after the last. Checked over 120 000 frames (`display_bench` from the start, from
bridge 60 on PAL, `-wild`, game 7) against the counter walk it replaced: the same objects,
rows and colours in every frame. The loop never fell behind an object, so only the river's top
and end cut them.

### The edge by a row's first and last dots

An object's edge bit (P3PF: helicopters, balloons, ships turn at the edge) comes from each of
its rows as a span, its first dot to its last, against the line's runs of edge pixels
(`Beam::edgeTouches`), without looking dot by dot: a run inside a hole of a row counts, unlike
the original's bit-by-bit test in GTIA. The Next tests it so: per line the edge's runs, per row
its two ends. Over 20 000 frames of the bot's games (more than 100 touches) the result was
never different from the dot-by-dot test.

### The other sprites against the bank and the edge the same way

`Beam::spanTouches`: a row's first and last dots against the line's runs of the colours wanted
(bank, edge, or either); `edgeTouches` is it with the edge. The tank against the edge, the
plane against the bank, the missile and the bullet against either (`pfAny`) use it too, so
every collision with the playfield is one test: the row's two ends against the line's runs.
The sprites with each other (the bullet, shell and plane against an object, the plane and the
shell, the missile and the plane) stay bit by bit: step 2 showed the holes matter there.

### What it costs and what it is for

The display is now three loops: the sprites' boxes against the runs of their lines, the pairs
of sprites where their boxes meet, the objects placed from their slots and tested against the
bullet, the shell and the plane. No scanline is walked that holds nothing. On the Next the
same work (`display_simple.asm`, `display_hits.asm` in river_raid_zx) takes 45 000-62 000
cycles of its 567 000 a frame at 28 MHz; its CODE.md and NOTES.md have the numbers routine by
routine.
