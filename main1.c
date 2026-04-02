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

typedef enum { PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L, NUM_PIECE_TYPES } PieceType;
typedef enum { STATE_MENU, STATE_PLAYING, STATE_GAME_OVER } GameState;

typedef struct {
    PieceType type;
    int rotation;
    int x, y;
} Piece;

typedef struct {
    float x, y, vy, size, alpha;
    bool active;
} BGParticle;

typedef struct {
    char text[32];
    float x, y, alpha;
    Uint32 start_time;
    bool active;
} FloatText;

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font;
    TTF_Font     *font_large;    
    GameState     state;
    bool          running;
    int           board[BOARD_HEIGHT][BOARD_WIDTH];
    Piece         current;
    Piece         next;
    int           bag[NUM_PIECE_TYPES];
    int           bag_index;
    int           score;
    int           level;
    int           lines_cleared;
    Uint32        last_drop_time;
    int           drop_interval;
    bool          soft_dropping;
    Uint32        lock_timer;
    bool          lock_timer_active;
    BGParticle    bg_particles[MAX_BG_PARTICLES];
    FloatText     float_texts[MAX_FLOAT_TEXTS];
    int           flash_rows[4];
    int           flash_count;
    Uint32        flash_start_time;
} Game;

static const int shapes[NUM_PIECE_TYPES][4][4][4] = {
    [PIECE_I] = {
        {{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}},
        {{0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}
    },
    [PIECE_O] = {
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}
    },
    [PIECE_T] = {
        {{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    [PIECE_S] = {
        {{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0}},
        {{1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    },
    [PIECE_Z] = {
        {{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0}}
    },
    [PIECE_J] = {
        {{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}}
    },
    [PIECE_L] = {
        {{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}},
        {{0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0}},
        {{0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0}},
        {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}
    }
};

static const SDL_Color piece_colors[NUM_PIECE_TYPES] = {
    { 90, 210, 220, 255 }, { 240, 210,  80, 255 }, { 175, 120, 220, 255 },
    { 100, 210, 130, 255 }, { 240, 100, 100, 255 }, { 90, 140, 240, 255 },
    { 245, 165,  80, 255 }
};

