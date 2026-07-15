#!/usr/bin/env python3
"""
生成 fc2048 NES 游戏的 CHR ROM 瓦片数据。
瓦片全部在 pattern table 0 内（tiles 0-255），PPU 背景单表模式。

布局：
  tiles   0- 95: ASCII 字体（来自 src/tiles.chr）
  tiles  96-103: 块背景共享（行 0 + 行 3）
  tiles 104-191: 数字瓦片（11 值 × 8 瓦片）
  tiles 192-197: 边框瓦片
  tiles 198-255: 填充
"""

import sys
import os

# ─── 5×7 位图字体（数字和大写字母） ──────────────────────────
FONT_5x7 = {
    '0': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    '1': [[0,0,1,0,0],[0,1,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,1,1,1,0]],
    '2': [[0,1,1,1,0],[1,0,0,0,1],[0,0,0,0,1],[0,0,0,1,0],[0,0,1,0,0],[0,1,0,0,0],[1,1,1,1,1]],
    '3': [[1,1,1,1,1],[0,0,0,0,1],[0,0,0,1,0],[0,0,0,0,1],[0,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    '4': [[0,0,0,1,0],[0,0,1,1,0],[0,1,0,1,0],[1,0,0,1,0],[1,1,1,1,1],[0,0,0,1,0],[0,0,0,1,0]],
    '5': [[1,1,1,1,1],[1,0,0,0,0],[1,1,1,1,0],[0,0,0,0,1],[0,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    '6': [[0,0,1,1,0],[0,1,0,0,0],[1,0,0,0,0],[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    '7': [[1,1,1,1,1],[0,0,0,0,1],[0,0,0,1,0],[0,0,1,0,0],[0,1,0,0,0],[0,1,0,0,0],[0,1,0,0,0]],
    '8': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    '9': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,1],[0,0,0,0,1],[0,0,0,1,0],[1,1,1,0,0]],
}

LETTERS_5x7 = {
    'A': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1]],
    'B': [[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,0]],
    'C': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,1],[0,1,1,1,0]],
    'D': [[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,0]],
    'E': [[1,1,1,1,1],[1,0,0,0,0],[1,0,0,0,0],[1,1,1,1,0],[1,0,0,0,0],[1,0,0,0,0],[1,1,1,1,1]],
    'F': [[1,1,1,1,1],[1,0,0,0,0],[1,0,0,0,0],[1,1,1,1,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0]],
    'G': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,0],[1,0,0,1,1],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    'H': [[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1]],
    'I': [[0,1,1,1,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,1,1,1,0]],
    'J': [[0,0,0,0,1],[0,0,0,0,1],[0,0,0,0,1],[0,0,0,0,1],[0,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    'K': [[1,0,0,0,1],[1,0,0,0,1],[1,0,0,1,0],[1,1,1,0,0],[1,0,0,1,0],[1,0,0,0,1],[1,0,0,0,1]],
    'L': [[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0],[1,1,1,1,1]],
    'M': [[1,0,0,0,1],[1,1,0,1,1],[1,0,1,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1]],
    'N': [[1,0,0,0,1],[1,0,0,0,1],[1,1,0,0,1],[1,0,1,0,1],[1,0,0,1,1],[1,0,0,0,1],[1,0,0,0,1]],
    'O': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    'P': [[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,0],[1,0,0,0,0],[1,0,0,0,0],[1,0,0,0,0]],
    'Q': [[0,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,1,0],[0,1,1,0,1]],
    'R': [[1,1,1,1,0],[1,0,0,0,1],[1,0,0,0,1],[1,1,1,1,0],[1,0,0,1,0],[1,0,0,0,1],[1,0,0,0,1]],
    'S': [[0,1,1,1,1],[1,0,0,0,0],[1,0,0,0,0],[0,1,1,1,0],[0,0,0,0,1],[0,0,0,0,1],[1,1,1,1,0]],
    'T': [[1,1,1,1,1],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0]],
    'U': [[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[0,1,1,1,0]],
    'V': [[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[0,1,0,1,0],[0,1,0,1,0],[0,0,1,0,0]],
    'W': [[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,0,0,1],[1,0,1,0,1],[1,1,0,1,1],[1,0,0,0,1]],
    'X': [[1,0,0,0,1],[1,0,0,0,1],[0,1,0,1,0],[0,0,1,0,0],[0,1,0,1,0],[1,0,0,0,1],[1,0,0,0,1]],
    'Y': [[1,0,0,0,1],[1,0,0,0,1],[0,1,0,1,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0],[0,0,1,0,0]],
    'Z': [[1,1,1,1,1],[0,0,0,0,1],[0,0,0,1,0],[0,0,1,0,0],[0,1,0,0,0],[1,0,0,0,0],[1,1,1,1,1]],
}

ALL_GLYPHS = {**FONT_5x7, **LETTERS_5x7}

# ─── 瓦片工具函数 ──────────────────────────────────────────
def empty_tile():
    return [[0]*8 for _ in range(8)]

def tile_to_chr(tile):
    """8×8 瓦片 → 16 bytes CHR 格式（plane0 + plane1）"""
    plane0 = [0]*8
    plane1 = [0]*8
    for y in range(8):
        for x in range(8):
            v = tile[y][x]
            plane0[y] |= ((v >> 0) & 1) << (7 - x)
            plane1[y] |= ((v >> 1) & 1) << (7 - x)
    return bytes(plane0 + plane1)

def tile_ascii(tile):
    """8×8 瓦片 → 8 行 ASCII 字符"""
    chars = {0: '░', 1: '░', 2: '▒', 3: '█'}
    return [''.join(chars.get(tile[y][x], ' ') for x in range(8)) for y in range(8)]

def render_glyph_8x8(ch, val=3):
    """5×7 字体居中放入 8×8 瓦片"""
    glyph = ALL_GLYPHS.get(ch)
    if not glyph:
        return empty_tile()
    tile = empty_tile()
    ox = (8 - 5) // 2
    oy = (8 - 7) // 2
    for dy, row in enumerate(glyph):
        for dx, p in enumerate(row):
            if p:
                tile[oy + dy][ox + dx] = val
    return tile

# ─── 块背景 ──────────────────────────────────────────────
def make_block_bg_tiles():
    """生成完整的 4×4 块背景（16 瓦片），返回 (bg_tiles, shared8)
    bg_tiles: 16 瓦片（行主序）
    shared8:  8 瓦片（行 0 的 4 瓦片 + 行 3 的 4 瓦片）
    """
    block = [[0]*32 for _ in range(32)]
    R = 3
    for y in range(32):
        for x in range(32):
            in_corner = False
            if x < R and y < R:
                in_corner = (x - R + 0.5)**2 + (y - R + 0.5)**2 > R*R - 1
            elif x >= 32 - R and y < R:
                in_corner = (x - 31 + R - 0.5)**2 + (y - R + 0.5)**2 > R*R - 1
            elif x < R and y >= 32 - R:
                in_corner = (x - R + 0.5)**2 + (y - 31 + R - 0.5)**2 > R*R - 1
            elif x >= 32 - R and y >= 32 - R:
                in_corner = (x - 31 + R - 0.5)**2 + (y - 31 + R - 0.5)**2 > R*R - 1
            if not in_corner:
                if y < 2 or y >= 30 or x < 2 or x >= 30:
                    block[y][x] = 2
                else:
                    block[y][x] = 1

    # 切 16 瓦片（行主序）
    all16 = []
    for ty in range(4):
        for tx in range(4):
            tile = empty_tile()
            for py in range(8):
                for px in range(8):
                    tile[py][px] = block[ty*8 + py][tx*8 + px]
            all16.append(tile)

    # 共享的 8 瓦片：行 0 (0-3) + 行 3 (12-15)
    shared8 = all16[0:4] + all16[12:16]
    return all16, shared8

def compose_digit_tiles(full16_bg, digit_str):
    """把数字渲染到块中央，返回 8 瓦片（行 1-2，全宽 4 列）"""
    tiles = [[row[:] for row in t] for t in full16_bg]
    n = len(digit_str)
    canvas = [[0]*32 for _ in range(16)]

    if n == 1:
        glyph = ALL_GLYPHS.get(digit_str[0])
        if glyph:
            ox = (32 - 16) // 2
            oy = (16 - 16) // 2
            for dy in range(7):
                for dx in range(5):
                    if glyph[dy][dx]:
                        for sy in range(2):
                            for sx in range(3):
                                cy = oy + dy * 2 + sy
                                cx = ox + dx * 3 + sx
                                if 0 <= cx < 32 and 0 <= cy < 16:
                                    canvas[cy][cx] = 3
    elif n == 2:
        for i, ch in enumerate(digit_str):
            glyph = ALL_GLYPHS.get(ch)
            if not glyph: continue
            dw = 5 * 2
            gap = (32 - 2 * dw) // 3
            ox = gap + i * (dw + gap)
            oy = (16 - 7 * 2) // 2
            for dy in range(7):
                for dx in range(5):
                    if glyph[dy][dx]:
                        for sy in range(2):
                            for sx in range(2):
                                cy = oy + dy * 2 + sy
                                cx = ox + dx * 2 + sx
                                if 0 <= cx < 32 and 0 <= cy < 16:
                                    canvas[cy][cx] = 3
    else:
        dw = 7 if n == 4 else 8
        total_w = n * dw
        ox = (32 - total_w) // 2 + (2 if n == 3 else 0) + (1 if n == 4 else 0)
        oy = (16 - 8) // 2
        for i, ch in enumerate(digit_str):
            glyph = ALL_GLYPHS.get(ch)
            if not glyph: continue
            for dy in range(7):
                for dx in range(5):
                    if glyph[dy][dx]:
                        sx = ox + i * dw + dx
                        sy = oy + dy
                        if 0 <= sx < 32 and 0 <= sy < 16:
                            canvas[sy][sx] = 3

    # 画布切到 8 瓦片（行 1-2，列 0-3，行主序 idx = ty*4 + tx）
    digit_tiles = []
    for ty in range(1, 3):
        for tx in range(4):
            tile = [[0]*8 for _ in range(8)]
            for py in range(8):
                for px in range(8):
                    tile[py][px] = tiles[ty*4 + tx][py][px]
            # 覆盖 canvas 像素
            for py in range(8):
                for px in range(8):
                    v = canvas[(ty-1)*8 + py][tx*8 + px]
                    if v:
                        tile[py][px] = v
            digit_tiles.append(tile)
    return digit_tiles

# ─── 边框瓦片 ──────────────────────────────────────────────
def make_border_tiles():
    tl = empty_tile()
    tr = empty_tile()
    bl = empty_tile()
    br = empty_tile()
    hline = empty_tile()
    vline = empty_tile()
    for y in range(8):
        for x in range(8):
            tl[y][x] = 1 if (x > 2   and y > 2 and x + y > 5 and x + y < 9) else 0
            tr[y][x] = 1 if ((7-x)>2 and y > 2 and (7-x)+y<9 and (7-x)+y>5) else 0
            bl[y][x] = 1 if (x > 2   and y > 1 and y < 5 and x+(7-y)<9 and x+(7-y)>5) else 0
            br[y][x] = 1 if ((7-x)>2 and y > 1 and y < 5 and (7-x)+(7-y)<9 and (7-x)+(7-y)>5) else 0
            hline[y][x] = 1 if y == 3 or y == 4 else 0
            vline[y][x] = 1 if x == 3 or x == 4 else 0
    return [tl, tr, bl, br, hline, vline]

# ─── 字体瓦片 ──────────────────────────────────────────
def load_font(path='src/tiles.chr'):
    with open(path, 'rb') as f:
        data = f.read()
    tiles = []
    for i in range(96):
        off = i * 16
        pixels = empty_tile()
        plane0 = data[off:off+8]
        plane1 = data[off+8:off+16]
        for y in range(8):
            for x in range(8):
                p0 = (plane0[y] >> (7-x)) & 1
                p1 = (plane1[y] >> (7-x)) & 1
                pixels[y][x] = (p1 << 1) | p0
        tiles.append(pixels)
    return tiles

# ─── 主生成 ──────────────────────────────────────────────
def generate_chr(output_path='tiles.chr', src_font='src/tiles.chr'):
    # 值列表
    values = [
        (1, '2'), (2, '4'), (3, '8'), (4, '16'), (5, '32'), (6, '64'),
        (7, '128'), (8, '256'), (9, '512'), (10, '1024'), (11, '2048'),
    ]

    # 1) ASCII 字体（0-95）
    tiles = load_font(src_font)  # 96 tiles

    # 2) 块背景共享（96-103）：8 瓦片（行 0 + 行 3）
    full16, shared8 = make_block_bg_tiles()
    tiles.extend(shared8)  # 8 tiles
    BLOCK_BG = len(tiles) - 8  # = 96

    # 3) 数字瓦片（104-191）：每值 8 瓦片
    DIGIT_BASE = len(tiles)  # = 104
    for val, ds in values:
        dt = compose_digit_tiles(full16, ds)
        tiles.extend(dt)  # 8 tiles per value

    # 4) 边框瓦片（192-197）
    BORDER_START = len(tiles)  # = 192
    border = make_border_tiles()
    tiles.extend(border)  # 6 tiles

    # 5) CJK 汉字瓦片（198-207）：手动绘制 16x16 字体
    CJK_START = len(tiles)  # = 198
    # 每个字 16x16 像素，拆成 4 个 8×8 瓦片（2×2）
    cjk_pixels = {
            '公': [
                "....##...##.....",
                "....##...##.....",
                "....##...##.....",
                "...##.....##....",
                "...#.......##...",
                "..##..##....##..",
                ".##...##.....##.",
                ".#...##.........",
                ".....##..##.....",
                "....##...##.....",
                "...##.....##....",
                "..###########...",
                "..######...##...",
                "................",
                "................",
                "................",
            ],
            '孙': [
                "######...##.....",
                "######...##.....",
                "....##...##.....",
                "..###.##.##.##..",
                "..##..##.##.##..",
                "..###.##.##.##..",
                "#####.##.##.##..",
                "####..##.##..##.",
                "..##.##..##..##.",
                "..##.##..##..##.",
                "..##.##..##..##.",
                "..##.....##.....",
                ".###...####.....",
                ".##....###......",
                "................",
                "................",
            ],
            '林': [
                "..##.....##.....",
                "..##.....##.....",
                "######.#######..",
                "######.#######..",
                "..##.....##.....",
                "..###...###.....",
                ".#####..####....",
                ".###.#.######...",
                "####..##.##.##..",
                "#.##..##.##..##.",
                "..##.##..##...#.",
                "..##.....##.....",
                "..##.....##.....",
                "..##.....##.....",
                "................",
                "................",
            ],
            '敬': [
                "..##.##..##.....",
                "########.##.....",
                "########.######.",
                "..##.##.#######.",
                "..#.....##..##..",
                ".#########..##..",
                "########.##.##..",
                "##....##.##.##..",
                ".####.##..#.#...",
                ".##.#.##..###...",
                ".####.##..##....",
                ".##.#.##..###...",
                ".....###.##.##..",
                ".....##.##...##.",
                "................",
                "................",
            ],
            '上': [
                "......##........",
                "......##........",
                "......##........",
                "......##........",
                "......########..",
                "......########..",
                "......##........",
                "......##........",
                "......##........",
                "......##........",
                "......##........",
                "###############.",
                "###############.",
                "................",
                "................",
                "................",
            ],
        }

    for name, rows in cjk_pixels.items():
        # Convert 16x16 text grid to pixel values
        pixels = [[1 if c == '#' else 0 for c in row] for row in rows]
        # Split into 4 tiles (2x2): top-left, top-right, bottom-left, bottom-right
        for ty in (0, 8):   # top/bottom
            for tx in (0, 8):  # left/right
                tile = [[0]*8 for _ in range(8)]
                for py in range(8):
                    for px in range(8):
                        if pixels[ty+py][tx+px]:
                            tile[py][px] = 3  # pixel value 3 = foreground color
                tiles.append(tile)

    # 6) 填充到 256
    while len(tiles) < 256:
        tiles.append(empty_tile())

    # 输出
    data = b''.join(tile_to_chr(t) for t in tiles)
    with open(output_path, 'wb') as f:
        f.write(data)

    print(f"生成 {output_path}")
    print(f"  瓦片总数: {len(tiles)}")
    print(f"  文件大小: {len(data)} 字节")
    print(f"  ASCII 字体: tiles 0-95")
    print(f"  块背景共享: tiles 96-103 ({BLOCK_BG})")
    print(f"  数字瓦片: tiles {DIGIT_BASE}-{DIGIT_BASE+88-1} (每值 8 瓦片)")
    print(f"  边框瓦片: tiles 192-197")
    print(f"  CJK 汉字: tiles {CJK_START}-{CJK_START+20-1} (5 字 × 4 瓦片)")
    print(f"  空瓦片填充: {256 - len(tiles)}/{256}")

    print("\n瓦片索引对照（用于 main.c）:")
    print("  #define TILE_EMPTY  0")
    print("  // ASCII 字体 tile = ascii - 0x20（原版不变）")
    print(f"  #define BLOCK_BG    {BLOCK_BG}  // 块背景共享")
    print(f"  #define DIGIT_BASE  {DIGIT_BASE}  // 数字瓦片起始")
    print(f"  #define TILE_TL     {BORDER_START}")
    print(f"  #define TILE_TR     {BORDER_START+1}")
    print(f"  #define TILE_BL     {BORDER_START+2}")
    print(f"  #define TILE_BR     {BORDER_START+3}")
    print(f"  #define TILE_HLINE  {BORDER_START+4}")
    print(f"  #define TILE_VLINE  {BORDER_START+5}")
    cjk_chars = ['公','孙','林','敬','上']
    print(f"  // CJK 汉字（手动绘制 16×16）")
    for i, name in enumerate(cjk_chars):
        base = CJK_START + i * 4
        print(f"  // {name}: tiles {base}-{base+3}")

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else 'tiles.chr'
    generate_chr(out)