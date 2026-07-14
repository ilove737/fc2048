#!/usr/bin/env python3
"""
fc2048 CHR 图像预览工具
解析 tiles.chr，在终端/PNG 中渲染瓦片。

用法:
  python3 chrview.py tiles.chr --tile 0        查看单个瓦片
  python3 chrview.py tiles.chr --block 2       查看值"2"的 4×4 块
  python3 chrview.py tiles.chr --sheet         查看全部 512 瓦片
  python3 chrview.py tiles.chr --value 1       查看游戏值 val=1 的块
  python3 chrview.py tiles.chr --sheet -o out.png  导出 PNG
"""

import sys, os, textwrap

# ─── NES 标准 64 色调色板 ──────────────────────────────
NES_PAL = [
    0x545454, 0x001E74, 0x081090, 0x300088, 0x440064, 0x48003C,
    0x48001A, 0x440C00, 0x382400, 0x243800, 0x004000, 0x003C22,
    0x003C54, 0x000000, 0x000000, 0x000000,
    0x8C8C8C, 0x005CC8, 0x3838F0, 0x6800F0, 0x9800D0, 0xA8007C,
    0xA80038, 0x9C4400, 0x807C00, 0x54A800, 0x00B800, 0x00B068,
    0x00ACB8, 0x000000, 0x000000, 0x000000,
    0xD8D8D8, 0x0098F8, 0x4078FF, 0xA070FF, 0xDC60FF, 0xFC40D0,
    0xFC5098, 0xFC9840, 0xFCE800, 0xB4FC00, 0x50FC00, 0x00FC70,
    0x00FCF8, 0x000000, 0x000000, 0x000000,
    0xFFFFFF, 0x60D0FC, 0x98B8FF, 0xC8B8FF, 0xFCB8FF, 0xFC9CF8,
    0xFCC0D8, 0xFCD8A0, 0xFCE898, 0xD8F898, 0xA0F090, 0x78F0B0,
    0x78F0D8, 0x000000, 0x000000, 0x000000,
]

# ─── CHR 解析 ──────────────────────────────────────────
def read_chr(path):
    """读取 CHR 文件，返回 [tile] 列表，每个 tile 是 8×8 像素矩阵"""
    with open(path, 'rb') as f:
        data = f.read()
    n_tiles = len(data) // 16
    tiles = []
    for i in range(n_tiles):
        off = i * 16
        plane0 = data[off:off+8]
        plane1 = data[off+8:off+16]
        pixels = [[0]*8 for _ in range(8)]
        for y in range(8):
            for x in range(8):
                p0 = (plane0[y] >> (7-x)) & 1
                p1 = (plane1[y] >> (7-x)) & 1
                pixels[y][x] = (p1 << 1) | p0
        tiles.append(pixels)
    return tiles

# ─── 终端 ASCII 渲染 ───────────────────────────────────
CHARS = [' ', '\u2591', '\u2592', '\u2588']  # 4 级灰度

def tile_ascii(tile, palette=None):
    """单个瓦片 → 8 行 ASCII"""
    lines = []
    for y in range(8):
        line = ''
        for x in range(8):
            v = tile[y][x]
            line += CHARS[v] if v < 4 else '?'
        lines.append(line)
    return lines

def block_ascii(tiles, rows, cols, palette=None, w=1):
    """
    将 rows×cols 个瓦片拼接成 ASCII 块。
    w: 每个像素占的字符宽度
    """
    block_lines = []
    for r in range(rows):
        # 每个瓦片行 8 个像素行
        for py in range(8):
            line = ''
            for c in range(cols):
                t = tiles[r * cols + c]
                for px in range(8):
                    v = t[py][px]
                    line += (CHARS[v] if v < 4 else '?') * w
            block_lines.append(line)
    return block_lines

# ─── PNG 渲染 ──────────────────────────────────────────
def try_render_png(tiles, rows, cols, palette, output_path, scale=4):
    """尝试用 PIL 导出 PNG，失败则提示"""
    try:
        from PIL import Image
    except ImportError:
        print("PIL/Pillow 未安装，无法导出 PNG")
        return False

    tw, th = 8, 8
    w, h = cols * tw * scale, rows * th * scale
    img = Image.new('RGB', (w, h))
    px = img.load()

    for r in range(rows):
        for c in range(cols):
            tile = tiles[r * cols + c]
            for by in range(8):
                for bx in range(8):
                    v = tile[by][bx]
                    p = palette or NES_PAL
                    color = p[v] if v < len(p) else NES_PAL[0]
                    for dy in range(scale):
                        for dx in range(scale):
                            sx = (c * 8 + bx) * scale + dx
                            sy = (r * 8 + by) * scale + dy
                            if 0 <= sx < w and 0 <= sy < h:
                                px[sx, sy] = (
                                    (color >> 16) & 0xFF,
                                    (color >> 8) & 0xFF,
                                    color & 0xFF,
                                )

    img.save(output_path)
    print(f"已导出 {output_path} ({w}x{h})")
    return True

# ─── 瓦片索引映射（与 gen_tiles.py 一致） ───────────────
VALUE_BLOCK_BASE = {  # val → digit tile base
    0: 0,   # empty
    1: 104, # 2
    2: 112, # 4
    3: 120, # 8
    4: 128, # 16
    5: 136, # 32
    6: 144, # 64
    7: 152, # 128
    8: 160, # 256
    9: 168, # 512
    10: 176, # 1024
    11: 184, # 2048
}

BORDER_TILES = {  # name → tile index
    'TL': 192, 'TR': 193, 'BL': 194, 'BR': 195,
    'HLINE': 196, 'VLINE': 197,
}

BLOCK_BG_BASE = 96  # 8 shared background tiles (rows 0 and 3)

# ─── CLI ───────────────────────────────────────────────
def print_help():
    print(__doc__)

def main():
    if len(sys.argv) < 3:
        print_help()
        sys.exit(1)

    chr_path = sys.argv[1]
    if not os.path.exists(chr_path):
        print(f"文件不存在: {chr_path}")
        sys.exit(1)

    tiles = read_chr(chr_path)
    mode = sys.argv[2]

    # 默认调色板（预览用）
    pal = [NES_PAL[0x0F], NES_PAL[0x30], NES_PAL[0x10], NES_PAL[0x00]]

    # 解析可选参数
    output = None
    show_pal = False
    for i, arg in enumerate(sys.argv):
        if arg == '-o' and i + 1 < len(sys.argv):
            output = sys.argv[i + 1]
        if arg == '--pal':
            show_pal = True

    if mode == '--tile':
        n = int(sys.argv[3]) if len(sys.argv) > 3 else 0
        if n < 0 or n >= len(tiles):
            print(f"tile {n} 超出范围 (0-{len(tiles)-1})")
            sys.exit(1)
        print(f"Tile {n}:")
        for line in tile_ascii(tiles[n]):
            print('  ' + line)
        print(f"  像素值: {sum(sum(row) for row in tiles[n])}")
        if output:
            try_render_png([tiles[n]], 1, 1, pal, output)

    elif mode == '--block':
        name = sys.argv[3] if len(sys.argv) > 3 else '2'
        # 按名称或索引
        base = None
        if name.isdigit():
            val = int(name)
            if val in VALUE_BLOCK_BASE:
                base = VALUE_BLOCK_BASE[val]
            else:
                # 尝试直接作为 tile 索引
                base = val
        elif name.upper() in BORDER_TILES:
            base = BORDER_TILES[name.upper()]
            lines = block_ascii(tiles[base:base+1], 1, 1, pal)
            for l in lines:
                print(l)
            if output:
                try_render_png([tiles[base]], 1, 1, pal, output)
            return
        else:
            print(f"未知块名: {name}")
            sys.exit(1)

        if base is None or base + 16 > len(tiles):
            print(f"块起始 {base} 超出范围")
            sys.exit(1)

        print(f"4×4 块 (起始 tile {base}):")
        lines = block_ascii(tiles[base:base+16], 4, 4, pal)
        for l in lines:
            print(l)

        if output:
            try_render_png(tiles[base:base+16], 4, 4, pal, output)

    elif mode == '--value':
        val = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        if val not in VALUE_BLOCK_BASE:
            print(f"无效值 {val}，可用: {sorted(VALUE_BLOCK_BASE.keys())}")
            sys.exit(1)
        base = VALUE_BLOCK_BASE[val]
        # 合成完整 4×4 块：8 共享背景 + 8 数字瓦片
        bg = tiles[BLOCK_BG_BASE:BLOCK_BG_BASE+8]  # 行 0 (4) + 行 3 (4)
        digits = tiles[base:base+8]                 # 行 1-2
        # 行 0: bg[0..3], 行 1: digits[0..3], 行 2: digits[4..7], 行 3: bg[4..7]
        composed = bg[0:4] + digits[0:4] + digits[4:8] + bg[4:8]
        lines = block_ascii(composed, 4, 4, pal)
        for l in lines:
            print(l)
        if output:
            try_render_png(composed, 4, 4, pal, output)

    elif mode == '--sheet':
        n = len(tiles)
        cols = 16
        rows = (n + cols - 1) // cols
        print(f"瓦片表 ({n} tiles, {cols}x{rows}):")
        lines = block_ascii(tiles, rows, cols, pal, w=1)
        for l in lines:
            print(l)
        if output:
            try_render_png(tiles, rows, cols, pal, output, scale=2)

    elif mode == '--all-values':
        for val in sorted(VALUE_BLOCK_BASE):
            if val == 0: continue
            name = ['', '2','4','8','16','32','64','128','256','512','1024','2048'][val]
            base = VALUE_BLOCK_BASE[val]
            bg = tiles[BLOCK_BG_BASE:BLOCK_BG_BASE+8]
            digits = tiles[base:base+8]
            composed = bg[0:4] + digits[0:4] + digits[4:8] + bg[4:8]
            print(f"\n=== {name} (val={val}, tiles {base}-{base+7}) ===")
            lines = block_ascii(composed, 4, 4, pal)
            for l in lines:
                print(l)

    elif mode == '--border':
        for name, idx in BORDER_TILES.items():
            print(f"\n{name} (tile {idx}):")
            lines = tile_ascii(tiles[idx])
            for l in lines:
                print('  ' + l)
        if output:
            try_render_png(
                [tiles[i] for n, i in BORDER_TILES.items()],
                2, 3, pal, output
            )

    elif mode == '--palette' or show_pal:
        print("NES 调色板（前 16 色）:")
        for i, c in enumerate(NES_PAL[:16]):
            r, g, b = (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF
            print(f"  0x{i:02X}: #{r:02X}{g:02X}{b:02X}")

    else:
        print_help()

if __name__ == '__main__':
    main()