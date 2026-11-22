"""A small model of how River Raid builds its river: one line at a time, from a
pseudo-random number generator. Simplified; port/src/river_gen.cpp has every rule."""

class Lfsr:
    """24 bits in three bytes a, b, c, stirred once per line (the cartridge's sub $B183)."""
    def __init__(self, a=0xE3, b=0xD4, c=0xF7):   # the start of game 1
        self.a, self.b, self.c = a, b, c

    def step(self):
        feedback = ((self.b >> 7) ^ (self.b >> 2)) & 1    # bits 7 and 2 of b
        out_a, out_c = self.a >> 7, self.c >> 7
        self.a = ((self.a << 1) | feedback) & 0xFF        # shift the chain a -> c -> b
        self.c = ((self.c << 1) | out_a) & 0xFF
        self.b = ((self.b << 1) | out_c) & 0xFF

ENEMIES = ['ship', 'ship', 'ship', 'heli', 'heli', 'heli', 'jet', 'balloon']   # the odds, not the order
LETTER = {'ship': 'S', 'heli': 'H', 'jet': 'J', 'balloon': 'B', 'fuel': 'F', 'bridge': '='}

class River:
    def __init__(self, level=1):
        self.rnd = Lfsr()
        self.level = level             # the bridge we fly towards
        self.line_in_block = 0         # 0..31
        self.blocks_left = 2           # in this section
        self.sections_left = 16        # to the next bridge
        self.bank = 32                 # left bank, in dots of the 192-dot line
        self.direction = 0             # the bank's walk: -1, 0, +1
        self.inner = 28                # land inside the 128-dot corridor, each side (or half the island)
        self.target = 28               # what inner moves towards
        self.island = False
        self.house_rows = 0            # rows of a house still to draw
        self.house_left = False
        self.bridge_rows = 0           # rows of a bridge still to draw

    def new_block(self):
        self.line_in_block = 0
        self.blocks_left -= 1
        if self.blocks_left == 0:                         # a new section: a new width
            self.blocks_left = 2
            self.sections_left -= 1
            if self.sections_left == 0:                   # we passed a bridge
                self.sections_left = 16
                self.level = min(self.level + 1, 80)
            if self.sections_left == 1:                   # the section that ends in a bridge
                self.island, self.target = False, 54
            elif self.level % 2 == 1:                     # odd levels: always the same width
                self.target = 28
            elif self.rnd.a & 0x30 == 0:                  # even levels, 1 in 4: the island opens or closes
                self.island, self.target = not self.island, 0
            else:                                         # even levels: a random width
                self.target = 4 * min(max(self.rnd.a & 15, 2), 14 if self.island else 15)
        if self.rnd.c & 0x20 and self.sections_left > 1:  # sometimes a house at a block's start
            self.house_rows = 12
            self.house_left = bool(self.rnd.c & 0x80)

    def move_bank(self):
        low, high = (28, 36) if self.level < 3 else (25, 40) if self.level % 2 == 0 else (13, 52)
        r = self.rnd.c & 15                               # 4 random bits decide
        if self.direction == 0:
            self.direction = 1 if r >= 8 else -1
        elif r < 8:
            return                                        # stay this line
        elif r == 13:
            self.direction = -self.direction              # turn round
        if low <= self.bank + self.direction <= high:
            self.bank += self.direction
        else:
            self.direction = -self.direction              # at a limit: turn round instead

    def new_object(self):
        """Every 16 lines something may enter at the top: an enemy, a fuel depot, a bridge."""
        if self.blocks_left == 1 and self.sections_left == 1:
            return ('bridge', None) if self.line_in_block == 0 else None
        if self.line_in_block == 0:                       # a block's start: depot, enemy or nothing
            if (0x3F - min(self.level, 46)) * 2 < self.rnd.b:
                kind = ENEMIES[self.rnd.b & 7]
            elif self.rnd.a & 0x40 == 0:
                kind = 'fuel'
            else:
                return None
        elif self.level >= 9 and (self.level - 8) * 8 >= self.rnd.b:
            kind = ENEMIES[self.rnd.b & 7]                # mid-block: from level 9, more often higher up
        else:
            return None
        if self.level % 2 == 1:                           # odd levels: anywhere on the water
            room = max(0, 128 - 2 * self.inner - 8)       # the water's width, less the object's
            x = self.bank + self.inner + (self.rnd.a & 0x3F) * room // 64
        elif self.rnd.a & 0x80:                           # even levels: by the right bank...
            x = self.bank + 128 - self.inner - 12
        else:                                             # ...or by the left one
            x = self.bank + self.inner + 4
        return kind, x

    def next_line(self):
        self.line_in_block += 1
        if self.line_in_block == 32:
            self.new_block()
        run_up = self.blocks_left == 1 and self.sections_left == 1
        if not run_up:                                    # the run-up to a bridge is straight
            self.rnd.step()
        if self.inner != self.target:                     # the width changes a dot a line
            self.inner += 1 if self.target > self.inner else -1
        if not run_up:
            self.move_bank()
        obj = self.new_object() if self.line_in_block % 16 == 0 else None
        return self.draw(obj)

    def draw(self, obj):
        dots = ['.'] * 192                                # land
        left, right = self.bank, self.bank + 128
        if self.island:                                   # water, the island, water
            water = [(left, 64 + left - self.inner), (64 + left + self.inner, right)]
        else:                                             # land inside the corridor on both sides
            water = [(left + self.inner, right - self.inner)]
        for start, end in water:
            dots[start:end] = '~' * (end - start)
        if self.house_rows:                               # a house on the wider bank
            self.house_rows -= 1
            x = left // 2 - 6 if self.house_left else (right + 192) // 2 - 6
            dots[x:x + 12] = '#' * 12
        if obj and obj[0] == 'bridge':
            self.bridge_rows = 6
        elif obj:
            kind, x = obj
            dots[x:x + 8] = LETTER[kind] * 8
        if self.bridge_rows:                              # a road across the whole screen
            self.bridge_rows -= 1
            dots = ['='] * 192
        return ''.join(dots[::4])                         # a character for every 4 dots

river = River(level=2)                                    # an even level: islands
lines = [river.next_line() for _ in range(1024)]          # from one bridge to the next
for line in reversed(lines):                              # the newest line is at the top of the screen
    print(line)
