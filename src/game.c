#include "../neslib/neslib.h"
#include "game.h"

/* Board state: 16 bytes */
unsigned char board[BOARD_SIZE * BOARD_SIZE];

/* Score total */
unsigned int game_score;

/* Temporary line buffer for slide/merge */
static unsigned char line[BOARD_SIZE];

/* Extract a line from the board into temp buffer */
static void extract_line(unsigned char idx, unsigned char dir) {
    unsigned char i;
    unsigned char r, c;

    for (i = 0; i < BOARD_SIZE; i++) {
        switch (dir) {
            case DIR_UP:
                /* extract column idx, top to bottom */
                r = i;
                c = idx;
                break;
            case DIR_DOWN:
                /* extract column idx, bottom to top */
                r = BOARD_SIZE - 1 - i;
                c = idx;
                break;
            case DIR_LEFT:
                /* extract row idx, left to right */
                r = idx;
                c = i;
                break;
            case DIR_RIGHT:
                /* extract row idx, right to left */
                r = idx;
                c = BOARD_SIZE - 1 - i;
                break;
            default:
                r = 0;
                c = i;
                break;
        }
        line[i] = board[r * BOARD_SIZE + c];
    }
}

/* Write processed line back to board */
static void write_line(unsigned char idx, unsigned char dir) {
    unsigned char i;
    unsigned char r, c;

    for (i = 0; i < BOARD_SIZE; i++) {
        switch (dir) {
            case DIR_UP:
                r = i;
                c = idx;
                break;
            case DIR_DOWN:
                r = BOARD_SIZE - 1 - i;
                c = idx;
                break;
            case DIR_LEFT:
                r = idx;
                c = i;
                break;
            case DIR_RIGHT:
                r = idx;
                c = BOARD_SIZE - 1 - i;
                break;
            default:
                r = 0;
                c = i;
                break;
        }
        board[r * BOARD_SIZE + c] = line[i];
    }
}

/* Compact: remove zeros by shifting tiles left */
static void compact(void) {
    unsigned char i;
    unsigned char temp[BOARD_SIZE];
    unsigned char pos;

    pos = 0;
    for (i = 0; i < BOARD_SIZE; i++) {
        if (line[i] != 0) {
            temp[pos++] = line[i];
        }
    }
    for (i = pos; i < BOARD_SIZE; i++) {
        temp[i] = 0;
    }
    for (i = 0; i < BOARD_SIZE; i++) {
        line[i] = temp[i];
    }
}

/* Merge adjacent equal tiles (first pair only) */
static unsigned int merge(void) {
    unsigned char i;
    unsigned int score;

    score = 0;
    for (i = 0; i < BOARD_SIZE - 1; i++) {
        if (line[i] != 0 && line[i] == line[i + 1]) {
            line[i]++;
            score += (unsigned int)(1 << line[i]);
            line[i + 1] = 0;
            i++;
        }
    }
    return score;
}

void game_init(void) {
    unsigned char i;
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        board[i] = TILE_EMPTY;
    }
    game_score = 0;
    game_add_random();
    game_add_random();
}

unsigned char game_move(unsigned char dir) {
    unsigned char i, j;
    unsigned char changed;
    unsigned int total_score;

    total_score = 0;
    changed = 0;
    for (i = 0; i < BOARD_SIZE; i++) {
        unsigned char old_line[BOARD_SIZE];

        extract_line(i, dir);
        for (j = 0; j < BOARD_SIZE; j++) {
            old_line[j] = line[j];
        }

        compact();
        total_score += merge();
        compact();

        for (j = 0; j < BOARD_SIZE; j++) {
            if (line[j] != old_line[j]) changed = 1;
        }

        write_line(i, dir);
    }

    if (changed) {
        game_score += total_score;
    }
    return changed;
}

unsigned char game_empty_count(void) {
    unsigned char i;
    unsigned char count;

    count = 0;
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (board[i] == TILE_EMPTY) {
            count++;
        }
    }
    return count;
}

void game_add_random(void) {
    unsigned char empty;
    unsigned char target;
    unsigned char i;
    unsigned char pos;

    empty = game_empty_count();
    if (empty == 0) return;

    /* Pick random empty cell */
    target = (unsigned char)(rand8() % empty);
    pos = 0;
    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (board[i] == TILE_EMPTY) {
            if (pos == target) {
                /* 90% chance of 2 (tile=1), 10% chance of 4 (tile=2) */
                board[i] = (rand8() % 10 == 0) ? TILE_4 : TILE_2;
                return;
            }
            pos++;
        }
    }
}

unsigned char game_can_move(void) {
    unsigned char i, j;
    unsigned char val, right, down;

    for (i = 0; i < BOARD_SIZE * BOARD_SIZE; i++) {
        if (board[i] == TILE_EMPTY) return 1;

        val = board[i];
        j = i + 1;
        if (j % BOARD_SIZE != 0) {
            right = board[j];
            if (right == val) return 1;
        }
        j = i + BOARD_SIZE;
        if (j < BOARD_SIZE * BOARD_SIZE) {
            down = board[j];
            if (down == val) return 1;
        }
    }
    return 0;
}
