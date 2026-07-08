#include "../neslib/neslib.h"
#include "game.h"

/* 名称表(nametable)地址：给定背景瓦片坐标(x,y)（x 为列 0~31，y 为行 0~29），
   返回该瓦片在名称表中的 PPU 地址。名称表起始于 0x2000，每行 32 个瓦片。 */
#define NT_ADDR(x, y) (0x2000 + (y) * 32 + (x))

/* 属性表地址：给定背景瓦片坐标(tx,ty)，返回其所在属性字节地址。
   属性表位于 0x23C0，屏幕按 4x4 瓦片分块，每块 1 字节（见 draw_game_over）。
   用法与 NT_ADDR 对应，用于把某区域指向指定背景调色板来"设置颜色"。 */
#define AT_ADDR(tx, ty) (0x23C0 + ((ty) / 4) * 8 + ((tx) / 4))

/* Board: 4x4 tiles per cell = 16x16 tiles total */
#define CELL_TILES 4
#define BOARD_TILES (BOARD_SIZE * CELL_TILES)

/* Board position: centered on 32x30 screen */
#define BOARD_X ((32 - BOARD_TILES) / 2)
#define BOARD_Y ((30 - BOARD_TILES) / 2)

/* Border tile indices */
#define TILE_TL      0x80
#define TILE_TR      0x81
#define TILE_BL      0x82
#define TILE_BR      0x83
#define TILE_HLINE   0x84
#define TILE_VLINE   0x85

/* ASCII tile mapping: '0' = 0x30, '1' = 0x31, etc. */
#define ASCII_0 0x30

/* ============================================================
 * NES 2C02 调色板完整参考（共 64 色，索引 0x00~0x3F）
 * 注意：实际显示颜色因模拟器/电视略有差异；
 *       0x0D/0x0E/0x0F/0x1D/0x1E/0x1F/0x2E/0x2F/0x3E/0x3F 近似黑/透明，慎用于文字。
 * 本游戏背景只用前 16 字节(pal0~pal3)，精灵用后 16 字节(pal4~pal7，未用)。
 * 本游戏用到的：0x0F(黑底)、0x00(黑)、0x10(深蓝灰)、0x15(亮红，GAME OVER)、0x30(白，文字)。
 * ============================================================
 * 0x00 深灰      0x10 浅灰      0x20 亮白      0x30 极浅灰
 * 0x01 午夜蓝    0x11 浅蓝      0x21 浅蓝      0x31 极浅蓝
 * 0x02 深蓝      0x12 蓝        0x22 淡蓝      0x32 浅灰蓝
 * 0x03 靛蓝      0x13 紫        0x23 天蓝      0x33 蓝紫(periwinkle)
 * 0x04 深紫      0x14 浅紫      0x24 薰衣草    0x34 极浅薰衣草
 * 0x05 酒红/品红 0x15 鲑红(亮红) 0x25 浅粉      0x35 浅鲑红
 * 0x06 栗色      0x16 红/橙红   0x26 珊瑚红    0x36 桃色
 * 0x07 暗红      0x17 橙        0x27 橙        0x37 亮黄
 * 0x08 棕        0x18 黄棕      0x28 黄        0x38 金黄
 * 0x09 深绿      0x19 深叶绿    0x29 中亮绿    0x39 黄绿
 * 0x0A 暗绿      0x1A 中绿      0x2A 亮霓虹绿  0x3A 亮绿
 * 0x0B 青绿      0x1B 亮绿      0x2B 水绿      0x3B 水绿
 * 0x0C 深青蓝    0x1C 青        0x2C 青        0x3C 浅青
 * 0x0D 黑(透明)  0x1D 黑(透明)  0x2D 浅灰      0x3D 银
 * 0x0E 黑(透明)  0x1E 黑(透明)  0x2E 黑(透明)  0x3E 黑(透明)
 * 0x0F 黑(透明)  0x1F 黑(透明)  0x2F 黑(透明)  0x3F 黑(透明)
 * ============================================================ */
static const unsigned char palette[32] = {
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x1B,
    0x0F, 0x00, 0x10, 0x17,
    0x0F, 0x00, 0x10, 0x15,   /* 背景调色板3：亮红前景(color3)，用于 GAME OVER */
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
};

/* Map board value (exponent) to digit string characters.
   Returns array of ASCII code points, len=number of chars */
static unsigned char get_chars(unsigned char val, unsigned char *buf) {
    /* Values: 0=empty,1='2',2='4',3='8',4='16',5='32',6='64',
               7='128',8='256',9='512',10='1024',11='2048' */
    switch (val) {
        case 0:  return 0;  /* empty */
        case 1:  buf[0]='2'; return 1;
        case 2:  buf[0]='4'; return 1;
        case 3:  buf[0]='8'; return 1;
        case 4:  buf[0]='1'; buf[1]='6'; return 2;
        case 5:  buf[0]='3'; buf[1]='2'; return 2;
        case 6:  buf[0]='6'; buf[1]='4'; return 2;
        case 7:  buf[0]='1'; buf[1]='2'; buf[2]='8'; return 3;
        case 8:  buf[0]='2'; buf[1]='5'; buf[2]='6'; return 3;
        case 9:  buf[0]='5'; buf[1]='1'; buf[2]='2'; return 3;
        case 10: buf[0]='1'; buf[1]='0'; buf[2]='2'; buf[3]='4'; return 4;
        case 11: buf[0]='2'; buf[1]='0'; buf[2]='4'; buf[3]='8'; return 4;
        default: return 0;
    }
}

/* Write 4x4 tile block for a cell, digits centered horizontally in row 1 */
static void draw_cell(unsigned char sx, unsigned char sy, unsigned char val) {
    unsigned char chars[4];
    unsigned char n;
    unsigned char i;
    unsigned char tiles[4];
    unsigned char sc;

    n = get_chars(val, chars);

    for (i = 0; i < 4; i++) {
        if (i < n) {
            unsigned char c = (unsigned char)chars[i];
            if (c >= '0' && c <= '9') {
                tiles[i] = c - 0x20;
            } else {
                tiles[i] = c;
            }
        } else {
            tiles[i] = TILE_EMPTY;
        }
    }

    vram_adr(NT_ADDR(sx, sy));
    vram_fill(TILE_EMPTY, 4);
    vram_adr(NT_ADDR(sx, sy + 1));
    vram_fill(TILE_EMPTY, 4);
    vram_adr(NT_ADDR(sx, sy + 2));
    vram_fill(TILE_EMPTY, 4);
    vram_adr(NT_ADDR(sx, sy + 3));
    vram_fill(TILE_EMPTY, 4);

    sc = (unsigned char)((4 - n) / 2);

    if (n >= 1) {
        vram_adr(NT_ADDR(sx + sc, sy + 1));
        vram_put(tiles[0]);
    }
    if (n >= 2) {
        vram_adr(NT_ADDR(sx + sc + 1, sy + 1));
        vram_put(tiles[1]);
    }
    if (n >= 3) {
        vram_adr(NT_ADDR(sx + sc + 2, sy + 1));
        vram_put(tiles[2]);
    }
    if (n >= 4) {
        vram_adr(NT_ADDR(sx + sc + 3, sy + 1));
        vram_put(tiles[3]);
    }
}

static void render_board(void) {
    unsigned char r, c, val;
    for (r = 0; r < BOARD_SIZE; r++) {
        for (c = 0; c < BOARD_SIZE; c++) {
            val = board[r * BOARD_SIZE + c];
            draw_cell(BOARD_X + c * CELL_TILES, BOARD_Y + r * CELL_TILES, val);
        }
    }
}

static void draw_border(void) {
    unsigned char i;
    unsigned char bx = BOARD_X - 1;
    unsigned char by = BOARD_Y - 1;
    unsigned char bw = BOARD_TILES + 2;
    unsigned char bh = BOARD_TILES + 2;

    vram_adr(NT_ADDR(bx, by));
    vram_put(TILE_TL);
    for (i = 0; i < bw - 2; i++) vram_put(TILE_HLINE);
    vram_put(TILE_TR);

    for (i = 0; i < bh - 2; i++) {
        vram_adr(NT_ADDR(bx, by + 1 + i));
        vram_put(TILE_VLINE);
        vram_adr(NT_ADDR(bx + bw - 1, by + 1 + i));
        vram_put(TILE_VLINE);
    }

    vram_adr(NT_ADDR(bx, by + bh - 1));
    vram_put(TILE_BL);
    for (i = 0; i < bw - 2; i++) vram_put(TILE_HLINE);
    vram_put(TILE_BR);
}

static void clear_nametable(void) {
    unsigned int i;
    vram_adr(0x2000);
    for (i = 0; i < 960; i++) vram_put(0x00);
    for (i = 0; i < 64; i++) vram_put(0x00);
}

static unsigned int high_score = 0;

static void draw_score(void) {
    unsigned char d[5];
    unsigned int s;
    unsigned char lead;
    unsigned char col;
    unsigned char nameIndex = 17;

    s = game_score;
    d[0] = (unsigned char)(s % 10); s = s / 10;
    d[1] = (unsigned char)(s % 10); s = s / 10;
    d[2] = (unsigned char)(s % 10); s = s / 10;
    d[3] = (unsigned char)(s % 10); s = s / 10;
    d[4] = (unsigned char)(s % 10);

    /* "SCORE" at row 2, centered above 18-tile border */
    vram_adr(NT_ADDR(3, 2)); vram_put('S' - 0x20);
    vram_adr(NT_ADDR(4, 2)); vram_put('C' - 0x20);
    vram_adr(NT_ADDR(5, 2)); vram_put('O' - 0x20);
    vram_adr(NT_ADDR(6, 2)); vram_put('R' - 0x20);
    vram_adr(NT_ADDR(7, 2)); vram_put('E' - 0x20);


    /* 5-digit score at row 3, right-aligned, suppress leading zeros */
    lead = 0;
    col = 9;

    if (d[4] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[4] != 0) { vram_put(d[4] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[3] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[3] != 0) { vram_put(d[3] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[2] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[2] != 0) { vram_put(d[2] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[1] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[1] != 0) { vram_put(d[1] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    vram_adr(NT_ADDR(col, 2));
    vram_put(d[0] + 0x10);

    /* "BEST" label and high score at row 3 */
    s = high_score;
    d[0] = (unsigned char)(s % 10); s = s / 10;
    d[1] = (unsigned char)(s % 10); s = s / 10;
    d[2] = (unsigned char)(s % 10); s = s / 10;
    d[3] = (unsigned char)(s % 10); s = s / 10;
    d[4] = (unsigned char)(s % 10);

    vram_adr(NT_ADDR(20, 2)); vram_put('B' - 0x20);
    vram_adr(NT_ADDR(21, 2)); vram_put('E' - 0x20);
    vram_adr(NT_ADDR(22, 2)); vram_put('S' - 0x20);
    vram_adr(NT_ADDR(23, 2)); vram_put('T' - 0x20);

    lead = 0;
    col = 25;

    if (d[4] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[4] != 0) { vram_put(d[4] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[3] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[3] != 0) { vram_put(d[3] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[2] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[2] != 0) { vram_put(d[2] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[1] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 2));
    if (lead || d[1] != 0) { vram_put(d[1] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    vram_adr(NT_ADDR(col, 2));
    vram_put(d[0] + 0x10);

    /* 设置最高分为绿色 */
    vram_adr(AT_ADDR(27,     2)); vram_put(0x50);
    vram_adr(AT_ADDR(27 + 4, 2)); vram_put(0x50);

    /* 署名 */
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('F' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('C' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('2' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('0' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('4' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('8' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put(0x0);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('B' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('y' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put(0x0);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('L' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('X' - 0x20);
    vram_adr(NT_ADDR(nameIndex++, 27)); vram_put('S' - 0x20);

    vram_adr(AT_ADDR(27,     27)); vram_put(0x80);
    vram_adr(AT_ADDR(27 + 4, 27)); vram_put(0xa0);
}

static void draw_game_over(void) {
    unsigned char i;
    const unsigned char str[] = {
        'G' - 0x20, 'A' - 0x20, 'M' - 0x20, 'E' - 0x20,
        0x00, /* space */
        'O' - 0x20, 'V' - 0x20, 'E' - 0x20, 'R' - 0x20
    };
    unsigned char x = (32 - 9) / 2;
    vram_adr(NT_ADDR(x, 14));
    for (i = 0; i < 9; i++) {
        vram_put(str[i]);
    }

    /* 设置属性表，使 GAME OVER 区域使用 3 号背景调色板（亮红）。
       该行位于 rowband=3(row 14)，列 11~19 跨 colband 2/3/4（x 为起始列）。 */
    vram_adr(AT_ADDR(x,     14)); vram_put(0xC0);  /* cols 8-11 ：仅右下象限（G） */
    vram_adr(AT_ADDR(x + 4, 14)); vram_put(0xF0);  /* cols 12-15：下方两象限（GAME+空格） */
    vram_adr(AT_ADDR(x + 8, 14)); vram_put(0xF0);  /* cols 16-19：下方两象限（OVER） */
}

void main(void) {
    unsigned char pad;
    unsigned char moved;
    unsigned char prev_pad;
    unsigned char game_over;

    game_over = 0;

    ppu_off();
    pal_bg(palette);
    clear_nametable();

    game_init();
    render_board();
    draw_border();
    draw_score();

    ppu_on_all();

    prev_pad = 0;

    while (1) {
        ppu_wait_nmi();

        pad = pad_poll(0);
        moved = 0;

        if (!game_over && (pad & (pad ^ prev_pad))) {
            if (pad & PAD_UP) {
                if (game_move(DIR_UP)) moved = 1;
            } else if (pad & PAD_DOWN) {
                if (game_move(DIR_DOWN)) moved = 1;
            } else if (pad & PAD_LEFT) {
                if (game_move(DIR_LEFT)) moved = 1;
            } else if (pad & PAD_RIGHT) {
                if (game_move(DIR_RIGHT)) moved = 1;
            }
        }

        if (pad & PAD_START) {
            game_init();
            game_over = 0;
            moved = 2;
        }

        prev_pad = pad;

        if (moved == 1) {
            game_add_random();
            if (!game_can_move()) game_over = 1;
            if (game_score > high_score) high_score = game_score;
            ppu_off();
            render_board();
            draw_score();
            if (game_over) draw_game_over();
            ppu_on_all();
        }

        if (moved == 2) {
            ppu_off();
            render_board();
            draw_border();
            draw_score();
            ppu_on_all();
        }
    }
}