# River Raid (Atari 8-bit) — findings

English | [Русский](NOTES.ru.md)

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
1. **RNG.** `RANDOM` ($D20A, the POKEY polynomial) is read in 3 places. Game logic only at A205: every 16 frames an enemy slot (0–10) is chosen and bit $40 is set in its flags. The other two reads (B362, B3D0) only produce sound noise. Reproducing it exactly would require emulating POKEY's 17-bit polynomial tied to CPU cycles. The practical route: a deterministic RNG with the same distribution in the logic, and for bit-exact comparison feed the original's values from the reference trace.
2. **PAL/NTSC.** The PAL bit is read from $D014 (A06A), kept in $BF and affects the colour table (B77A) and branches at B135/B14F/B741. Pick a target mode or support both.
3. **Font from the OS.** The code copies the character set from the OS ROM ($E000/$E100/$E300) to $0600–$09FF. That is an external dependency: use our own font or the user's ROM (Altirra OS is free).
4. **Raster coupling.** Sprites are drawn by a DLI kernel with WSYNC on every line; in C++ this becomes a software renderer working from the frame state.
5. **Sound.** The driver lives entirely in the VBI ($B2A5–$B484): 4 POKEY channels, AUDF/AUDC computed from $C6, $7A–$7F, $50, $24 and the tables $B4AC/$B4C0/$B4E1. Reproducing this driver plus a POKEY core is enough for an exact match.
6. **Code style.** Monolithic, variables are reused by different subsystems, flags are packed into bits. Full mapping needs traces from an emulator.

## Data tables (from code references)
Purposes are tentative unless stated otherwise; only the way they are accessed is confirmed.
- $B4AC (4), $B4C0 (17), $B4E1: sound, read from the VBI.
- $B4D1: pointer table (read in pairs through $A1/$A2 at AEB9).
- $B776 (4 bytes) is copied to $3F00 (start of the display list); $B77A (15 or 30 bytes, depending on PAL) is copied to $3FA9.
- $BB70 (60 bytes) is copied to $1000; $BEEE (64) to $06C0; $BF2E (112) to $0818; $BF9E (24) to $08D0; $BFB6 (48) to $07D0 (probably characters and graphics).
- $BAAF, $BAC5, $BB24, $BB40: indexed from the logic and the DLI (probably object shapes and animation), not confirmed.

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
| $5B:$5C | fuel, 16 bits: $FFFF is a full tank; at neutral stick −32 per frame (−1 in $5B every 8 frames); fuel gauge needle HPOSM1 = $5B/8 + $5C (constant) in the DLI at B527 | verified (burn at neutral stick, reset to $FFFF) |
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
| Address | What | Status |
|---|---|---|
| $0C+X | vertical position of the object (grows with scrolling; ≈ 240–247 for the slot at the plane) | hypothesis from trace |
| $0500+X | state / animation frame: 0 empty, 8/9 helicopter frames, 2 explosion from a missile, 3 explosion from a collision; 1, 4–6, 13–15 also occur | partly verified (2 and 3 through points) |
| $050B+X | object type (index into the points table): 0, 1, 8, $0D, $0E, $0F in traces | verified through points |
| $0516+X | object X | from code (A500–A511, compared with $39) |
| $42+X | flags: bit 7, bit 6 ($40, set from RANDOM at A205), low bits | not analysed |
| $4E | slot index being processed (compared with #$0B) | from code |

### Reference runs for mapping
- `oracle/scripts/bot_run_8000.txt`: input recorded by the bot (`rr_oracle -bot`), 8000 frames. Contains 2 bridges (one with a tank), 5 deaths, game over around frame 3300, then the title screen and a new start around 5323. No real refuelling: the bot shoots the depots.
- Replaying this file without the bot gives byte-identical `ram.bin`, `pokey.log` and `random.log`.

## Next steps
1. A run with refuelling (a bot that does not shoot depots) and with speeding up / slowing down: the refuelling rule and fuel burn at other speeds.
2. Finish $42+X and $0500+X (enemy movement, when it switches on, the role of bit $40 and $60).
3. C++ skeleton (CMake): `GameState`, level generator, score, fuel; frame-by-frame comparison with `ram.bin`.
