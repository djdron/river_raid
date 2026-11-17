# Experiments: the game without the DLI model

Branch `dli_removal`. The goal is a simpler display for ports to slow machines (the ZX
Spectrum Next): sprites drawn as ready pictures at their positions, collisions worked out per
object, no scanline-by-scanline model of the display list interrupt, while the game stays the
same as the original's frame by frame. Each step is checked against the original
(`rr_oracle` on libatari800, `game_check`).

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
