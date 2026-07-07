#ifndef GAME_H
#define GAME_H

/* Board: 4x4, values are exponents (0=empty, 1=2, 2=4, ..., 11=2048) */
#define BOARD_SIZE 4
#define TILE_EMPTY 0
#define TILE_2     1
#define TILE_4     2
#define TILE_8     3
#define TILE_16    4
#define TILE_32    5
#define TILE_64    6
#define TILE_128   7
#define TILE_256   8
#define TILE_512   9
#define TILE_1024  10
#define TILE_2048  11
#define TILE_MAX   11

/* Direction constants */
#define DIR_UP    0
#define DIR_DOWN  1
#define DIR_LEFT  2
#define DIR_RIGHT 3

/* Board state: 16 bytes, one per cell */
extern unsigned char board[BOARD_SIZE * BOARD_SIZE];

/* Score: accumulates merged tile values */
extern unsigned int game_score;

/* Initialize board with two random tiles */
void game_init(void);

/* Slide board in given direction, merge tiles, return score gained */
unsigned char game_move(unsigned char dir);

/* Check if any move is possible */
unsigned char game_can_move(void);

/* Count empty cells */
unsigned char game_empty_count(void);

/* Add random tile (2 or 4) at random empty cell */
void game_add_random(void);

#endif
