#include "../neslib/neslib.h"
#include "game.h"

#define NT_ADDR(x, y) (0x2000 + (y) * 32 + (x))

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

static const unsigned char palette[32] = {
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
    0x0F, 0x00, 0x10, 0x30,
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

    /* 署名 */
    vram_adr(NT_ADDR(16, 27)); vram_put('F' - 0x20);
    vram_adr(NT_ADDR(17, 27)); vram_put('C' - 0x20);
    vram_adr(NT_ADDR(18, 27)); vram_put('2' - 0x20);
    vram_adr(NT_ADDR(19, 27)); vram_put('0' - 0x20);
    vram_adr(NT_ADDR(20, 27)); vram_put('4' - 0x20);
    vram_adr(NT_ADDR(21, 27)); vram_put('8' - 0x20);
    vram_adr(NT_ADDR(23, 27)); vram_put('B' - 0x20);
    vram_adr(NT_ADDR(24, 27)); vram_put('y' - 0x20);
    vram_adr(NT_ADDR(26, 27)); vram_put('L' - 0x20);
    vram_adr(NT_ADDR(27, 27)); vram_put('X' - 0x20);
    vram_adr(NT_ADDR(28, 27)); vram_put('S' - 0x20);

    /* 5-digit score at row 3, right-aligned, suppress leading zeros */
    lead = 0;
    col = 10;

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

    vram_adr(NT_ADDR(3, 3)); vram_put('B' - 0x20);
    vram_adr(NT_ADDR(4, 3)); vram_put('E' - 0x20);
    vram_adr(NT_ADDR(5, 3)); vram_put('S' - 0x20);
    vram_adr(NT_ADDR(6, 3)); vram_put('T' - 0x20);

    lead = 0;
    col = 10;

    if (d[4] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 3));
    if (lead || d[4] != 0) { vram_put(d[4] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[3] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 3));
    if (lead || d[3] != 0) { vram_put(d[3] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[2] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 3));
    if (lead || d[2] != 0) { vram_put(d[2] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    if (d[1] != 0) lead = 1;
    vram_adr(NT_ADDR(col, 3));
    if (lead || d[1] != 0) { vram_put(d[1] + 0x10); }
    else { vram_put(TILE_EMPTY); }
    col++;

    vram_adr(NT_ADDR(col, 3));
    vram_put(d[0] + 0x10);
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