# River Raid (Atari 8-bit) — findings

English | [Русский](NOTES.ru.md) | [Українська](NOTES.uk.md) | [Deutsch](NOTES.de.md) | [Polski](NOTES.pl.md) | [Italiano](NOTES.it.md) | [Español](NOTES.es.md) | [中文](NOTES.zh.md) | [日本語](NOTES.ja.md)

Image: 8192 bytes, no CART header, loads at $A000–$BFFF. Code ~5.9 KB (2941 instructions), data ~2.3 KB.

## Entry points
| Address | What | How found |
|---|---|---|
| $A000 | start + main frame loop | cartridge trailer ($BFFA) |
| $B2A5 | VBI (VVBLKI $0222): POKEY sound, frame counters, per-frame registers | write to $0222/$0223 at A0D1 |
| $B500 | DLI (VDSLST $0200): per-line sprite multiplexing | write to $0200/$0201 at A0C7 |
| $B485 | IRQ stub | write to $0216/$0217 |

## Frame
- `L_AE99`: waits for VCOUNT < $50 (a new frame has started), prepares the display list and sprites.
- `L_A197`: waits for VCOUNT >= $50, reads the joystick (PORTA, TRIG0/1), runs the game logic.
- `A18D`: waits for VCOUNT = $78, then writes NMIEN ($C0 = DLI+VBI, $40 = VBI only).
- One pass of the loop = one logic step, so an "update() once per frame" model is correct.

## What matters for the port
1. **RNG.** `RANDOM` ($D20A, the POKEY polynomial) is read in 3 places. Game logic only at A205: every 16 frames an enemy slot (0–10) is chosen and bit $40 is set in its flags. The other two reads (B362, B3D0) only produce sound noise. The value depends on the CPU cycle of the read (POKEY's 17-bit polynomial runs at the machine clock). The port models the counter (`port/src/pokey_random.h`) and starts it from the clock, like a real power-on; for frame-by-frame comparison the original's values come from the reference trace (`random.log`).
2. **PAL/NTSC.** The PAL bit is read from $D014 (A06A), kept in $BF and chooses the display list variant ($B77A, copied into the display list at $3FA9), the depot limit `$CB` ($2E/$2F) and branches at B135/B14F/B741. Nothing compensates the frame rate: on PAL the game runs at 50 frames per second, about 17% slower. There was no separate PAL release: the UK cassette is the same code (see "UK cassette version"). The port's `-pal` sets the depot limit; the PAL display list and 50 Hz are not reproduced yet.
3. **Font from the OS.** The code copies the character set from the OS ROM ($E000/$E100/$E300) to $0600–$09FF. That is an external dependency: use our own font or the user's ROM (Altirra OS is free).
4. **Raster coupling.** Sprites are drawn by a DLI kernel with WSYNC on every line; in C++ this becomes a software renderer working from the frame state.
5. **Sound.** The driver lives entirely in the VBI ($B2A5–$B484): 4 POKEY channels, AUDF/AUDC computed from $C6, $7A–$7F, $50, $24 and the tables $B4AC/$B4C0/$B4E1. Reproducing this driver plus a POKEY core is enough for an exact match.
6. **Code style.** Monolithic, variables are reused by different subsystems, flags are packed into bits. Full mapping needs traces from an emulator.

## Data tables (from code references)
All of them are read through named accessors in `port/src/rom.h`.
- $B4AC (4): engine tone (AUDC) by stick up/down; $B4C0 (17): tank shell whistle pitch; $B4E1: extra-life sound.
- $B4D1: tank shapes (pointer pairs, read through $A1/$A2 at AEB9).
- $B776 (4 bytes) is copied to $3F00 (start of the display list); $B77A (15 bytes, the second 15 on PAL) to $3FA9, the rest of the display list.
- $BB70 (60 bytes): panel text, copied to $1000; $BEEE (64): Activision lettering → $06C0; $BF2E (112): panel symbols → $0818; $BF9E (24): "P1"/"P2" → $08D0; $BFB6 (48): the digits of "1982, 1983" → $07D0.
- $BAAF: next tank kind; $BAC5: shell colours; $BB24: next object kind (animation); $BB40: object shape offsets from $B900.

## Reference runner (done)
`oracle/rr_oracle` runs the original on libatari800 and writes the RAM of every frame, every POKEY write and every RANDOM read (see `oracle/README.md`).
- The game starts with the START key; fire on the title screen does not start it.
- atari800 seeds the POKEY generator from the clock; with `-seed` runs are byte-identical.
- RANDOM reads: scanline ~249 is sound from the VBI, scanlines 190–192 and 241 are game logic (A205). For C++ this means that in verification mode gameplay random values are taken from `random.log`, not emulated.

## Level generator (confirmed by trace)
- The river and object placement are driven by a software 24-bit LFSR in `sub_B183` on $AA/$AC/$AB. Called from A756 and A767.
- One step: `fb = bit7(AB) ^ bit2(AB)`, then a left shift of the chain AA←fb, AC←bit7(AA), AB←bit7(AC). The Python model explains 265 of 268 changes in the trace; the other 3 are reloads.
- The start value of AA/AB/AC is $E3/$D4/$F7. It is kept in $66–$68 and restored at A3C8 after the start and after a death (restart from the bridge).
- With different POKEY seeds over 1000 frames, $AA–$AC, $66–$68, object types ($0500…) and their X ($0516…) are identical. Only the slot flags $42–$4C (bit $40 from A205), the scratch byte $07 and the stack differ.
- Conclusion: the level is always the same. `RANDOM` only affects enemy behaviour (which one gets flag $40). Early in the game enemy movement seems to be off (the $60 check at A297), so it did not show in a short run.

## Variables (stage 2)
Status: **verified** means the rule is checked by `tools/verify_trace.py` on reference traces without exceptions. **from code** means the value was read from the listing without a separate trace check. **hypothesis** means a guess still to be confirmed.

### Player
| Address | What | Status |
|---|---|---|
| $57 | plane X (HPOS units; on the reference screen the centre ≈ 2·X−55 px) | verified: moves only while the stick is deflected |
| $58 | fractional part of X; on overflow $57 moves by ±1 (A462–A493) | from code |
| $59 | speed: 128 normal, up to ~142 with up, down to ~114 with down, back to 128 when released | from trace, no separate check |
| $5B:$5C | fuel, 16 bits, $FFFF is a full tank. Code A3E9–A453, every flying frame: over a depot `$5B += 2` (it is `ADC #1` with the carry left over from `CMP #$0E`), capped at $FFFF; then always `$5C −= $20` with borrow from `$5B` (−1 in $5B every 8 frames). If $5B is 0 on a borrow, the plane dies. Burn per frame does not depend on speed. The fuel gauge needle HPOSM1 = $5B/8 + $5C in the DLI at B527 (there $5C is the constant `#$5C`, not the variable) | verified, including refuelling: +480 per frame, capped at $FFFF |
| $1D | slot the plane collided with; bit 7 = none | verified through refuelling |
| $7A / $7B | sound event: 4 refuelling, 3 tank full (A40E–A422); $7B = 8 on a change, then the VBI counts it down | from code and trace |
| $6A | spare lives: 4 at game start, 3 after take-off, 0 on the last life | from trace |
| $76 | life phase: 128 at game start; 49/47 waiting at the bridge for take-off (any stick movement launches); 0 flying; 253 and then −3 per frame exploding; 130→127 respawn (fuel to $FFFF) | from trace |

### Score and bridges
| Address | What | Status |
|---|---|---|
| $63/$64/$65 | score, BCD, most significant byte first, stored divided by 10 (950 on screen = `00 00 95`); on overflow 999999 and $6A = 0 (A563) | verified |
| $BB0B + type | points/10 per object (BCD): 8 helicopter 60, $0D ship 30, $0E bridge 500, $0F fuel 80, $10 bridge with a tank 750; the table also has 7 → 100, $0A/$0B → 150 (not seen in traces) | verified for 8, $0D, $0E, $0F, $10 |
| $61/$62 | current bridge number, BCD (as "BRIDGE n" on screen), +1 when a bridge is destroyed (A4F0) | from code and trace |
| $38 bit 7 | the road tank was destroyed together with the bridge (A513–A517) | from code, 1 case in the trace |
| $39 | X of the road tank (HPOSP1 in the DLI at B54D) | from code |

Points are also awarded for the enemy the plane crashed into: its slot goes to state 3. Shot down by a missile it goes to state 2.

### Object slots (11, index X = 0…10)
A slot is a horizontal band of the screen, not a permanent object number: when scrolling, an object moves to the next slot (in the trace a depot went through slots 8 → 9 → 10). The collision check in a frame happens before that move.

| Address | What | Status |
|---|---|---|
| $0C+X | vertical position of the object (+1 per frame at normal speed; ≈ 240–247 at the plane) | from trace |
| $0500+X | current kind of the object. ≥ 7 is a live object: 7…$0D enemies (8/9 helicopter frames, $0D ship), $0E bridge, $0F depot. < 7 is empty or exploding: 2 shot by a missile, 3 collision; 0, 1, 4–6 also occur. On collision (A3E9): 7…$0D death, $0E death + `INC $2E`, $0F refuelling | verified (2/3 through points, $0F through refuelling) |
| $050B+X | object type (index into the points table): 0, 1, 8, $0D, $0E, $0F in traces | verified through points |
| $0516+X | object X | from code (A500–A511, compared with $39) |
| $42+X | flags: bit 7, bit 6 ($40, set from RANDOM at A205), low bits | not analysed |
| $4E | slot index being processed (compared with #$0B) | from code |

### Reference runs for mapping
- `oracle/scripts/bot_run_8000.txt`: input recorded by the bot (`rr_oracle -bot`), 8000 frames. Contains 2 bridges (one with a tank), 5 deaths, game over around frame 3300, then the title screen and a new start around 5323. No real refuelling: the bot shoots the depots.
- Replaying this file without the bot gives byte-identical `ram.bin`, `pokey.log` and `random.log`.

- `oracle/scripts/refuel_first_depot.txt`: flying over the first depot without shooting (32 frames over the depot, the tank reaches $FFFF).

## Terrain generation (code A6FD–A9D7, drawing AA8C–AB4B)
The bank is not stored as a picture but as a few numbers per line. Each new screen line is one generator step, after which the line is drawn into the screen's ring buffer (the line address comes from the display list, $3F04/$3F05).

**Scrolling (from code).** Every frame the speed `$59` is added twice to the accumulator `$30`, and each overflow produces one new line. At speed 128 that is exactly 1 line per frame; with `$59 ≥ $FE` a line is produced without accumulating.

**Structure (from code).**
- `$2F` — line number within the block, 0…31.
- `$5A` — blocks left in the section (2).
- `$5D` — sections left to the bridge, reloaded with 16; `sub_B194` is called at the end.
- The LFSR `sub_B183` steps on every line except lines with bit 7 set in `$23` (straight bridge approaches).
- At a section boundary a new width target `$5F` is taken from the low nibble of `$AA` with limits (minimum 2, maximum 15/14/8 depending on `$2D`, `$60`, `$B5`), then `$34 = $5F·4`.

**Numbers per line.**
| Address | What | Status |
|---|---|---|
| $52 | bank position | verified (random walk, below) |
| $C3 | walk direction: +1, $FF (−1) or 0 | verified |
| $54 | half width of the island / inner part | verified (smoothing) |
| $53 | water width; changes by −2 × the `$54` step, so the total width is kept | verified |
| $34, $35 | target for `$54` and the step toward it (±1 if the difference is < 32, else ±2) | verified |
| $2D | bit 7 — island mode (changes drawing), bit 6 | from code |
| $C4, $C5 | limits of `$52`: $1C…$24 when `$60 < 3`, otherwise wider (1…$40 with variants by `$60`, `$052D`, `$36`) | from code, 28…36 seen in traces |
| $23 bit 7 | near the bridge the bank follows the table `$BA8F[$2F] + $52`, no randomness | verified through C++ (`terrain_check`) |
| $052D | ≠ 0: the bank is pulled into the corridor $18…$28 | verified through C++ (`terrain_check`) |

**Bank random walk (A84F–A96A), verified.** Each line takes the low nibble `n = $AC & 15` (the LFSR byte after this line's step):
- if `$C3 = 0`, movement starts: +1 when `n ≥ 8`, otherwise −1;
- if the bank is already moving: with `n < 8` it holds; with `n = $D` it turns around and steps; with `n = $F` it holds if `$0E` (`$0D` on an island) is less than `$5F` or `$2C`; with any other value it steps;
- a step that would cross the limits `$C4/$C5` or run into the object in the current slot `$4E` (type ≥ 8; the boundary uses `$54/$34`, `$0516+X`, `$B5`, `$0521+X`) is replaced by turning around in place.

Timing subtlety: the slots and `$4E` are taken as at the start of the frame, while `$54/$34/$2D/$B5/$37` are the values already recomputed in this frame. With this rule the model matches the traces on all 4669 lines.

**Island smoothing (A7FB–A824), verified.** While `$35 ≠ 0`, each line does `$54 += $35` and `$53 −= 2·$35`. When `$54` reaches `$34`, the step `$35` is cleared. Checked on 4825 lines, 1195 of which actually changed.

**Decorations (houses, mountains).** Start at the beginning of a block (`$2F = 0`) when bit 5 of `$AC` is set. Offset, width and row count come from the tables `$BBCC`, `$BBD4`, `$BBB2`, `$BBEB` by `$AA` and `$5F`, the bitmap pointer from `$BBF0`. Verified through C++.

**Objects.** Every 16 lines a new object is placed in the top free slot (`$4D`−1); the type is chosen by `$AB` through the table `$BB1C`, the sprite width from `$BBA2` (into `$0521+X`, which is `SIZEP3`: 1 for the ship, 3 for the bridge, otherwise 0), the line count from `$BB60` (+4). While the object is still being drawn in (`$32` lines), the bank avoids that object (`$4E`). Verified through C++.

**Bridges.** The generator only draws a straight channel where the bridge is (bank from the table `$BA8F`) and puts an object of type `$0E` into a slot with flag `$80/$84` in `$42+X` (a copy of `$23`). The bridge itself appears at display time, in the DLI (B575–B5FC): for a slot with bit 7 in its flags, the bank colour `COLPF0` on every line comes from the table `$B4E5` (`04 04 04 04 08×6 1C 1C 06×5 04 04 04`: grey road bands and the yellow marking), so the road runs across the whole screen over bank pixels; the span over the water is that slot's P3 sprite at quad width, like ships and helicopters. After the object the bank colour is restored from `$0B` (`$D2`/`$D6`: the bank alternates two shades of green by section parity). The road tank is sprite P1 (`$39`). The bitmap lines of a bridge do not change after generation (checked on 119 lines).

**Colours and sprites per line (verified through C++, `port/src/river_draw.cpp`).** Base colours come from the vertical blank (B408–B43E): water `COLPF2` = $84 ($42 during the flash), edge `COLPF1` = $20, bank `COLPF0` from `sub_AFD0`: `($23 & 4) ^ $D6`, with bit 2 flipped while the newest object (`$4E`) is a bridge with y < $5C. The DLI walks the slots from `$4D` up with a scanline counter (value $5D at row 20, where new lines enter): the first slot with `0 ≤ y + $BB60[type] − counter ≤ $BB60[type]` gives the row r; the shape is `$B900 + $BB40[kind (+16 if flags bit 3)] + r`, the colour `$B700 + $BB30[type] + r`, HPOSP3 = `$0516+X` (32 = left edge of the buffer), SIZEP3 = `$0521+X`; for a bridge slot COLPF0 = `$B4E5 + r`. Compared with row 20 of the original: 96.8% of rows identical with generator positions, 98.1% with the game's object positions, all remaining rows while the road tank is active.

**Attract mode and the bottom of the screen (verified through C++, `port/src/status_bar.cpp`).** Without START the game runs the river of game 1 from power-on: the scrolling line moves from frame 11, the river from frame 38, 1 line per frame, through the bridges, with the same generator (the strip of row 20 matches C++ on 19 960 frames apart from moving objects). Display list at $3F00: 24 blank lines, 160 mode E lines, then the panel: 3 rows of mode 6 (normal width, `CHBASE` $08, colours $1E/$8E/$1E/$00 on $06, `PRIOR` 4) from $1000, set up from `$BB70`; fuel needle is missile 1 at `$5B/8 + $5C`. Last row: mode 6 wide with `HSCROL`, `CHBASE` $06, colour $0C, text at `$B800 + $3FB3`; the VBI (B44B) steps `HSCROL` 7→0 and then the text by one character ($4B…$D6, repeating) while `$2A` ≠ 0, and moves the logo `$27` with it. The logo is player 3 at quad width (data $FC) plus player 2 ($FF) shifted one clock left per row, in colours `$B48D`. The character sets at $0600/$0800 are the OS font (A0FB) with cartridge characters at $06C0, $0818, $08D0, $07D0 (the "1982, 1983" digits).

**Object movement, tank, missiles (verified through C++, `port/src/game.cpp`, `display.cpp`).** Every 16 frames a slot chosen by `RANDOM & 15` (11–15 → 6–10) gets bit 6 in `$42+X` ("awake"), unless it is the object still being drawn in. Objects of kind 7 (jet) move every frame; kinds 8–13 only when awake, from bridge 2 on (`$60 >> 1 ≠ 0`), on alternate frames by slot parity (`X ^ $18 ^ $33`), kind $0C only on frames with `$18 & 2 = 0`; 1 clock per step, bit 3 = left, wrapping $2C…$D4. Bit 0 of `$42+X` is set by the DLI when the object's sprite touched the bank edge colour (P3PF bit 1) on screen; the next move then reverses the direction once (bit 4 marks the turn until the contact ends). Kinds below $0C animate through `$BB24` every 8 frames (every 2 for kinds 7+). The tank (`$38`–`$3A`) drives every 16 frames (4 for kinds 4–7) until player 1 touches the edge colour, then (`$41` bit 7) fires: the shell is player 0, `$50` counts 0…$38 (flight table `$B4AF`, speed `$3E/$3F`), then explosion frames. A helicopter of kind $0A/$0B in slot `$51` fires missile 0 (`$C1` x, `$C2` y, step `$C0` = ±2) until it hits the bank (M0PF). The DLI's collision registers are read after each object and cleared, so collisions are known per slot (`$1A`–`$22` hold the slot number). With RANDOM replayed, C++ matches the original's RAM for 19 900 attract frames from power-on.

**Sound driver (verified through C++, `Game::soundDriver`).** Once per frame in the VBI (B2A5–B3D8), AUDCTL stays 0 (64 kHz clock, 17-bit noise). `$C6` ≠ 0: start beep on channel 1 ($40/$AF), the rest off. Channel 1: event sound `$7A` 1–4 for `$7B` frames (pitch $39/$11/$1F/$3F, volume from `$7B`); otherwise, with the plane flying, the low-fuel warning when `$5B` < $40 (a beep every 63 frames, a falling tone below $04) or the engine: AUDF = $1F − `$59`/16 (or the falling `$052C` after a crash), AUDC from the stick's up/down bits (`$B4AC`: $84 normal, $83/$86). Channel 2: explosion noise `$7C` (AUDF = RANDOM | $18) or, during play, the helicopter missile `$7F`. Channel 3: firing `$7D` or the score sound `$7E` (`$B4E1`). Channel 4: the tank shell `$50` (`$B4C0` while flying, then noise). The attract mode is silent: `$76` ≠ 0 turns channel 1 off and nothing else is triggered.

**The game with a player (verified through C++, `port/src/game.cpp`).** `$76` is the plane's state: 0 flying; $FE…$80 exploding (126 frames, `$5E` plane frame down every 8); then $7F: with lives left the river is reset (`sub_B0F1` with X = $12, which clears screen and player-missile memory for 13 frames while only interrupts run) and refilled at `$59` = $FE (2 lines per frame) while `$76` counts $7F…$30; at $30 a life is taken and `$052C` = $80 (the rising engine sound); at $2F the plane waits at the bridge without scrolling until any stick or fire input sets `$76` = 0 and `$59` = $40. Without lives: game over (high score `sub_B261` with check bytes `$90`–`$9F`, `sub_B21A`: `$76` = `$2A` = $FF, "Game over" in the scrolling line). Flying (A3E9–A5B4): collision of the plane with an object (`$1D`): kinds 7–$0E crash (a bridge also counts `$2E`), kind $0F refuels (`$5B` + 2 per frame, sounds `$7A` 4/3); shell explosion (`$1E` with `$50` ≥ $11), helicopter missile (`$21`) or the bank (`$1B`) crash; fuel burns $20/256 per frame; sideways movement accelerates (`$2B` += 8, fraction `$58`), frames 7/8/9; speed `$59` ±2 between $40 and $FE, back to $80 with the stick centred; the bullet (missile 2, 8 rows from `$56`, 6 rows up per frame) destroys objects of kind ≥ 7 (`$1C`), the score table `$BB0B` by type, a life every 10 000 points (up to 9). A destroyed bridge counts `$61/$62`, takes a tank standing on it, and from bridge 5 on makes a nearby tank fire. `sub_B1C0` decides where the next plane starts: past a destroyed bridge from the next section, otherwise one checkpoint back. Two-player games (odd `$09`) swap `$60`–`$6A` with `$6B`–`$75` (`sub_B247`) after each plane; player 2 uses the second joystick (PORTA high nibble). START (A673) starts a new game (B0F1 with X = $29, 13 frames); SELECT/OPTION every 30 frames the next game; any keyboard key pauses (`$24`: 3 → 2 paused → 1 → 0).

**Drawing (verified through C++).** A line is 48 bytes of bitmap (ANTIC mode E, 4 pixels per byte), built from fill runs (subroutines `AFEC`/`AFF0`/`AFF4`: `$AA`, `$FF`, `$55`) and 2-byte edge pieces from the table at `$B800` (`B034`/`B03B`): bank, water, island (with bit 7 of `$2D`), water, bank. The pixel position divided by 4 is the byte number, the low 2 bits select the edge piece. A decoration row is copied by `B01A`. The banks are not mirrored: the right edge is computed from the left one through the water and island widths, so the river meanders. On 2200 lines from C++: when both edges move, they move the same way in 810 cases out of 944; the river centre wanders between 71 and 122 pixels.

## C++ generator (`port/`)
- All per-line processing (A711–AE96 and subroutines) is ported to `port/src/river_gen.cpp`. The check `port/tools/terrain_check` compares every generator field and the 48 bytes of each new screen line with the original.
- Result: 119 758 lines of the whole river without differences; a start from scratch (`startGame`) matches too.
- The generator depends neither on the player (bot vs. flying without input) nor on `RANDOM` (3 seeds, 120 000 frames, byte-identical including the screen strip).
- Screen: ANTIC mode E bitmap, 48 bytes per line (192 pixels, 4 per byte), ring buffer, address of the new line in `$3F04/$3F05`, −$30 per line. Pixel values: 1 bank, 3 water, 2 edge, 0 rarely (inside edge pieces).
- Games 1–8: `$09` selects a row of the tables `$B494`–`$B4A8`: LFSR, bank and starting section. Games 1–2 start at bridge 1, 3–4 at 5, 5–6 at 20, 7–8 at 50. These are checkpoints of the same river: C++ reaches bridge 5/20/50 with exactly the table values (`$B4A4`: level, `$B4A8`: bridge counter in BCD). The port builds the same table by running the generator for `-seed N` (game 1's river from LFSR state N; the cartridge's river is $F7D4E3 = 16241891) and for `-bridge N` (every game from bridge N, 1–9999; after 80 the level goes 79, 80, 79...).
- `$CB` = $2E on NTSC and $2F on PAL (A077, from the PAL register); it affects the depot odds at the start of a block (AC48). The river differs slightly between PAL and NTSC.
- Start and respawn: `sub_B0F1` loads $4D–$5F from `$BAE1`, clears $2B–$4C and $0500–$05FF, puts the starting bridge into slot 10 (type $0E, y=$4A, height $17), mode $23 = $84/$80. Then A3C8 restores LFSR and bank from the checkpoint $66–$69. After that the game generates 160–165 lines at 2 per frame (speed $FE) to fill the screen.
- A verification pitfall: while the screen is being filled at 2 lines per frame, the emulator's frame boundary can fall between lines, so frame-by-frame comparison is wrong in those frames. In normal play (one line per frame) the snapshot is always on a line boundary.

## Level map
- The bank generator does not depend on the plane: object avoidance looks at the type `$050B`, which does not change when an object is shot. `$60` (section number) grows every 1024 lines, when the bridge is generated, not when it is destroyed (in the bot game `$60` changed at frame 1133, the bridge was destroyed at 1177).
- So the whole level can be filmed with an invulnerable plane: `rr_oracle -ghost` patches 5 bytes in emulated memory (A3E9/A424/A42E/A432 `LDX flag` → `LDX #$FF`, A443 fuel burn $20 → 0). No deaths in 120 000 frames.
- At normal speed the level moves exactly 1 screen line per frame, and the new line appears in screen row 20. A strip of row 20 from every frame matches the live screen except for moving objects.
- `$60` grows to $50 (80), then alternates $4F/$50; the LFSR state at section boundaries did not repeat over 117 sections, so the river does not loop, but difficulty stops growing after section 80.
- Building the per-section pictures: `tools/level_map.py`. The viewer is published as the page "Карта реки River Raid".

## Method
A rule is first read from the listing, then checked against a reference trace (`tools/verify_trace.py`). If the trace does not match, look for the misreading. Example: refuelling was derived from code A3E9–A453 (including the non-obvious +2 instead of +1 because of the carry flag) and confirmed on the trace; the only mismatch showed that the collision check happens before objects move between slots.

## UK cassette version
`River Raid.cas` (UK, 600 baud): two files. The boot file (7 blocks at $0A00, start $0A06) checks for 24K of RAM ("NOT ENOUGH RAM MEMORY - NEED 24K"), shows a title screen and loads $2200 bytes to $1000, then jumps to $3000. That is the cartridge's code relocated, compared instruction by instruction: from A21B on all 2 900-odd instructions are the same operations; code and data moved $A000 → $1000, the game's RAM pages up by $2000 (display list $3F00 → $5F00, screen ring $2000–$3FFF → $4000–$5FFF, $1000 → $3F00), zero-page variables mostly +2 ($25 → $A1, $28 → $A2); pointers in the tables moved the same way. Real differences:
- the init (A000–A0EE) moved to $3000, with a checksum of the loaded code into $E4/$E5;
- copy protection: if the checksum is wrong or a `D:` handler is in HATABS (booted from DOS), the main loop hangs or jumps into the middle of an instruction. The cartridge's own protection (A151–A158) stores $FF into its own code at $A19B unless $6000 holds $47: harmless in ROM, fatal in a RAM copy;
- the scrolling line says "1982, 1983, 1984": an extra character "4" (copied to $07C0), the text 5 characters longer, scroll start/wrap $20/$4B → $1B/$46.

Generator, game logic, sound and tables are identical; so is the PAL check.

## Next steps
Nothing open in the analysis. For the port: the exact power-on sequence (A000–A17C) and PAL timing (50 Hz, the PAL display list).
