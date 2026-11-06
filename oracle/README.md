# rr_oracle — River Raid reference runner

English | [Русский](README.ru.md)

Runs the original cartridge headless on libatari800 (Atari 800, NTSC by default, built-in AltirraOS, BASIC off). Used to compare the C++ port with the original frame by frame.

## Build
```
./build.sh
```
The script clones atari800 (tested commit 98cc2dc), builds `libatari800.a` and links `rr_oracle`. Needs git, gcc, make, autoconf and automake (Linux, WSL or MSYS2).

## Run
```
./rr_oracle <rom> <script> <frames> <outdir> [-pal] [-seed N] [-shot N,N,...]
./rr_oracle ../rom/River_Raid.rom scripts/start_and_fly.txt 1000 out -shot 240,500
```

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
| `frames.csv` | input and CPU registers at the end of the frame |
| `fNNNNN.ppm` | 384×240 screenshots |

## Determinism
atari800 itself seeds the POKEY generator from the system clock, so runs are not repeatable. `rr_oracle` sets the counter explicitly (`-seed`, default 0). With the same ROM, script and seed all files are byte-identical (checked).

Licence: libatari800 is GPL-2.0+, so `rr_oracle` as a program linked with it is too. It is a development tool: the C++ port does not link with it and only reads its output files.
