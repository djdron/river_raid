#!/usr/bin/env python3
"""Re-check documented River Raid variables against an rr_oracle trace.

usage: python verify_trace.py <outdir> [<rom>]
  <outdir>  directory written by rr_oracle (needs ram.bin, frames.csv)
  <rom>     cartridge image (default ../rom/River_Raid.rom), used for the points table

Every check prints PASS/FAIL with counts; exit code 1 if any check fails.
"""
import csv
import os
import sys

import numpy as np

out = sys.argv[1]
rom_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..', 'rom', 'River_Raid.rom')
rom = open(rom_path, 'rb').read()
r = np.fromfile(os.path.join(out, 'ram.bin'), dtype=np.uint8).reshape(-1, 0x4000).astype(int)
inp = list(csv.DictReader(open(os.path.join(out, 'frames.csv'))))
failed = False


def report(name, ok, bad, extra=''):
	global failed
	failed |= bad > 0
	print(f"{'PASS' if bad == 0 else 'FAIL'}  {name}: {ok} ok, {bad} bad {extra}")


# frames where the plane is flying ($76 == 0), after the game first initialised RAM ($76 != 0 once)
init = next((f for f in range(len(r)) if r[f, 0x76] != 0), len(r))
flying = [f for f in range(init + 1, len(r)) if r[f, 0x76] == 0 and r[f - 1, 0x76] == 0]

# 1. level LFSR (sub_B183) on $AA/$AB/$AC: every change is 1-2 steps or a reload of $66-$68
def step(aa, ab, ac):
	fb = ((((ab << 5) & 0xFF) ^ ab) >> 7) & 1
	return ((aa << 1) | fb) & 0xFF, ((ab << 1) | (ac >> 7)) & 0xFF, ((ac << 1) | (aa >> 7)) & 0xFF

ok = bad = 0
for f in range(1, len(r)):
	p, c = tuple(r[f - 1, [0xAA, 0xAB, 0xAC]]), tuple(r[f, [0xAA, 0xAB, 0xAC]])
	if p == c:
		continue
	s1 = step(*p)
	if c in (s1, step(*s1)) or c == tuple(r[f, 0x66:0x69]):
		ok += 1
	else:
		bad += 1
report('level LFSR $AA/$AB/$AC', ok, bad)

# 2. fuel $5B:$5C (16-bit), code A3E9-A453. Every flying frame:
#    - over a fuel depot (collision slot $1D has kind $0500 == $0F): $5B += 2, capped at $FFFF
#      (ADC #1 with carry left set by CMP #$0E)
#    - then always: $5C -= $20, borrow decrements $5B (independent of speed)
def fuel_model(hi, lo, refuel):
	if refuel:
		hi += 2
		if hi > 255:
			hi, lo = 255, 255
	lo -= 0x20
	if lo < 0:
		lo += 256
		hi -= 1
	return hi, lo

ok = bad = refuels = 0
for f in flying:
	if r[f, 0x76] != 0:
		continue
	slot = r[f, 0x1D]
	# the collision is handled before objects shift to the next slot band in this frame,
	# so the slot kind must be taken from the start of the frame (previous snapshot)
	refuel = slot < 11 and r[f - 1, 0x500 + slot] == 0x0F
	want = fuel_model(r[f - 1, 0x5B], r[f - 1, 0x5C], refuel)
	if want == (r[f, 0x5B], r[f, 0x5C]):
		ok += 1
		refuels += refuel
	else:
		bad += 1
report('fuel: burn $20/frame, +2 in $5B over depot, cap $FFFF', ok, bad, f'({refuels} refuel frames)')

# 3. fuel is reset to $FFFF on respawn ($76 128 -> 127) and on game start ($76 -> 128, lives $6A == 4)
ok = bad = 0
for f in range(1, len(r)):
	if r[f, 0x5B] == 255 and r[f, 0x5C] == 255 and r[f - 1, 0x5B] * 256 + r[f - 1, 0x5C] < 0xFF00:
		if r[f, 0x76] == 127 or (r[f, 0x76] == 128 and r[f, 0x6A] == 4):
			ok += 1
		else:
			bad += 1
report('fuel reset to $FFFF on respawn / game start', ok, bad)

# 4. score $63-$65 (BCD, displayed x10) grows by points table $BB0B[type] for every destroyed slot
pts = lambda t: int(f"{rom[0xBB0B - 0xA000 + t]:02X}")
score = lambda f: int(''.join(f'{v:02X}' for v in r[f, 0x63:0x66]))
ok = bad = 0
special = []
for f in range(1, len(r)):
	d = score(f) - score(f - 1)
	if d <= 0:
		continue
	killed = [x for x in range(11) if r[f, 0x500 + x] in (2, 3) and r[f - 1, 0x500 + x] not in (2, 3)]
	exp = sum(pts(r[f, 0x50B + x]) for x in killed)
	if exp == d:
		ok += 1
	elif d == 75 and any(r[f, 0x50B + x] == 0x0E for x in killed) and r[f, 0x38] & 0x80:
		special.append(f)          # bridge destroyed with the road tank on it: table index $10
		ok += 1
	else:
		bad += 1
report('score += points[$BB0B + type]', ok, bad, f'(bridge+tank bonus at frames {special})' if special else '')

# 5. plane X $57 moves only while stick is left/right (fraction in $58)
ok = bad = 0
for f in flying:
	d = r[f, 0x57] - r[f - 1, 0x57]
	j = int(inp[f]['joy'])
	if d == 0:
		continue
	if (d < 0 and j & 4) or (d > 0 and j & 8) or (j & 12) == 0:
		ok += 1        # with neutral stick the plane may still drift while decelerating
	else:
		bad += 1
report('plane X $57 follows stick', ok, bad)

sys.exit(1 if failed else 0)
