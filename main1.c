#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>

#define WINDOW_WIDTH  450
#define WINDOW_HEIGHT 700
#define BOARD_WIDTH   10
#define BOARD_HEIGHT  20
#define CELL_SIZE     30
#define BOARD_OFFSET_X ((WINDOW_WIDTH - BOARD_WIDTH * CELL_SIZE) / 2)
#define BOARD_OFFSET_Y 50

#define INITIAL_DROP_INTERVAL 800
#define SOFT_DROP_INTERVAL    50
#define MIN_DROP_INTERVAL     100
#define LOCK_DELAY            500
#define LINES_PER_LEVEL       10

#define MAX_BG_PARTICLES      50
#define MAX_DROP_PARTICLES    30
#define DROP_PARTICLE_LIFE    600
#define MAX_FLOAT_TEXTS       5
#define FLOAT_TEXT_DURATION   1500
#define FLASH_DURATION        250


