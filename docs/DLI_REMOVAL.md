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
