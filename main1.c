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

/* ===========================================================================
 * LOGIC FUNCTIONS
 * =========================================================================== */

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

void game_init(Game *game) {
    memset(game, 0, sizeof(Game));
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    TTF_Init();
    game->window = SDL_CreateWindow("Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    game->renderer = SDL_CreateRenderer(game->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    game->font = TTF_OpenFont("assets/font.ttf", 18);
    game->font_large = TTF_OpenFont("assets/font.ttf", 32);
    
    srand(time(NULL));
    piece_bag_shuffle(game);
    game->next = piece_get_next(game);
    game->drop_interval = INITIAL_DROP_INTERVAL;
    game->state = STATE_PLAYING;
    game->running = true;
    game->level = 1;
    game_spawn_next(game);

    for (int i = 0; i < MAX_BG_PARTICLES; i++) {
        game->bg_particles[i].active = true;
        game->bg_particles[i].x = rand() % WINDOW_WIDTH;
        game->bg_particles[i].y = rand() % WINDOW_HEIGHT;
        game->bg_particles[i].vy = 20 + rand() % 40;
        game->bg_particles[i].size = 1 + rand() % 3;
        game->bg_particles[i].alpha = 0.1f + (rand() % 20) / 100.0f;
    }
}

void game_update(Game *game, float dt) {
    if (game->state != STATE_PLAYING) return;
    
    for (int i = 0; i < MAX_BG_PARTICLES; i++) {
        game->bg_particles[i].y += game->bg_particles[i].vy * dt;
        if (game->bg_particles[i].y > WINDOW_HEIGHT) {
            game->bg_particles[i].y = -10;
            game->bg_particles[i].x = rand() % WINDOW_WIDTH;
        }
    }

    for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
        if (!game->float_texts[i].active) continue;
        game->float_texts[i].y -= 30 * dt;
        Uint32 age = SDL_GetTicks() - game->float_texts[i].start_time;
        if (age > FLOAT_TEXT_DURATION) game->float_texts[i].active = false;
        else game->float_texts[i].alpha = 1.0f - (float)age / FLOAT_TEXT_DURATION;
    }

    if (game->flash_count > 0) {
        if (SDL_GetTicks() - game->flash_start_time > FLASH_DURATION) finalize_clear(game);
        return;
    }

    Uint32 now = SDL_GetTicks();
    int interval = game->soft_dropping ? SOFT_DROP_INTERVAL : game->drop_interval;
    if (now - game->last_drop_time > (Uint32)interval) {
        if (!check_collision(game, &game->current, 0, 1, 0)) {
            game->current.y++;
            game->last_drop_time = now;
            game->lock_timer_active = false;
        } else {
            if (!game->lock_timer_active) {
                game->lock_timer = now;
                game->lock_timer_active = true;
            } else if (now - game->lock_timer > LOCK_DELAY) {
                lock_piece(game);
            }
        }
    }
}

void render_block(SDL_Renderer *r, int x, int y, SDL_Color c, bool ghost) {
    SDL_Rect rect = { x, y, CELL_SIZE - 1, CELL_SIZE - 1 };
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, ghost ? 50 : 255);
    SDL_RenderFillRect(r, &rect);
    if (!ghost) {
        SDL_SetRenderDrawColor(r, 255, 255, 255, 40);
        SDL_RenderDrawRect(r, &rect);
    }
}

void game_render(Game *game) {
    SDL_SetRenderDrawColor(game->renderer, 15, 15, 20, 255);
    SDL_RenderClear(game->renderer);

    for (int i = 0; i < MAX_BG_PARTICLES; i++) {
        SDL_SetRenderDrawColor(game->renderer, 200, 200, 255, (Uint8)(game->bg_particles[i].alpha * 255));
        SDL_Rect p = {(int)game->bg_particles[i].x, (int)game->bg_particles[i].y, (int)game->bg_particles[i].size, (int)game->bg_particles[i].size};
        SDL_RenderFillRect(game->renderer, &p);
    }

    SDL_SetRenderDrawColor(game->renderer, 30, 30, 40, 255);
    SDL_Rect board_rect = { BOARD_OFFSET_X, BOARD_OFFSET_Y, BOARD_WIDTH * CELL_SIZE, BOARD_HEIGHT * CELL_SIZE };
    SDL_RenderFillRect(game->renderer, &board_rect);

    for (int y = 0; y < BOARD_HEIGHT; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game->board[y][x]) {
                render_block(game->renderer, BOARD_OFFSET_X + x * CELL_SIZE, BOARD_OFFSET_Y + y * CELL_SIZE, piece_colors[game->board[y][x]-1], false);
            }
        }
    }

    Piece ghost = game->current;
    while (!check_collision(game, &ghost, 0, 1, 0)) ghost.y++;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (shapes[ghost.type][ghost.rotation][i][j]) {
                render_block(game->renderer, BOARD_OFFSET_X + (ghost.x + j) * CELL_SIZE, BOARD_OFFSET_Y + (ghost.y + i) * CELL_SIZE, piece_colors[ghost.type], true);
            }
        }
    }

    if (game->flash_count == 0) {
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (shapes[game->current.type][game->current.rotation][i][j]) {
                    render_block(game->renderer, BOARD_OFFSET_X + (game->current.x + j) * CELL_SIZE, BOARD_OFFSET_Y + (game->current.y + i) * CELL_SIZE, piece_colors[game->current.type], false);
                }
            }
        }
    }

    if (game->font) {
        for (int i = 0; i < MAX_FLOAT_TEXTS; i++) {
            if (!game->float_texts[i].active) continue;
            SDL_Color tc = {255, 255, 255, (Uint8)(game->float_texts[i].alpha * 255)};
            SDL_Surface *s = TTF_RenderText_Blended(game->font, game->float_texts[i].text, tc);
            if (s) {
                SDL_Texture *t = SDL_CreateTextureFromSurface(game->renderer, s);
                SDL_Rect r = {(int)game->float_texts[i].x - s->w/2, (int)game->float_texts[i].y, s->w, s->h};
                SDL_RenderCopy(game->renderer, t, NULL, &r);
                SDL_FreeSurface(s); SDL_DestroyTexture(t);
            }
        }

        char buf[64];
        sprintf(buf, "SCORE: %d   LEVEL: %d", game->score, game->level);
        SDL_Surface *ss = TTF_RenderText_Blended(game->font, buf, (SDL_Color){255,255,255,255});
        if (ss) {
            SDL_Texture *st = SDL_CreateTextureFromSurface(game->renderer, ss);
            SDL_Rect sr = { BOARD_OFFSET_X, 15, ss->w, ss->h };
            SDL_RenderCopy(game->renderer, st, NULL, &sr);
            SDL_FreeSurface(ss); SDL_DestroyTexture(st);
        }
    }

    if (game->state == STATE_GAME_OVER && game->font_large) {
        SDL_Surface *go = TTF_RenderText_Blended(game->font_large, "GAME OVER", (SDL_Color){255,50,50,255});
        if (go) {
            SDL_Texture *got = SDL_CreateTextureFromSurface(game->renderer, go);
            SDL_Rect gor = { WINDOW_WIDTH/2 - go->w/2, WINDOW_HEIGHT/2 - go->h/2, go->w, go->h };
            SDL_RenderCopy(game->renderer, got, NULL, &gor);
            SDL_FreeSurface(go); SDL_DestroyTexture(got);
        }
    }

    SDL_RenderPresent(game->renderer);
}

int main(int argc, char *argv[]) {
    Game game;
    game_init(&game);
    SDL_Event e;
    Uint32 last_time = SDL_GetTicks();

    while (game.running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) game.running = false;
            if (e.type == SDL_KEYDOWN) {
                if (game.state == STATE_PLAYING) {
                    switch (e.key.keysym.sym) {
                        case SDLK_LEFT:  if (!check_collision(&game, &game.current, -1, 0, 0)) game.current.x--; break;
                        case SDLK_RIGHT: if (!check_collision(&game, &game.current, 1, 0, 0))  game.current.x++; break;
                        case SDLK_DOWN:  game.soft_dropping = true; break;
                        case SDLK_UP:    if (!check_collision(&game, &game.current, 0, 0, 1))  game.current.rotation = (game.current.rotation + 1) % 4; break;
                        case SDLK_SPACE: while(!check_collision(&game, &game.current, 0, 1, 0)) game.current.y++; lock_piece(&game); break;
                        case SDLK_ESCAPE: game.running = false; break;
                    }
                } else if (game.state == STATE_GAME_OVER) {
                    if (e.key.keysym.sym == SDLK_r) {
                        GameState old_state = game.state;
                        game_init(&game);
                    }
                }
            }
            if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_DOWN) game.soft_dropping = false;
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - last_time) / 1000.0f;
        last_time = now;

        game_update(&game, dt);
        game_render(&game);
        SDL_Delay(1);
    }

    if (game.font) TTF_CloseFont(game.font);
    if (game.font_large) TTF_CloseFont(game.font_large);
    SDL_DestroyRenderer(game.renderer);
    SDL_DestroyWindow(game.window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
