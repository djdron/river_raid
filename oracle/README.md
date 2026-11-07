# rr_oracle — River Raid reference runner

English | [Русский](README.ru.md)

Runs the original cartridge headless on libatari800 (Atari 800, NTSC by default, built-in AltirraOS, BASIC off). Used to compare the C++ port with the original frame by frame.

## Build
```
./build.sh
```
The script clones atari800 (tested commit 98cc2dc), builds `libatari800.a` and links `rr_oracle`. Needs git, gcc, make, autoconf and automake (Linux, WSL or MSYS2; Apple's linker does not support `-Wl,--wrap`).

## Run
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-bot|-bot-nofire] [-ghost] [-noram] [-ram A:B] [-strip Y] [-screens A:B] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
./rr_oracle ../rom/River_Raid.rom scripts/start_only.txt 8000 out -bot      # bot
./rr_oracle ../rom/River_Raid.rom scripts/bot_run_8000.txt 8000 out         # replay of a bot run
```

## Bot (`-bot`)
The script plays first (for example `start_only.txt`: power-on, START, wait), then the autopilot takes over. It steers toward the middle of the water ahead, dodges nearby objects, fires all the time, and pushes "up" while exploding or waiting at the bridge. The bot is simple and meant only for long traces, not high scores: it does not look for fuel and dies to helicopters coming from the side.

`-bot-nofire`: the same autopilot without firing or dodging.

The input actually applied in any run is written to `inputs.txt` in script format. Feeding it back without `-bot` gives a byte-identical trace (checked on 8000 frames).

## Options for filming the level
- `-ghost` — invulnerable plane (5-byte code patch in emulated memory, for filming only).
- `-strip Y` — screen row Y of every frame into `strip.bin`, palette into `palette.bin`.
- `-screens A:B` — full screens of frames A…B into `screens.bin`.
- `-noram` — do not write `ram.bin` (for long runs).
- `-ram A:B` — write `ram.bin` only for frames A…B (frame A at offset 0).
- `-wav` — the emulator's sound output into `sound.wav`.
- `-vbi` — `vbi.bin`: RAM at the start of every vertical blank interrupt (records of 4 bytes frame number + 16 384 bytes), the state the sound driver sees.
- `frames.csv` has the columns `$60, $2F, $5D, $AA, $AB, $AC, $76, $52, $53, $54, $2D, $4E`.

## Scripts
| File | What |
|---|---|
| `start_and_fly.txt` | START, acceleration, turns, firing (1000 frames) |
| `attract_fire_only.txt` | fire on the title screen (the game does not start) |
| `start_only.txt` | power-on, START, wait; for `-bot` |
| `probe_lr.txt`, `probe_hold.txt` | short taps / holds for finding variables |
| `bot_run_8000.txt` | recorded bot run: 2 bridges, 5 deaths, game over, new start |
| `refuel_first_depot.txt` | flying over the first depot without firing (refuelling) |
| `attract_idle.txt` | power-on and no input: the attract mode (river of game 1, scrolling credits) |
| `ghost_film.txt` | start and fly without input; with `-ghost -noram -strip 20` films the level map |

## Input script
Each line: `<frames> <joystick> <fire> [start]`; `#` starts a comment. The last line repeats until the end of the run.
Joystick bits: 1 up, 2 down, 4 left, 8 right, 0 centred.
The game starts with the START key; fire on the title screen does not start it.

## What goes into `<outdir>`
| File | Contents |
|---|---|
| `ram.bin` | $0000–$3FFF (16 KB) after every frame; frame N is at offset N*16384 |
| `pokey.log` | every POKEY write: `frame scanline cycle register value` |
| `random.log` | every RANDOM read: `frame scanline cycle value`. Scanline ~249 is sound (VBI), other scanlines are game logic (A205) |
| `frames.csv` | applied input and CPU registers at the end of the frame |
| `inputs.txt` | applied input in script format (for replay) |
| `fNNNNN.ppm` | 384×240 screenshots |

## Determinism
atari800 itself seeds the POKEY generator from the system clock, so runs are not repeatable. `rr_oracle` sets the counter explicitly (`-seed`, default 0). With the same ROM, script and seed all files are byte-identical (checked).

Licence: libatari800 is GPL-2.0+, so `rr_oracle` as a program linked with it is too. It is a development tool: the C++ port does not link with it and only reads its output files.
