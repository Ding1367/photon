#!/usr/bin/env python3
import sys
import struct

argv = sys.argv
argc = len(argv)

if argc < 3:
    print("usage: diff.py <front.bin> <back.bin>")
    raise SystemExit(1)

class Snapshot:
    def __init__(self, rows, cols, cells):
        self.rows = rows
        self.cols = cols
        self.cells = cells
    def __getitem__(self, idx):
        if type(idx) != tuple:
            return self.cells[idx]
        y = idx[0]
        x = idx[1]
        pos = y * self.cols + x
        if y >= self.rows or y < 0 or x < 0 or x >= self.cols:
            raise IndexError(f"index {y}, {x} (position {pos}) is out of bounds")
        return self.cells[pos]
    def __iter__(self):
        self._current = 0
        return self
    def __next__(self):
        if self._current == self.rows * self.cols:
            raise StopIteration
        row = self._current // self.cols
        col = self._current %  self.cols
        cell = self.cells[self._current]
        self._current += 1
        return (cell, row, col)

class InvalidSnapshot(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class NotASnapshot(Exception):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

class Cell:
    def __init__(self, fg, bg, style, ch):
        self.fg = fg
        self.bg = bg
        self.style = style
        self.ch = chr(ch)
    def __repr__(self):
        return f'Cell(fg=#{format(self.fg, "06x")}, bg=#{format(self.fg, "06x")}, style={format(self.style, "07b")}, ch={repr(self.ch)})'
    def __eq__(self, other):
        return self.fg == other.fg and self.bg == other.bg and self.style == other.style and self.ch == other.ch

def parse(file_path):
    with open(file_path, "rb") as file:
        header_bytes = file.read(16)
        if len(header_bytes) < 16:
            raise NotASnapshot()
        header, rows, cols, area = struct.unpack("Iiii", header_bytes)
        if header != 0xdecafc0f:
            raise NotASnapshot()
        if area != rows * cols:
            raise InvalidSnapshot("rows * cols != area")
        cell_fmt = f'iibb2x'
        size = struct.calcsize(cell_fmt) * area
        screen_bytes = file.read(size)
        if len(screen_bytes) < size:
            raise InvalidSnapshot("screen does not contain amount of cells as the area")
        cells = [*struct.unpack('iibb2x' * area, screen_bytes)]
        cells = [Cell(*cells[i:i+4]) for i in range(0, len(cells), 4)]
        return Snapshot(rows, cols, cells)

start = True
length = 0
start_pos = 0
front = parse(argv[1])
back  = parse(argv[2])

def extract_channels(i):
    return i >> 16, (i >> 8) & 0xFF, i & 0xFF

def cell_to_str(c):
    fg = [*extract_channels(c.fg)]
    bg = [*extract_channels(c.bg)]
    return f"\x1b[38;2;{fg[0]};{fg[1]};{fg[2]};48;2;{bg[0]};{bg[1]};{bg[2]}m{c.ch}"

if argc > 3 and argv[3] == '-v':
    for cell, y, x in back:
        if cell != front[y, x]:
            print(f"{cell_to_str(front[y, x])}\x1b[0m -> {cell_to_str(cell)}\x1b[0m\t{repr(front[y, x])} -> {repr(cell)}")
else:
    for cell, y, x in back:
        if cell != front[y, x]:
            if start:
                start_pos = y * back.cols + x
                print(f"diff at {y}, {x}, position {start_pos}:\n", end='')
                length = 0
                start = False
            length += 1
        else:
            if not start:
                _ = [print(cell_to_str(front[start_pos + x]), end='') for x in range(length)]
                print('\x1b[0m')
                _ = [print(cell_to_str(back [start_pos + x]), end='') for x in range(length)]
                print('\x1b[0m')
                start = True
    if start == False:
        print()
        for cell, _, _ in front:
            print(cell_to_str(cell), end='')
input()
