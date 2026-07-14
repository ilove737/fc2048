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

/* Board position: 4×4 grid, each cell = 4×4 tiles (32×32 px).
   BOARD_Y=8 ensures cell-tile-Y is a multiple of 4 → each cell maps to
   exactly one attribute table byte. */
#define BOARD_X ((32 - BOARD_TILES) / 2)
#define BOARD_Y ((30 - BOARD_TILES + 2) / 2)  /* 8 */

/* Border tile indices (all in pattern table 0, 0-255) */
#define TILE_TL      192
#define TILE_TR      193
#define TILE_BL      194
#define TILE_BR      195
#define TILE_HLINE   196
#define TILE_VLINE   197

/* Shared block background (tiles 96-103) */
#define BLOCK_BG     96

/* Digit tiles base (tiles 104, each value = 8 tiles) */
#define DIGIT_BASE   104

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
    0x0F, 0x2C, 0x10, 0x30,   /* pal0 边框+空块 */
    0x0F, 0x27, 0x10, 0x30,   /* pal1 值 2,4    ：橙黄底 */
    0x0F, 0x16, 0x17, 0x30,   /* pal2 值 8,16,32：红橙底 */
    0x0F, 0x06, 0x15, 0x30,   /* pal3 值 64+    ：棕红底/亮红字 */
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
};

/* Value → palette index for cell attribute table */
static unsigned char val_palette(unsigned char val) {
    if (val == 0) return 0;
    if (val <= 2) return 1;  /* 2, 4 */
    if (val <= 5) return 2;  /* 8, 16, 32 */
    return 3;                 /* 64+ */
}

/* Write a 4×4 cell: shared background + value-specific digit tiles */
static void draw_cell(unsigned char sx, unsigned char sy, unsigned char val) {
    unsigned char r, c, base, p, attr;
    if (val == 0) {
        for (r = 0; r < 4; r++) {
            vram_adr(NT_ADDR(sx, sy + r));
            vram_fill(TILE_EMPTY, 4);
        }
        vram_adr(AT_ADDR(sx, sy));
        vram_put(0);
        return;
    }
    base = DIGIT_BASE + (unsigned char)(val - 1) * 8;
    /* Row 0: bg[0..3] */
    vram_adr(NT_ADDR(sx, sy));
    for (c = 0; c < 4; c++) vram_put(BLOCK_BG + c);
    /* Row 1: digit[0..3] */
    vram_adr(NT_ADDR(sx, sy + 1));
    for (c = 0; c < 4; c++) vram_put(base + c);
    /* Row 2: digit[4..7] */
    vram_adr(NT_ADDR(sx, sy + 2));
    for (c = 0; c < 4; c++) vram_put(base + 4 + c);
    /* Row 3: bg[4..7] */
    vram_adr(NT_ADDR(sx, sy + 3));
    for (c = 0; c < 4; c++) vram_put(BLOCK_BG + 4 + c);
    /* Attribute */
    p = val_palette(val);
    attr = p | (p << 2) | (p << 4) | (p << 6);
    vram_adr(AT_ADDR(sx, sy));
    vram_put(attr);
}

/* neslib set_vram_update */
#define UPD_MAX 300
static unsigned char upd[UPD_MAX];
static unsigned char *upd_ptr;

/* Append cell update to buffer: 4 horizontal sequences + attribute */
static void upd_cell(unsigned char sx, unsigned char sy, unsigned char val) {
    unsigned char r, c, base, p, attr;
    unsigned int adr, tiles[4];

    if (val == 0) {
        for (r = 0; r < 4; r++) {
            adr = NT_ADDR(sx, sy + r);
            *upd_ptr++ = (unsigned char)((adr >> 8) | NT_UPD_HORZ);
            *upd_ptr++ = (unsigned char)(adr & 0xff);
            *upd_ptr++ = 4;
            for (c = 0; c < 4; c++) *upd_ptr++ = TILE_EMPTY;
        }
        adr = AT_ADDR(sx, sy);
        *upd_ptr++ = (unsigned char)(adr >> 8);
        *upd_ptr++ = (unsigned char)(adr & 0xff);
        *upd_ptr++ = 0;
        return;
    }
    base = DIGIT_BASE + (unsigned char)(val - 1) * 8;
    for (r = 0; r < 4; r++) {
        adr = NT_ADDR(sx, sy + r);
        *upd_ptr++ = (unsigned char)((adr >> 8) | NT_UPD_HORZ);
        *upd_ptr++ = (unsigned char)(adr & 0xff);
        *upd_ptr++ = 4;
        if (r == 0) {
            tiles[0]=BLOCK_BG; tiles[1]=BLOCK_BG+1; tiles[2]=BLOCK_BG+2; tiles[3]=BLOCK_BG+3;
        } else if (r == 1) {
            tiles[0]=base; tiles[1]=base+1; tiles[2]=base+2; tiles[3]=base+3;
        } else if (r == 2) {
            tiles[0]=base+4; tiles[1]=base+5; tiles[2]=base+6; tiles[3]=base+7;
        } else {
            tiles[0]=BLOCK_BG+4; tiles[1]=BLOCK_BG+5; tiles[2]=BLOCK_BG+6; tiles[3]=BLOCK_BG+7;
        }
        for (c = 0; c < 4; c++) *upd_ptr++ = tiles[c];
    }
    p = val_palette(val);
    attr = p | (p << 2) | (p << 4) | (p << 6);
    adr = AT_ADDR(sx, sy);
    *upd_ptr++ = (unsigned char)(adr >> 8);
    *upd_ptr++ = (unsigned char)(adr & 0xff);
    *upd_ptr++ = attr;
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

/* 增量重绘状态：记录上一帧已绘制的棋盘/分数，移动时只更新变化部分 */
static unsigned char prev_board[BOARD_SIZE * BOARD_SIZE];
static unsigned int  prev_score;
static unsigned int  prev_high;
static unsigned char prev_game_over;

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

/* 向更新列表追加 5 位分数（第 2 行，从 start_col 起），复用 draw_score 的前导零逻辑 */
static void upd_digits(unsigned char start_col, unsigned int value) {
    unsigned char d[5];
    unsigned char lead = 0;
    unsigned char col = start_col;
    unsigned char k;
    unsigned int adr;

    d[0] = (unsigned char)(value % 10); value = value / 10;
    d[1] = (unsigned char)(value % 10); value = value / 10;
    d[2] = (unsigned char)(value % 10); value = value / 10;
    d[3] = (unsigned char)(value % 10); value = value / 10;
    d[4] = (unsigned char)(value % 10);

    adr = NT_ADDR(col, 2);
    *upd_ptr++ = (unsigned char)((adr >> 8) | NT_UPD_HORZ);
    *upd_ptr++ = (unsigned char)(adr & 0xff);
    *upd_ptr++ = 5;
    for (k = 4; k >= 1; k--) {
        if (lead || d[k] != 0) { *upd_ptr++ = d[k] + 0x10; lead = 1; }
        else { *upd_ptr++ = TILE_EMPTY; }
    }
    *upd_ptr++ = d[0] + 0x10;
}

/* 向更新列表追加 GAME OVER 文字与属性表（仅在首次出现时调用） */
static void upd_game_over(void) {
    static const unsigned char str[] = {
        'G' - 0x20, 'A' - 0x20, 'M' - 0x20, 'E' - 0x20,
        0x00, 0x00,
        'O' - 0x20, 'V' - 0x20, 'E' - 0x20, 'R' - 0x20
    };
    unsigned char x = (32 - 10) / 2;
    unsigned char i;
    unsigned int adr;

    adr = NT_ADDR(x, 6);
    *upd_ptr++ = (unsigned char)((adr >> 8) | NT_UPD_HORZ);
    *upd_ptr++ = (unsigned char)(adr & 0xff);
    *upd_ptr++ = 10;
    for (i = 0; i < 10; i++) *upd_ptr++ = str[i];

    /* 属性表：非连续单字节写 */
    adr = AT_ADDR(x,     6);
    *upd_ptr++ = (unsigned char)(adr >> 8); *upd_ptr++ = (unsigned char)(adr & 0xff); *upd_ptr++ = 0x0C;
    adr = AT_ADDR(x + 4, 6);
    *upd_ptr++ = (unsigned char)(adr >> 8); *upd_ptr++ = (unsigned char)(adr & 0xff); *upd_ptr++ = 0x0F;
    adr = AT_ADDR(x + 8, 6);
    *upd_ptr++ = (unsigned char)(adr >> 8); *upd_ptr++ = (unsigned char)(adr & 0xff); *upd_ptr++ = 0x0F;
}

void main(void) {
    unsigned char pad;
    unsigned char moved;
    unsigned char prev_pad;
    unsigned char game_over;
    unsigned char i;

    game_over = 0;

    ppu_off();
    pal_bg(palette);
    clear_nametable();

    game_init();
    render_board();
    draw_border();
    draw_score();

    ppu_on_all();

    /* 初始化增量重绘状态：记录首屏，并把更新列表置空（不每帧重写） */
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) prev_board[i] = board[i];
    prev_score = 0;
    prev_high = 0;
    prev_game_over = 0;
    upd[0] = 0xff;
    set_vram_update(upd);

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
        }

        /* 增量更新：每帧至多写 2 格（每格 4 水平序列+属性≈708 周期），
           超出部分分摊到后续帧以避免 VBlank 溢出
           （未写入的格子因 prev_board 未更新，下一帧会继续补上） */
        {
            unsigned char cells_written = 0;
            upd_ptr = upd;
            for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
                if (board[i] != prev_board[i]) {
                    unsigned char r = (unsigned char)(i / BOARD_SIZE);
                    unsigned char c = (unsigned char)(i % BOARD_SIZE);
                    if (cells_written >= 2) break;
                    upd_cell(BOARD_X + c * CELL_TILES, BOARD_Y + r * CELL_TILES, board[i]);
                    prev_board[i] = board[i];
                    cells_written++;
                }
            }
            if (game_score != prev_score) { upd_digits(9, game_score);  prev_score = game_score; }
            if (high_score != prev_high)  { upd_digits(25, high_score); prev_high  = high_score; }
            if (game_over && !prev_game_over) { upd_game_over(); prev_game_over = 1; }
            *upd_ptr++ = 0xff;
            set_vram_update(upd);
        }

        if (moved == 2) {
            ppu_off();
            render_board();
            draw_border();
            draw_score();
            /* 清空旧更新列表，避免 ppu_on_all 的 NMI 冲掉新画面 */
            upd[0] = 0xff;
            set_vram_update(upd);
            ppu_on_all();

            /* 重开后刷新增量状态 */
            for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) prev_board[i] = board[i];
            prev_score = 0;
            prev_high = high_score;
            prev_game_over = 0;
        }
    }
}