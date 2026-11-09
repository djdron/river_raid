"""Draws the pictures in docs/HOW_IT_WORKS.md (docs/img/*.svg).

usage: python doc_figures.py <rom> [river_dump]

Runs the port's river_dump tool (port/build/river_dump by default) on game 1 and draws, in
plain colours of its own (no game graphics):
  stretch.svg     one stretch from bridge to bridge, with blocks, sections and objects
  levels.svg      stretches at different levels: how the river changes
  screen.svg      the screen: one object per band, one sprite for all of them
  difficulty.svg  chance of an enemy or a depot by level
  screen_layout.svg  the frame: river, panel, scrolling line, border
  seeds.svg       one river for all games; the rivers of other seeds
  bank_zoom.svg   a bank close up: bytes, the edge, one step per line
  object_mix.svg  what enters the river, by level (counted)
  bank_range.svg  the limits of the bank's random walk by level
  collisions.svg  which collisions the game reads
  speeds.svg      sideways speeds, steering, the river's speed
and the same in the other languages of the documents (*.ru.svg, *.uk.svg, ...;
labels in tools/doc_figures_i18n.py).
"""
import os
import subprocess
import sys
import tempfile

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.colors import ListedColormap
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from doc_figures_i18n import KEYS, LABELS   # noqa: E402

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, '..', 'docs', 'img')

# 2-bit colours of the river bitmap: black, bank, bank edge, water
RIVER_COLOURS = ListedColormap(['#000000', '#4f8f3a', '#9a7a3c', '#3d6fd0'])
FIRST_DOT, LAST_DOT = 12, 179           # the dots on screen (columns 24..359)

# object types (port/src/rom.h) -> name, marker colour, marker
OBJECTS = {
	7: ('jet', '#ffffff', '>'),
	8: ('helicopter', '#ffd23f', 'o'),
	10: ('helicopter that fires', '#ff5a36', 'o'),
	12: ('balloon', '#f28cff', '^'),
	13: ('ship', '#111111', 's'),
	15: ('fuel depot', '#ff2a8a', 'D'),
}

plt.rcParams.update({
	'font.family': 'DejaVu Sans',
	'font.size': 10,
	'svg.fonttype': 'none',
	'svg.hashsalt': 'river_raid',   # the same file every run
	'figure.facecolor': 'white',
	'axes.facecolor': 'white',
	'text.color': '#222222',
	'axes.labelcolor': '#222222',
	'xtick.color': '#444444',
	'ytick.color': '#444444',
})


LANG = 'en'
LANGUAGES = ('en',) + tuple(LABELS)
# the font for each language's labels; DejaVu Sans has no Chinese or Japanese characters
FONTS = {'zh': ['DejaVu Sans', 'Noto Sans CJK SC'], 'ja': ['DejaVu Sans', 'Noto Sans CJK JP']}
TRANSLATIONS = {lang: dict(zip(KEYS, texts)) for lang, texts in LABELS.items()}


def T(text):
	return TRANSLATIONS[LANG].get(text, text) if LANG != 'en' else text


def read_dump(path):
	"""Lines (level, sections left, blocks left, line in block, dots) and objects (line, type, dot)."""
	lines, objects = [], []
	with open(path) as f:
		for row in f:
			p = row.split()
			if p[0] == 'L':
				lines.append((int(p[1]), int(p[2]), int(p[3]), int(p[4]),
					np.frombuffer(p[5].encode(), dtype=np.uint8) - ord('0')))
			elif p[0] == 'O':
				objects.append((len(lines) - 1, int(p[1]), int(p[2])))
	return lines, objects


def stretch(lines, objects, level):
	"""The lines and objects of the stretch flown at `level` (towards bridge `level`)."""
	idx = [i for i, l in enumerate(lines) if l[0] == level]
	a, b = idx[0], idx[-1] + 1
	img = np.array([lines[i][4][FIRST_DOT:LAST_DOT + 1] for i in range(a, b)]).T
	objs = [(i - a, t, d - FIRST_DOT) for i, t, d in objects if a <= i < b and t in OBJECTS | {14: 0}]
	return img, objs, a


def draw_objects(ax, objs, size=22, bridges=True):
	for i, t, d in objs:
		if t == 14:
			if bridges:
				ax.axvspan(i - 2, i + 14, color='#c8c8c8', zorder=3)
			continue
		name, colour, marker = OBJECTS[t]
		ax.scatter([i + 8], [d], s=size, c=colour, marker=marker, edgecolors='#000000' if colour != '#111111' else '#ffffff',
			linewidths=0.6, zorder=4)


def save(fig, name):
	os.makedirs(OUT, exist_ok=True)
	if LANG != 'en':
		name = name.replace('.svg', f'.{LANG}.svg')
	fig.savefig(os.path.join(OUT, name), format='svg', bbox_inches='tight', metadata={'Date': None})
	plt.close(fig)


def figure_stretch(lines, objects):
	img, objs, start = stretch(lines, objects, 2)
	n = img.shape[1]
	fig, ax = plt.subplots(figsize=(13, 3.6))
	ax.imshow(img, cmap=RIVER_COLOURS, vmin=0, vmax=3, aspect=1.6, interpolation='nearest', origin='lower')
	draw_objects(ax, objs)
	# block and section boundaries
	sec = 0
	for i in range(n):
		lvl, sections, blocks, line_in_block, _ = lines[start + i]
		if line_in_block != 0:
			continue
		major = blocks == 2
		ax.axvline(i - 0.5, color='#ffffff' if major else '#ffffff', lw=1.2 if major else 0.4,
			alpha=0.95 if major else 0.6, zorder=5)
		if major:
			sec += 1
			ax.text(i + 32, img.shape[0] + 4, str(sec), ha='center', va='bottom', fontsize=9)
	ax.text(-8, img.shape[0] + 4, T('section'), ha='right', va='bottom', fontsize=9)
	y = -30
	arrow = dict(arrowstyle='<->', color='#222222', lw=1)
	ax.annotate('', xy=(0, y), xytext=(32, y), annotation_clip=False, arrowprops=arrow)
	ax.text(38, y, T('block = 32 lines'), va='center', fontsize=9)
	ax.annotate('', xy=(192, y), xytext=(256, y), annotation_clip=False, arrowprops=arrow)
	ax.text(262, y, T('section = 2 blocks = 64 lines'), va='center', fontsize=9)
	ax.annotate('', xy=(n - 64, y), xytext=(n, y), annotation_clip=False, arrowprops=arrow)
	ax.text(n - 70, y, T('last section: fixed width, the run-up to the bridge'), ha='right', va='center', fontsize=9)
	ax.text(n + 6, img.shape[0] / 2, T('bridge 2'), rotation=90, ha='left', va='center', fontsize=9)
	ax.set_xlim(-1, n + 1)
	ax.set_ylim(-1, img.shape[0])
	ax.set_xticks([0, 256, 512, 768, n])
	ax.set_xticklabels(['0', '256', '512', '768', f"{n} {T('lines')}"])
	ax.set_yticks([])
	ax.set_title(T('From bridge 1 to bridge 2 (level 2). The plane flies to the right; the screen shows 160 lines at a time.\n'
		'Brown and blue shapes on the banks are decorations: houses, trees.'), fontsize=10, loc='left', pad=22)
	legend(ax, objs)
	save(fig, 'stretch.svg')


def legend(ax, objs, loc='upper left', anchor=(0, -0.36)):
	kinds = sorted({t for _, t, _ in objs if t in OBJECTS}, key=list(OBJECTS).index)
	handles = [plt.Line2D([], [], ls='', marker=OBJECTS[t][2], markerfacecolor=OBJECTS[t][1],
		markeredgecolor='#000000' if OBJECTS[t][1] != '#111111' else '#ffffff', markersize=6, label=T(OBJECTS[t][0]))
		for t in kinds]
	handles += [plt.Rectangle((0, 0), 1, 1, color=c, label=T(l)) for c, l in
		(('#4f8f3a', 'bank'), ('#9a7a3c', 'edge'), ('#3d6fd0', 'water'), ('#c8c8c8', 'bridge'))]
	ax.legend(handles=handles, loc=loc, bbox_to_anchor=anchor, ncol=len(handles), frameon=False, fontsize=8.5,
		handletextpad=0.4, columnspacing=1.2)


def figure_levels(lines, objects):
	levels = [(1, 'level 1: straight, enemies stand still'), (2, 'level 2 (even): islands, objects next to a bank'),
		(11, 'level 11 (odd): one width, wider bends'), (12, 'level 12 (even): islands, narrow passages'),
		(31, 'level 31: enemies in mid-block too'), (52, 'level 52: an enemy every 16 lines, few depots')]
	fig, axes = plt.subplots(3, 2, figsize=(13, 6.6))
	axes = axes.T.flatten()
	all_objs = []
	for ax, (lvl, title) in zip(axes, levels):
		img, objs, _ = stretch(lines, objects, lvl)
		all_objs += objs
		ax.imshow(img, cmap=RIVER_COLOURS, vmin=0, vmax=3, aspect=1.6, interpolation='nearest', origin='lower')
		draw_objects(ax, objs, size=14)
		ax.set_xticks([])
		ax.set_yticks([])
		ax.set_xlim(-1, img.shape[1] + 1)
		ax.set_title(T(title), fontsize=9.5, loc='left', pad=3)
	legend(axes[2], all_objs, anchor=(0, -0.06))
	fig.subplots_adjust(hspace=0.3, wspace=0.04)
	save(fig, 'levels.svg')


def plane_dot(lines, top, rows=range(146, 160)):
	"""A dot where the plane is over water on screen rows `rows` (row 0 = line `top`): the middle
	of the widest stretch of water that is clear on all those rows, as in the game, where the
	plane flies over the river."""
	clear = np.ones(LAST_DOT - FIRST_DOT + 1, dtype=bool)
	for row in rows:
		clear &= lines[top - row][4][FIRST_DOT:LAST_DOT + 1] == 3
	best, start = (0, 0), None
	for x, c in enumerate(list(clear) + [False]):
		if c and start is None:
			start = x
		elif not c and start is not None:
			best = max(best, (x - start, (start + x - 1) // 2))
			start = None
	return best[1]


def figure_screen(lines, objects):
	# 160 lines as they are on screen, newest at the top, at level 12
	idx = [i for i, l in enumerate(lines) if l[0] == 12]
	top = idx[0] + 600
	rows = list(range(top, top - 160, -1))            # screen row 0 = newest line
	img = np.array([lines[i][4][FIRST_DOT:LAST_DOT + 1] for i in rows])
	fig, (ax, info) = plt.subplots(1, 2, figsize=(12, 5.4), gridspec_kw=dict(width_ratios=[1.5, 1]))
	ax.imshow(img, cmap=RIVER_COLOURS, vmin=0, vmax=3, aspect=1 / 1.6, interpolation='nearest')
	placed = [o for o in objects if top - 200 <= o[0] <= top + 40]
	for band, ((i, t, d), nxt) in enumerate(zip(placed, placed[1:] + [(top + 99, 0, 0)])):
		# the band runs from the line where the object entered up to the next object's line
		y0, y1 = top - i, top - nxt[0]                  # screen rows (row 0 at the top)
		if y0 < 0 or y1 > 159:
			continue
		lo_row, hi_row = max(y1 + 0.5, -0.5), min(y0 + 0.5, 159.5)
		if band % 2:
			ax.axhspan(lo_row, hi_row, color='#ffffff', alpha=0.12, zorder=2)
		ax.axhline(hi_row, color='#ffffff', lw=0.6, alpha=0.8, zorder=3)
		mid = (lo_row + hi_row) / 2
		if t in OBJECTS:
			name, colour, marker = OBJECTS[t]
			ax.scatter([d - FIRST_DOT], [y0 - 7], s=50, c=colour, marker=marker, zorder=4,
				edgecolors='#000000' if colour != '#111111' else '#ffffff', linewidths=0.8)
			label, col = T(name), '#222222'
		elif t == 14:
			label, col = T('bridge'), '#222222'
		else:
			label, col = T('(empty)'), '#888888'
		ax.text(LAST_DOT - FIRST_DOT + 6, mid, label, va='center', fontsize=8.5, color=col)
	px = plane_dot(lines, top)
	ax.scatter([px], [153], s=90, c='#e8e8e8', marker='^', edgecolors='#000000', zorder=4)
	ax.text(px + 8, 153, T('your plane (its own sprite)'), va='center', fontsize=8, color='#ffffff', zorder=5)
	ax.set_xticks([])
	ax.set_yticks([0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 159])
	ax.set_ylabel(T('screen line'))
	ax.set_xlim(-0.5, LAST_DOT - FIRST_DOT + 0.5)
	ax.set_title(T('The river on screen: one band per object, 16 lines (32 after a tall one)'), fontsize=10, loc='left')
	info.axis('off')
	info.text(0.2, 0.98, T('How one sprite shows them all'), fontsize=10.5, weight='bold', va='top')
	info.text(0.2, 0.90, T(
		'The picture is drawn line by line, top to bottom.\n'
		'At the top of each band an interrupt (DLI)\n'
		"moves player 3 to that band's object, sets its\n"
		'width and colours, then feeds it the shape\n'
		'one line at a time.\n'
		'\n'
		'At the end of the band the game reads the\n'
		"chip's collision registers and clears them:\n"
		'"player 3 touched the bullet" now means\n'
		'"the bullet hit this band\'s object".\n'
		'\n'
		'Every 16 lines (32 after a tall object) a new\n'
		'band enters at the top and the oldest leaves\n'
		'at the bottom.'),
		fontsize=9.5, va='top', linespacing=1.45)
	save(fig, 'screen.svg')


def figure_difficulty():
	lv = np.arange(1, 81)
	mid = np.where(lv >= 40, 1.0, np.clip(((lv - 8) * 8 + 1) / 256, 0, 1))
	mid[lv < 9] = 0
	t = (63 - np.minimum(lv, 46)) * 2
	enemy_start = (255 - t) / 256
	depot = (t + 1) / 512
	fig, ax = plt.subplots(figsize=(8.5, 3.6))
	ax.plot(lv, enemy_start * 100, color='#d1495b', lw=2, label=T('enemy at a block start'))
	ax.plot(lv, mid * 100, color='#edae49', lw=2, label=T('enemy in the middle of a block'))
	ax.plot(lv, depot * 100, color='#00798c', lw=2, label=T('fuel depot at a block start'))
	for x, label in ((3, 'balloons'), (5, 'jets'), (7, 'bank tanks'), (13, 'missile helicopters')):
		ax.axvline(x, color='#999999', lw=0.7, ls=':')
		ax.text(x - 0.3, 97, T(label), fontsize=8, va='top', ha='right', rotation=90, color='#555555')
	ax.set_xlim(1, 80)
	ax.set_ylim(0, 100)
	ax.set_xlabel(T('level (bridge number)'))
	ax.set_ylabel(T('chance, %'))
	ax.grid(True, color='#e6e6e6', lw=0.6)
	for s in ('top', 'right'):
		ax.spines[s].set_visible(False)
	ax.legend(frameon=False, fontsize=9, loc='center right')
	save(fig, 'difficulty.svg')


def run_dump(tool, rom, lines, seed=None):
	with tempfile.TemporaryDirectory() as tmp:
		path = os.path.join(tmp, 'river.txt')
		cmd = [tool, rom, '1', str(lines), path] + (['-seed', str(seed)] if seed else [])
		subprocess.run(cmd, check=True)
		return read_dump(path)


def figure_screen_layout(lines):
	# the frame in columns (half colour clocks, 0..383) and lines (0..239); a column is 0.86
	# of a line wide
	idx = [i for i, l in enumerate(lines) if l[0] == 12]
	top = idx[0] + 600
	frame = np.zeros((240, 384), dtype=np.uint8)
	for row in range(160):
		dots = lines[top - row][4]
		frame[20 + row, 24:360] = np.repeat(dots, 2)[24:360]
	frame[181:210, 24:360] = 4                       # panel
	frame[211:218, 24:360] = 5                       # scrolling line
	colours = ListedColormap(['#000000', '#4f8f3a', '#9a7a3c', '#3d6fd0', '#9a9a9a', '#3a3a3a'])
	fig, ax = plt.subplots(figsize=(10, 6.3))
	ax.imshow(frame, cmap=colours, vmin=0, vmax=5, aspect=1 / 0.86, interpolation='nearest')
	ax.add_patch(plt.Rectangle((23.5, 18.5), 336, 200, fill=False, ec='#ffcf33', lw=1.6, ls='--'))
	ax.scatter([24 + 2 * plane_dot(lines, top)], [20 + 153], s=110, c='#e8e8e8', marker='^', edgecolors='#000000', zorder=4)
	kw = dict(fontsize=9, va='center')
	ax.text(392, 100, T('river: 160 lines'), **kw)
	ax.text(392, 195, T('status panel: fuel, score, lives, bridges'), **kw)
	ax.text(392, 214, T('scrolling line: credits, Activision logo'), **kw)
	ax.text(392, 20, T('new lines enter here'), **kw)
	ax.text(392, 173, T('your plane stays here'), **kw)
	ax.text(392, 60, T('the river moves down:\n30 / 60 / 120 lines a second\n(slow / normal / fast)'), fontsize=9,
		va='center', linespacing=1.4)
	ax.annotate('', xy=(372, 150), xytext=(372, 30), annotation_clip=False,
		arrowprops=dict(arrowstyle='->', color='#222222', lw=1.4))
	ax.text(192, 232, T('shown by default (336 × 200), about what a TV showed'), color='#ffcf33', fontsize=8.5,
		ha='center', va='center')
	ax.text(192, 8, T('black border (-border shows it)'), color='#bbbbbb', fontsize=8.5, ha='center', va='center')
	ax.set_xticks([0, 24, 359, 383])
	ax.set_yticks([0, 20, 179, 209, 218, 239])
	ax.tick_params(labelsize=8)
	ax.set_title(T('The whole picture the Atari sends to the TV (384 × 240)'), fontsize=10, loc='left')
	save(fig, 'screen_layout.svg')


def figure_seeds(tool, rom, lines):
	n = 3 * 1024 + 40
	rivers = [(lines, T('the cartridge\'s river (every game, every Atari, since 1983)'))]
	rivers.append((run_dump(tool, rom, n, 12345)[0], T('-seed 12345: another river from the same rules')))
	rivers.append((run_dump(tool, rom, n, 777)[0], T('-seed 777')))
	fig = plt.figure(figsize=(13, 7.4))
	gs = fig.add_gridspec(4, 1, height_ratios=[0.7, 1, 1, 1], hspace=0.45)
	ax = fig.add_subplot(gs[0])
	ax.set_xlim(0, 61)
	ax.set_ylim(0, 1)
	ax.axhline(0.35, color='#3d6fd0', lw=6, solid_capstyle='butt')
	for b in range(1, 61):
		ax.plot([b, b], [0.25, 0.45], color='#666666', lw=0.8)
	for b, label, ha in ((1, T('games 1–2'), 'right'), (5, T('games 3–4'), 'left'), (20, T('games 5–6'), 'center'),
			(50, T('games 7–8'), 'center')):
		ax.plot([b, b], [0.2, 0.6], color='#d1495b', lw=2.2)
		ax.text(b + {'left': -0.3, 'right': 0.3, 'center': 0}[ha], 0.68, label, ha=ha, va='bottom',
			fontsize=9, color='#d1495b')
		ax.text(b, 0.1, f"{T('bridge')} {b}", ha='center', va='top', fontsize=8)
	ax.axis('off')
	ax.set_title(T('One river for all games: games 3–8 start further along it'), fontsize=10, loc='left')
	for k, (data, title) in enumerate(rivers):
		ax = fig.add_subplot(gs[k + 1])
		img = np.array([l[4][FIRST_DOT:LAST_DOT + 1] for l in data[:n]]).T
		ax.imshow(img, cmap=RIVER_COLOURS, vmin=0, vmax=3, aspect='auto', interpolation='nearest', origin='lower')
		ax.set_xticks([])
		ax.set_yticks([])
		ax.set_title(title, fontsize=9.5, loc='left', pad=3)
	fig.text(0.125, 0.085, T('The first 3 bridges of three rivers'), fontsize=9, color='#555555')
	save(fig, 'seeds.svg')


def figure_bank_zoom(lines):
	# a window of 40 lines on a left bank that moves, without decorations nearby
	best = None
	for start in range(40000, 52000, 7):
		rows = [lines[i][4][:72] for i in range(start, start + 40)]
		edges = []
		ok = True
		for r in rows:
			s = ''.join(map(str, r))
			e = s.find('2222')
			if e < 10 or e > 60 or '2' in s[:e] or '3' in s[:e] or s[e + 4:e + 12] != '33333333':
				ok = False
				break
			edges.append(e)
		if ok:
			moves = sum(a != b for a, b in zip(edges, edges[1:]))
			if best is None or moves > best[0]:
				best = (moves, start, edges)
	_, start, edges = best
	lo_dot = max(0, min(edges) - 14)
	lo_dot -= lo_dot % 4
	img = np.array([lines[i][4][lo_dot:lo_dot + 48] for i in range(start + 39, start - 1, -1)])
	fig, ax = plt.subplots(figsize=(9.5, 6))
	ax.imshow(img, cmap=RIVER_COLOURS, vmin=0, vmax=3, aspect=1 / 1.72, interpolation='nearest')
	for x in range(0, 49, 4):
		ax.axvline(x - 0.5, color='#ffffff', lw=0.7, alpha=0.85)
	for y in range(41):
		ax.axhline(y - 0.5, color='#000000', lw=0.25, alpha=0.35)
	row = 20
	e = edges[39 - row] - lo_dot
	ax.annotate(T('the edge: 4 dots, at any\ndot, across two bytes'), xy=(e + 2, row), xytext=(52, 8),
		annotation_clip=False, fontsize=9, va='center', arrowprops=dict(arrowstyle='->', color='#222222'))
	ax.annotate(T('each line the bank moves\nby at most one dot'), xy=(edges[39 - 32] - lo_dot - 0.5, 32),
		xytext=(52, 30), annotation_clip=False, fontsize=9, va='center',
		arrowprops=dict(arrowstyle='->', color='#222222'))
	ax.annotate('', xy=(0 - 0.5, -2.2), xytext=(4 - 0.5, -2.2), annotation_clip=False,
		arrowprops=dict(arrowstyle='<->', color='#222222', lw=1))
	ax.text(4.5, -2.2, T('byte: 4 dots'), fontsize=9, va='center')
	ax.set_xticks(range(0, 48, 8))
	ax.set_xticklabels([str(lo_dot + x) for x in range(0, 48, 8)])
	ax.set_yticks([])
	ax.set_xlabel(T('dot'))
	ax.set_title(T('A bank, close up (40 lines × 72 dots)').replace('72', '48'), fontsize=10, loc='left', pad=22)
	save(fig, 'bank_zoom.svg')


def figure_object_mix(lines, objects):
	bands = [(1, 2), (3, 4), (5, 12), (13, 39), (40, 55)]
	kinds = [(13, 'ship'), (8, 'helicopter'), (10, 'helicopter that fires'), (7, 'jet'), (12, 'balloon'), (15, 'fuel depot'),
		((0, 1), 'nothing')]
	colours = ['#111111', '#ffd23f', '#ff5a36', '#9ec5ff', '#f28cff', '#ff2a8a', '#dddddd']
	shares = []
	for a, b in bands:
		ts = [t for i, t, d in objects if a <= lines[i][0] <= b and t != 14]
		total = len(ts)
		shares.append([100 * sum(1 for t in ts if (t in k if isinstance(k, tuple) else t == k)) / total for k, _ in kinds])
	shares = np.array(shares)
	fig, ax = plt.subplots(figsize=(10, 3.6))
	left = np.zeros(len(bands))
	labels = [f"{T('levels')} {a}–{b}" for a, b in bands]
	for j, ((k, name), c) in enumerate(zip(kinds, colours)):
		ax.barh(labels, shares[:, j], left=left, color=c, edgecolor='white', lw=0.6, label=T(name))
		for y, (v, l) in enumerate(zip(shares[:, j], left)):
			if v >= 6:
				ax.text(l + v / 2, y, f'{v:.0f}', ha='center', va='center', fontsize=8,
					color='white' if c in ('#111111', '#ff2a8a', '#ff5a36') else '#222222')
		left += shares[:, j]
	ax.invert_yaxis()
	ax.set_xlim(0, 100)
	ax.set_xlabel(T('share, %'))
	for s in ('top', 'right'):
		ax.spines[s].set_visible(False)
	ax.legend(ncol=7, frameon=False, fontsize=8.5, loc='upper left', bbox_to_anchor=(0, -0.22), handlelength=1.2,
		columnspacing=1)
	ax.set_title(T('What enters the river every 16 lines (measured on game 1)'), fontsize=10, loc='left')
	save(fig, 'object_mix.svg')


def figure_bank_range():
	# RiverGen::moveBank: the limits of the bank's random walk by level (without the corridor
	# and decoration exceptions)
	def limits(level):
		if level < 3:
			return 0x1C, 0x24
		if level % 2 == 0 or level < 10:
			return 0x19, 0x28
		if level < 20:
			return 0x0D, 0x34
		return 0x01, 0x40
	levels = list(range(1, 81))
	fig, ax = plt.subplots(figsize=(10, 3.6))
	for lvl in levels:
		a, b = limits(lvl)
		ax.plot([lvl, lvl], [a, b], color='#d1495b' if lvl % 2 else '#00798c', lw=3.2, solid_capstyle='butt')
	ax.plot([], [], color='#d1495b', lw=4, label=T('odd level'))
	ax.plot([], [], color='#00798c', lw=4, label=T('even level'))
	ax.set_xlim(0, 81)
	ax.set_ylim(0, 72)
	ax.set_xlabel(T('level (bridge number)'))
	ax.set_ylabel(T('bank position, dots'))
	ax.grid(True, axis='y', color='#e6e6e6', lw=0.6)
	for sp in ('top', 'right'):
		ax.spines[sp].set_visible(False)
	ax.legend(frameon=False, fontsize=9, loc='upper left', ncol=2)
	ax.set_title(T('How far the bank may wander: the limits of its random walk'), fontsize=10, loc='left')
	save(fig, 'bank_range.svg')


def figure_collisions():
	cols = ['bank\n(green)', 'edge\n(brown)', 'enemies, depots,\nbridges (player 3)', 'your plane\n(player 2)', 'tank shell\n(player 0)']
	rows = ['your plane (player 2)', 'your bullet (missile 2)', 'helicopter\'s missile (missile 0)', 'tank (player 1)',
		'tank shell (player 0)']
	# (text, colour): red = you lose a plane, green = good for you, grey = stops, amber = not checked
	R, G, S, A = '#f4c7c3', '#cdebc8', '#e3e3e3', '#ffe8a3'
	cells = {
		(0, 0): ('crash', R), (0, 1): ('not checked:\nyou may touch it', A), (0, 2): ('crash; a depot\nrefuels', R),
		(0, 4): ('crash, while\nit explodes', R),
		(1, 0): ('stops', S), (1, 1): ('stops', S), (1, 2): ('the object\nexplodes, points', G),
		(2, 0): ('stops', S), (2, 1): ('stops', S), (2, 3): ('crash', R),
		(3, 1): ('stops and\nfires', S),
		(4, 2): ('destroys it,\nwhile exploding', S),
	}
	fig, ax = plt.subplots(figsize=(12, 4.4))
	ax.set_xlim(-2.6, len(cols))
	ax.set_ylim(len(rows), -1.1)
	ax.axis('off')
	for j, c in enumerate(cols):
		ax.text(j + 0.5, -0.55, T(c), ha='center', va='center', fontsize=8.8, weight='bold')
	ax.text(-0.1, -0.55, T('touches →'), ha='right', va='center', fontsize=8.8, color='#666666')
	for i, r in enumerate(rows):
		ax.text(-0.1, i + 0.5, T(r), ha='right', va='center', fontsize=9)
		for j in range(len(cols)):
			text, colour = cells.get((i, j), ('', '#ffffff'))
			ax.add_patch(plt.Rectangle((j + 0.03, i + 0.04), 0.94, 0.92, color=colour, ec='#cccccc', lw=0.5))
			if text:
				ax.text(j + 0.5, i + 0.5, T(text), ha='center', va='center', fontsize=8.3, linespacing=1.25)
	ax.set_title(T('What the game reads from the collision registers'), fontsize=10, loc='left')
	save(fig, 'collisions.svg')


def figure_speeds():
	fig, (a1, a2, a3) = plt.subplots(1, 3, figsize=(13, 3.6), gridspec_kw=dict(width_ratios=[1.25, 1, 1]))
	names = ['balloon', 'ship, helicopter', 'jet', 'your plane, full turn', 'helicopter\'s missile']
	speeds = [15, 30, 60, 60, 120]
	colours = ['#f28cff', '#ffd23f', '#9ec5ff', '#bbbbbb', '#ff5a36']
	a1.barh([T(n) for n in names], speeds, color=colours, edgecolor='#555555', lw=0.5)
	for y, v in enumerate(speeds):
		a1.text(v + 2, y, str(v), va='center', fontsize=8.5)
	a1.axvline(168, color='#888888', lw=0.8, ls=':')
	a1.text(166, -0.6, T('the screen is 168 dots wide'), fontsize=8, ha='right', color='#666666')
	a1.set_xlim(0, 175)
	a1.invert_yaxis()
	a1.set_title(T('Sideways speed, dots a second'), fontsize=10, loc='left')
	# steering (Game::steer): +8 per frame up to $F8, a dot each time the fraction overflows:
	# accel / 256 dots a frame, a dot every frame once full
	accel, v = 0, []
	for f in range(60):
		if accel + 8 > 0xFF:
			v.append(59.92)
		else:
			accel += 8
			v.append(accel / 256 * 59.92)
	t = np.arange(1, 61) / 59.92
	a2.plot(np.concatenate([[0], t]), np.concatenate([[0], v]), color='#555555', lw=2)
	a2.set_xlabel(T('seconds'))
	a2.set_ylabel(T('dots a second'))
	a2.set_ylim(0, 65)
	a2.set_title(T('Holding the stick sideways'), fontsize=10, loc='left')
	# speed (Game::changeSpeed, scrollRiver): +-2 per frame between $40 and $FE, back to $80
	def run(stick_frames, faster):
		speed, out = 0x80, []
		for f in range(200):
			if f < stick_frames:
				up = faster
			elif speed == 0x80:
				out.append(speed)
				continue
			else:
				up = speed < 0x80
			if up:
				if speed <= 0xFD:
					speed += 2
			elif speed >= 0x41:
				speed -= 2
			out.append(speed)
		return np.array([2 * 60 if s >= 0xFE else 2 * s / 256 * 59.92 for s in out])
	t = np.arange(200) / 59.92
	a3.plot(t, run(90, True), color='#d1495b', lw=2, label=T('Stick forward for 1.5 s, then let go'))
	a3.plot(t, run(90, False), color='#00798c', lw=2, label=T('Stick back for 1.5 s, then let go'))
	a3.set_ylim(0, 130)
	a3.set_title(T('How fast the river comes'), fontsize=10, loc='left')
	a3.set_xlabel(T('seconds'))
	a3.set_ylabel(T('lines a second'))
	a3.legend(frameon=False, fontsize=8, loc='upper left', bbox_to_anchor=(0, -0.22))
	for a in (a1, a2, a3):
		for s in ('top', 'right'):
			a.spines[s].set_visible(False)
		a.tick_params(labelsize=8.5)
	for a in (a2, a3):
		a.grid(True, color='#e6e6e6', lw=0.6)
	fig.subplots_adjust(wspace=0.45)
	save(fig, 'speeds.svg')


def main():
	if len(sys.argv) < 2:
		print(__doc__)
		return 2
	rom = sys.argv[1]
	tool = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '..', 'port', 'build', 'river_dump')
	lines, objects = run_dump(tool, rom, 56000)
	global LANG
	for LANG in LANGUAGES:
		plt.rcParams['font.family'] = FONTS.get(LANG, ['DejaVu Sans'])
		figure_stretch(lines, objects)
		figure_levels(lines, objects)
		figure_screen(lines, objects)
		figure_difficulty()
		figure_screen_layout(lines)
		figure_seeds(tool, rom, lines)
		figure_bank_zoom(lines)
		figure_object_mix(lines, objects)
		figure_bank_range()
		figure_collisions()
		figure_speeds()
	return 0


if __name__ == '__main__':
	sys.exit(main())
