"""Draws the pictures in docs/HOW_IT_WORKS.md (docs/img/*.svg).

usage: python doc_figures.py <rom> [river_dump]

Runs the port's river_dump tool (port/build/river_dump by default) on game 1 and draws, in
plain colours of its own (no game graphics):
  stretch.svg     one stretch from bridge to bridge, with blocks, sections and objects
  levels.svg      stretches at different levels: how the river changes
  screen.svg      the screen: one object per band, one sprite for all of them
  difficulty.svg  chance of an enemy or a depot by level
and the same with Russian labels (*.ru.svg).
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
# Russian labels for docs/HOW_IT_WORKS.ru.md
RU = {
	'jet': 'истребитель',
	'helicopter': 'вертолёт',
	'helicopter that fires': 'вертолёт с ракетами',
	'balloon': 'воздушный шар',
	'ship': 'корабль',
	'fuel depot': 'заправка',
	'bank': 'берег',
	'edge': 'кромка',
	'water': 'вода',
	'bridge': 'мост',
	'section': 'участок',
	'block = 32 lines': 'блок = 32 строки',
	'section = 2 blocks = 64 lines': 'участок = 2 блока = 64 строки',
	'last section: fixed width, the run-up to the bridge': 'последний участок: постоянная ширина, подход к мосту',
	'bridge 2': 'мост 2',
	'lines': 'строк',
	'From bridge 1 to bridge 2 (level 2). The plane flies to the right; the screen shows 160 lines at a time.\nBrown and blue shapes on the banks are decorations: houses, trees.': 'От моста 1 до моста 2 (уровень 2). Самолёт летит вправо; на экране одновременно 160 строк.\nКоричнево-синие фигуры на берегах — украшения: дома, деревья.',
	'level 1: straight, enemies stand still': 'уровень 1: прямая река, враги стоят на месте',
	'level 2 (even): islands, objects next to a bank': 'уровень 2 (чётный): острова, объекты у берега',
	'level 11 (odd): one width, wider bends': 'уровень 11 (нечётный): одна ширина, повороты шире',
	'level 12 (even): islands, narrow passages': 'уровень 12 (чётный): острова, узкие проходы',
	'level 31: enemies in mid-block too': 'уровень 31: враги и в середине блоков',
	'level 52: an enemy every 16 lines, few depots': 'уровень 52: враг каждые 16 строк, мало заправок',
	'(empty)': '(пусто)',
	'your plane (its own sprite)': 'ваш самолёт (свой спрайт)',
	'screen line': 'строка экрана',
	'The river on screen: one band per object, 16 lines (32 after a tall one)': 'Река на экране: по полосе на объект, 16 строк (32 после высокого)',
	'How one sprite shows them all': 'Как один спрайт показывает всех',
	'The picture is drawn line by line, top to bottom.\nAt the top of each band an interrupt (DLI)\nmoves player 3 to that band\'s object, sets its\nwidth and colours, then feeds it the shape\none line at a time.\n\nAt the end of the band the game reads the\nchip\'s collision registers and clears them:\n"player 3 touched the bullet" now means\n"the bullet hit this band\'s object".\n\nEvery 16 lines (32 after a tall object) a new\nband enters at the top and the oldest leaves\nat the bottom.': 'Картинка рисуется строка за строкой, сверху вниз.\nВ начале каждой полосы прерывание (DLI)\nпереносит игрока 3 к объекту этой полосы,\nзадаёт ему ширину и цвета и подаёт форму\nпо строке.\n\nВ конце полосы игра читает регистры\nстолкновений микросхемы и сбрасывает их:\n«игрок 3 коснулся пули» значит\n«пуля попала в объект этой полосы».\n\nКаждые 16 строк (32 после высокого объекта)\nсверху входит новая полоса, а самая старая\nуходит вниз.',
	'enemy at a block start': 'враг в начале блока',
	'enemy in the middle of a block': 'враг в середине блока',
	'fuel depot at a block start': 'заправка в начале блока',
	'balloons': 'шары',
	'jets': 'истребители',
	'bank tanks': 'танки на берегу',
	'missile helicopters': 'вертолёты с ракетами',
	'level (bridge number)': 'уровень (номер моста)',
	'chance, %': 'вероятность, %',
}


def T(text):
	return RU.get(text, text) if LANG == 'ru' else text


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
	if LANG == 'ru':
		name = name.replace('.svg', '.ru.svg')
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
	ax.scatter([84], [153], s=90, c='#e8e8e8', marker='^', edgecolors='#000000', zorder=4)
	ax.text(92, 153, T('your plane (its own sprite)'), va='center', fontsize=8, color='#ffffff', zorder=5)
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


def main():
	if len(sys.argv) < 2:
		print(__doc__)
		return 2
	rom = sys.argv[1]
	tool = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, '..', 'port', 'build', 'river_dump')
	with tempfile.TemporaryDirectory() as tmp:
		path = os.path.join(tmp, 'river.txt')
		subprocess.run([tool, rom, '1', '56000', path], check=True)
		lines, objects = read_dump(path)
	global LANG
	for LANG in ('en', 'ru'):
		figure_stretch(lines, objects)
		figure_levels(lines, objects)
		figure_screen(lines, objects)
		figure_difficulty()
	return 0


if __name__ == '__main__':
	sys.exit(main())
