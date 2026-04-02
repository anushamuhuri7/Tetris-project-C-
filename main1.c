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

void piece_bag_shuffle(Game *game) {
    for (int i = 0; i < NUM_PIECE_TYPES; i++) game->bag[i] = i;
    for (int i = NUM_PIECE_TYPES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = game->bag[i];
        game->bag[i] = game->bag[j];
        game->bag[j] = tmp;
    }
    game->bag_index = 0;
}

Piece piece_get_next(Game *game) {
    if (game->bag_index >= NUM_PIECE_TYPES) piece_bag_shuffle(game);
    Piece p = { .type = game->bag[game->bag_index++], .rotation = 0, .x = BOARD_WIDTH/2 - 2, .y = 0 };
    return p;
}

bool check_collision(Game *game, Piece *p, int dx, int dy, int dr) {
    int next_rot = (p->rotation + dr + 4) % 4;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shapes[p->type][next_rot][i][j]) {
                int nx = p->x + j + dx;
                int ny = p->y + i + dy;
                if (nx < 0 || nx >= BOARD_WIDTH || ny >= BOARD_HEIGHT) return true;
                if (ny >= 0 && game->board[ny][nx] != 0) return true;
            }
        }
    }
    return false;
}

void spawn_float_text(Game *game, const char *text, float x, float y) {
    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        if (!game->float_texts[i].active) {
            FloatText *ft = &game->float_texts[i];
            ft->active = true;
            strncpy(ft->text, text, 31);
            ft->x = x; ft->y = y; ft->alpha = 1.0f;
            ft->start_time = SDL_GetTicks();
            return;
        }
    }
}

void game_spawn_next(Game *game) {
    game->current = game->next;
    game->next = piece_get_next(game);
    game->last_drop_time = SDL_GetTicks();
    game->lock_timer_active = false;
    if (check_collision(game, &game->current, 0, 0, 0)) game->state = STATE_GAME_OVER;
}

void clear_lines(Game *game) {
    int lines[4], count = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) if (game->board[y][x] == 0) { full = false; break; }
        if (full) {
            lines[count++] = y;
            if (count == 4) break;
        }
    }
    if (count > 0) {
        game->flash_count = count;
        memcpy(game->flash_rows, lines, sizeof(lines));
        game->flash_start_time = SDL_GetTicks();
        
        int base = (count == 1) ? 100 : (count == 2) ? 300 : (count == 3) ? 500 : 800;
        game->score += base * game->level;
        game->lines_cleared += count;
        game->level = (game->lines_cleared / LINES_PER_LEVEL) + 1;
        game->drop_interval = SDL_max(MIN_DROP_INTERVAL, INITIAL_DROP_INTERVAL - (game->level * 50));
        
        if (count == 4) spawn_float_text(game, "TETRIS!", WINDOW_WIDTH/2, WINDOW_HEIGHT/2);
    } else {
        game_spawn_next(game);
    }
}

void finalize_clear(Game *game) {
    for (int i = 0; i < game->flash_count; i++) {
        int row = game->flash_rows[i];
        for (int y = row; y > 0; y--)
            for (int x = 0; x < BOARD_WIDTH; x++) game->board[y][x] = game->board[y-1][x];
        for (int x = 0; x < BOARD_WIDTH; x++) game->board[0][x] = 0;
        for (int j = i + 1; j < game->flash_count; j++) if (game->flash_rows[j] < row) game->flash_rows[j]++;
    }
    game->flash_count = 0;
    game_spawn_next(game);
}

void lock_piece(Game *game) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shapes[game->current.type][game->current.rotation][i][j]) {
                int nx = game->current.x + j;
                int ny = game->current.y + i;
                if (ny >= 0 && ny < BOARD_HEIGHT) game->board[ny][nx] = game->current.type + 1;
            }
        }
    }
    clear_lines(game);
}


