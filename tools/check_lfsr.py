#!/usr/bin/env python3
"""Check the River Raid level LFSR model (sub_B183) against an rr_oracle trace.

usage: python check_lfsr.py <outdir>/ram.bin
Every change of $AA/$AB/$AC between frames must be 1-2 LFSR steps or a reload
of the saved seed from $66-$68.
"""
import sys
import numpy as np

def step(aa, ab, ac):
	# B183: A=AB<<5; A^=AB; ASL A -> carry = bit7(AB) ^ bit2(AB); ROL AA; ROL AC; ROL AB
	fb = (((ab << 5) & 0xFF) ^ ab) >> 7 & 1
	return ((aa << 1) | fb) & 0xFF, ((ab << 1) | (ac >> 7)) & 0xFF, ((ac << 1) | (aa >> 7)) & 0xFF

r = np.fromfile(sys.argv[1], dtype=np.uint8).reshape(-1, 0x4000)
lf = r[:, [0xAA, 0xAB, 0xAC]]
steps = reloads = bad = 0
for i in range(1, len(r)):
	prev, cur = tuple(map(int, lf[i - 1])), tuple(map(int, lf[i]))
	if prev == cur:
		continue
	s1 = step(*prev)
	if cur in (s1, step(*s1)):
		steps += 1
	elif cur == tuple(map(int, r[i, 0x66:0x69])):
		reloads += 1
	else:
		bad += 1
		print(f"frame {i}: {prev} -> {cur} unexplained")
print(f"steps {steps}, reloads {reloads}, unexplained {bad}")
sys.exit(1 if bad else 0)
