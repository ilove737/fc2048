#!/usr/bin/env python3
"""
fc2048 CHR 图像预览工具
解析 tiles.chr，在终端/PNG 中渲染瓦片。

用法:
  python3 chrview.py tiles.chr --tile 0          查看单个瓦片
  python3 chrview.py tiles.chr --value 6        查看游戏值 val=6 的块
  python3 chrview.py tiles.chr --sheet -o out.png  导出瓦片表+数字方块合集
"""

import sys, os

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

# 游戏调色板索引（与 main.c 一致）
GAME_PALS = {
    'pal0': [0x0F, 0x0C, 0x10, 0x30],  # 边框
    'pal1': [0x0F, 0x28, 0x38, 0x30],  # 2,4
    'pal2': [0x0F, 0x27, 0x17, 0x30],  # 8,16,32
    'pal3': [0x0F, 0x16, 0x06, 0x37],  # 64+
}

# val → 游戏调色板
VAL_PAL = {0: 'pal0', 1: 'pal1', 2: 'pal1', 3: 'pal2', 4: 'pal2',
           5: 'pal2', 6: 'pal3', 7: 'pal3', 8: 'pal3', 9: 'pal3',
           10: 'pal3', 11: 'pal3'}

# 值标签
VAL_LABELS = ['', '2', '4', '8', '16', '32', '64', '128', '256', '512', '1024', '2048']

# ─── CHR 解析 ──────────────────────────────────────────
def read_chr(path):
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

# ─── 终端灰度渲染 ───────────────────────────────────────
CHARS = [' ', '\u2591', '\u2592', '\u2588']

def tile_gray(tile, w=1):
    lines = []
    for y in range(8):
        line = ''
        for x in range(8):
            v = tile[y][x]
            line += (CHARS[v] if v < 4 else '?') * w
        lines.append(line)
    return lines

def block_gray(tiles, rows, cols, w=1):
    block_lines = []
    for r in range(rows):
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
VALUE_BLOCK_BASE = {
    0: 0,   1: 104,  2: 112,  3: 120,  4: 128,  5: 136,
    6: 144, 7: 152,  8: 160,  9: 168, 10: 176, 11: 184,
}

BORDER_TILES = {
    'TL': 192, 'TR': 193, 'BL': 194, 'BR': 195,
    'HLINE': 196, 'VLINE': 197,
}

BLOCK_BG_BASE = 96

# ─── 辅助 ───────────────────────────────────────────────
def pal_rgb(pal_name):
    return [NES_PAL[i] for i in GAME_PALS[pal_name]]

def val_pal_name(val):
    return VAL_PAL.get(val, 'pal0')

def val_pal_rgb(val):
    return pal_rgb(val_pal_name(val))

def compose_block(tiles, val):
    """合成值 val 的 4×4 方块（16 瓦片）"""
    base = VALUE_BLOCK_BASE[val]
    bg = tiles[BLOCK_BG_BASE:BLOCK_BG_BASE+8]
    digits = tiles[base:base+8]
    return bg[0:4] + digits[0:4] + digits[4:8] + bg[4:8]


def save_combined_png(tiles, path, pal):
    """保存瓦片表 + 数字方块 + 调色板到一张大图"""
    import cairo

    TILE_SCALE = 2
    BLOCK_SCALE = 3
    PAD = 12
    TITLE_H = 24
    PAL_H = 20

    tg_size = 16 * 8 * TILE_SCALE  # 256
    bc = 4; br = 3
    bs = 32 * BLOCK_SCALE  # 96
    bg_w = bc * bs + (bc - 1) * 4
    row_h = bs + 4 + 16
    bg_h = (br - 1) * row_h + bs + 16

    iw = PAD + tg_size + PAD + bg_w + PAD
    ih = PAD + TITLE_H + PAD + max(tg_size, bg_h) + PAD + PAL_H + PAD

    surface = cairo.ImageSurface(cairo.FORMAT_RGB24, iw, ih)
    cr = cairo.Context(surface)

    cr.set_source_rgb(0.15, 0.15, 0.15)
    cr.rectangle(0, 0, iw, ih)
    cr.fill()

    cr.select_font_face('WenQuanYi Micro Hei', cairo.FONT_SLANT_NORMAL, cairo.FONT_WEIGHT_BOLD)

    cr.set_font_size(14)
    cr.set_source_rgb(0.9, 0.9, 0.9)
    cr.move_to(PAD, PAD + 16)
    cr.show_text('fc2048 CHR  —  瓦片表 + 数字方块')

    top = PAD + TITLE_H + PAD

    cr.set_font_size(10)
    cr.set_source_rgb(0.6, 0.6, 0.6)
    cr.move_to(PAD, top - 4)
    cr.show_text('瓦片表')

    for ty in range(16):
        for tx in range(16):
            tile = tiles[ty * 16 + tx]
            x = PAD + tx * 8 * TILE_SCALE
            y = top + ty * 8 * TILE_SCALE
            for py in range(8):
                for px in range(8):
                    v = tile[py][px]
                    c = pal[v] if v < len(pal) else 0
                    cr.set_source_rgb(((c >> 16) & 0xFF) / 255.0, ((c >> 8) & 0xFF) / 255.0, (c & 0xFF) / 255.0)
                    cr.rectangle(x + px * TILE_SCALE, y + py * TILE_SCALE, TILE_SCALE, TILE_SCALE)
                    cr.fill()

    bx = PAD + tg_size + PAD
    cr.set_font_size(10)
    cr.set_source_rgb(0.6, 0.6, 0.6)
    cr.move_to(bx, top - 4)
    cr.show_text('数字方块')

    for idx, val in enumerate(range(1, 12)):
        col = idx % 4
        row = idx // 4
        cx = bx + col * (bs + 4)
        cy = top + row * (bs + 4 + 16)
        block = compose_block(tiles, val)
        rgb = val_pal_rgb(val)

        for ty in range(4):
            for tx in range(4):
                tile = block[ty * 4 + tx]
                for py in range(8):
                    for px in range(8):
                        v = tile[py][px]
                        c = rgb[v] if v < len(rgb) else 0
                        cr.set_source_rgb(((c >> 16) & 0xFF) / 255.0, ((c >> 8) & 0xFF) / 255.0, (c & 0xFF) / 255.0)
                        cr.rectangle(cx + (tx * 8 + px) * BLOCK_SCALE, cy + (ty * 8 + py) * BLOCK_SCALE, BLOCK_SCALE, BLOCK_SCALE)
                        cr.fill()

        cr.set_font_size(9)
        cr.set_source_rgb(0.7, 0.7, 0.7)
        cr.move_to(cx, cy + bs + 12)
        cr.show_text(VAL_LABELS[val])

    pal_y = top + max(tg_size, bg_h) + PAD
    cr.set_font_size(10)
    cr.set_source_rgb(0.6, 0.6, 0.6)
    cr.move_to(PAD, pal_y + 14)
    cr.show_text('调色板:')

    x = PAD + 70
    for pname, idxs in GAME_PALS.items():
        for i in idxs:
            c = NES_PAL[i]
            r, g, b = (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF
            cr.set_source_rgb(r / 255.0, g / 255.0, b / 255.0)
            cr.rectangle(x, pal_y, 14, 14)
            cr.fill()
            x += 16
        cr.set_font_size(8)
        cr.set_source_rgb(0.5, 0.5, 0.5)
        cr.move_to(x, pal_y + 11)
        cr.show_text(pname)
        x += 32

    surface.write_to_png(path)
    surface.finish()


# ─── CLI ───────────────────────────────────────────────
def print_help():
    print(__doc__)

def main():
    if len(sys.argv) < 2:
        print_help()
        sys.exit(1)

    chr_path = sys.argv[1]
    if not os.path.exists(chr_path):
        print(f"文件不存在: {chr_path}")
        sys.exit(1)

    tiles = read_chr(chr_path)

    if len(sys.argv) < 3:
        # 默认行为：导出合集 PNG
        out_path = 'fc2048-chr.png'
        pal = [NES_PAL[0x0F], NES_PAL[0x0C], NES_PAL[0x10], NES_PAL[0x30]]
        save_combined_png(tiles, out_path, pal)
        print(f'已保存到 {os.path.abspath(out_path)}')
        return

    mode = sys.argv[2]

    pal = [NES_PAL[0x0F], NES_PAL[0x0C], NES_PAL[0x10], NES_PAL[0x30]]

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
        for line in tile_gray(tiles[n]):
            print('  ' + line)
        print(f"  像素值: {sum(sum(row) for row in tiles[n])}")
        if output:
            try_render_png([tiles[n]], 1, 1, pal, output)

    elif mode == '--value':
        val = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        if val not in VALUE_BLOCK_BASE:
            print(f"无效值 {val}，可用: {sorted(VALUE_BLOCK_BASE.keys())}")
            sys.exit(1)
        base = VALUE_BLOCK_BASE[val]
        vp = val_pal_rgb(val)
        bg = tiles[BLOCK_BG_BASE:BLOCK_BG_BASE+8]
        digits = tiles[base:base+8]
        composed = bg[0:4] + digits[0:4] + digits[4:8] + bg[4:8]
        pname = val_pal_name(val)
        print(f"=== {VAL_LABELS[val]} (val={val}, {pname}) ===")
        for line in block_gray(composed, 4, 4):
            print(line)
        if output:
            try_render_png(composed, 4, 4, vp, output)

    elif mode == '--sheet':
        if output:
            save_combined_png(tiles, output, pal)
            print(f'已保存到 {os.path.abspath(output)}')
        else:
            n = len(tiles)
            cols = 16
            rows = (n + cols - 1) // cols
            print(f"瓦片表 ({n} tiles, {cols}x{rows}):")
            for line in block_gray(tiles, rows, cols):
                print(line)

    elif mode == '--all-values':
        for val in sorted(VALUE_BLOCK_BASE):
            if val == 0: continue
            base = VALUE_BLOCK_BASE[val]
            vp = val_pal_rgb(val)
            bg = tiles[BLOCK_BG_BASE:BLOCK_BG_BASE+8]
            digits = tiles[base:base+8]
            composed = bg[0:4] + digits[0:4] + digits[4:8] + bg[4:8]
            pname = val_pal_name(val)
            print(f"\n=== {VAL_LABELS[val]} (val={val}, {pname}) ===")
            for line in block_gray(composed, 4, 4):
                print(line)
            if output:
                try_render_png(composed, 4, 4, vp, output)

    elif mode == '--border':
        for name, idx in BORDER_TILES.items():
            print(f"\n{name} (tile {idx}):")
            for line in tile_gray(tiles[idx]):
                print('  ' + line)
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

    elif mode == '--game-pal':
        print("游戏调色板预览:")
        print("  第 0 列=背景(透黑)  第 1 列=块填充  第 2 列=块边框  第 3 列=文字")
        for pname, idxs in GAME_PALS.items():
            hex_str = '  '.join(f'0x{i:02X}' for i in idxs)
            print(f"  {pname}: {hex_str}")
        print()
        for pname, idxs in GAME_PALS.items():
            print(f"  {pname}:")
            for i, idx in enumerate(idxs):
                c = NES_PAL[idx]
                r, g, b = (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF
                role = ['背景', '块填充', '块边框', '文字'][i]
                print(f"    0x{idx:02X}  #{r:02X}{g:02X}{b:02X}  ({role})")

    else:
        print_help()

if __name__ == '__main__':
    main()