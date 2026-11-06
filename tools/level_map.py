#!/usr/bin/env python3
"""Cut a filmed level strip into one PNG per section (bridge to bridge) plus manifest.json.

Film first (invulnerable plane, one level row per frame at neutral stick):
  oracle/rr_oracle rom/River_Raid.rom oracle/scripts/ghost_film.txt 120000 out_film \
	  -ghost -noram -strip 20 -screens 231:231
Then:
  python tools/level_map.py out_film <outdir>

Each screen row 20 of a frame is the newest generated line; frame 231 is the launch frame,
whose rows 20..159 hold the lines generated before take-off. Sections are runs of equal $60.
"""
import csv
import json
import os
import sys

import numpy as np
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
os.makedirs(os.path.join(dst, 'tiles'), exist_ok=True)
F0 = 231
st = np.fromfile(os.path.join(src, 'strip.bin'), dtype=np.uint8).reshape(-1, 384)[:, 48:336]
scr = np.fromfile(os.path.join(src, 'screens.bin'), dtype=np.uint8).reshape(240, 384)[:, 48:336]
pal = np.fromfile(os.path.join(src, 'palette.bin'), dtype=np.uint8).reshape(256, 3)
v60 = [int(r['r60'], 16) for r in csv.DictReader(open(os.path.join(src, 'frames.csv')))]

bottom = [scr[y] for y in range(159, 19, -1)]                 # bottom-up order
col = bottom + [st[f] for f in range(F0 + 1, len(st))]
sec = [v60[F0]] * len(bottom) + [v60[f] for f in range(F0 + 1, len(st))]

segs, s = [], 0
for i in range(1, len(col) + 1):
	if i == len(col) or sec[i] != sec[s]:
		segs.append((s, i))
		s = i

gray = [i for i in range(256) if abs(int(pal[i, 0]) - int(pal[i, 1])) < 8
		and abs(int(pal[i, 1]) - int(pal[i, 2])) < 8 and 40 < pal[i, 0] < 200]
tiles = []
for k, (a, b) in enumerate(segs):
	block = np.array(col[a:b][::-1])                        # top row = furthest ahead
	img = Image.fromarray(block, 'P')
	img.putpalette(pal.flatten().tolist())
	name = f'tiles/s{k:03d}.png'
	img.save(os.path.join(dst, name), optimize=True)
	road = np.nonzero(np.isin(block, gray).sum(axis=1) > 150)[0]   # bridge road rows
	marks = [[int(road.min()), int(road.max())]] if len(road) else []
	tiles.append({'file': name, 'h': int(b - a), 'section': int(sec[a]), 'road': marks})
json.dump({'width': 288, 'tiles': tiles, 'total': len(col)}, open(os.path.join(dst, 'manifest.json'), 'w'))
print(f'{len(tiles)} sections, {len(col)} rows -> {dst}')
