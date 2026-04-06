#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define WINDOW_WIDTH  600
#define WINDOW_HEIGHT 700
#define BOARD_WIDTH   10
#define BOARD_HEIGHT  20
#define CELL_SIZE     30
#define BOARD_OFFSET_X 50
#define BOARD_OFFSET_Y 60
#define SIDEBAR_X      (BOARD_OFFSET_X + BOARD_WIDTH * CELL_SIZE + 40)
#define INITIAL_DROP_INTERVAL 800
#define MIN_DROP_INTERVAL     100
#define HIGHSCORE_FILE        "highscore.dat"

typedef enum { PIECE_I, PIECE_O, PIECE_T, PIECE_S, PIECE_Z, PIECE_J, PIECE_L, NUM_PIECE_TYPES } PieceType;
typedef enum { STATE_PLAYING, STATE_GAME_OVER, STATE_PAUSED, STATE_MENU } GameState;

typedef struct {
    PieceType type;
    int rotation;
    int x, y;
} Piece;
typedef struct {
    int cells[BOARD_HEIGHT + 4][BOARD_WIDTH];
} Board;
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_label;
    TTF_Font     *font_score;
    TTF_Font     *font_title;

    GameState     state;
    bool          running;

    Board         board;
    Piece         current;
    Piece         next;

    int           bag[NUM_PIECE_TYPES];
    int           bag_index;

    int           score;
    int           high_score;
    int           level;
    int           lines_cleared;

    Uint32        last_drop_time;
    int           drop_interval;
    bool          soft_dropping;
} Game;

static const int shapes[NUM_PIECE_TYPES][4][4][4] = {
    [PIECE_I] = {{{0,0,0,0}, {1,1,1,1}, {0,0,0,0}, {0,0,0,0}}, {{0,0,1,0}, {0,0,1,0}, {0,0,1,0}, {0,0,1,0}}, {{0,0,0,0}, {0,0,0,0}, {1,1,1,1}, {0,0,0,0}}, {{0,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,1,0,0}}},
    [PIECE_O] = {{{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,1,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}},
    [PIECE_T] = {{{0,1,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,0,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}}, {{0,0,0,0}, {1,1,1,0}, {0,1,0,0}, {0,0,0,0}}, {{0,1,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}},
    [PIECE_S] = {{{0,1,1,0}, {1,1,0,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,0,0}, {0,1,1,0}, {0,0,1,0}, {0,0,0,0}}, {{0,0,0,0}, {0,1,1,0}, {1,1,0,0}, {0,0,0,0}}, {{1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,0,0,0}}},
    [PIECE_Z] = {{{1,1,0,0}, {0,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,0,1,0}, {0,1,1,0}, {0,1,0,0}, {0,0,0,0}}, {{0,0,0,0}, {1,1,0,0}, {0,1,1,0}, {0,0,0,0}}, {{0,1,0,0}, {1,1,0,0}, {1,0,0,0}, {0,0,0,0}}},
    [PIECE_J] = {{{1,0,0,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,1,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}, {{0,0,0,0}, {1,1,1,0}, {0,0,1,0}, {0,0,0,0}}, {{0,1,0,0}, {0,1,0,0}, {1,1,0,0}, {0,0,0,0}}},
    [PIECE_L] = {{{0,0,1,0}, {1,1,1,0}, {0,0,0,0}, {0,0,0,0}}, {{0,1,0,0}, {0,1,0,0}, {0,1,1,0}, {0,0,0,0}}, {{0,0,0,0}, {1,1,1,0}, {1,0,0,0}, {0,0,0,0}}, {{1,1,0,0}, {0,1,0,0}, {0,1,0,0}, {0,0,0,0}}}
};

static const SDL_Color piece_colors[NUM_PIECE_TYPES] = {
    {0, 122, 255, 255}, {255, 159, 10, 255}, {175, 82, 222, 255},
    {48, 209, 88, 255}, {255, 59, 48, 255}, {10, 132, 255, 255}, {255, 149, 0, 255}
};

void save_high_score(int score) {
    FILE *f = fopen(HIGHSCORE_FILE, "wb");
    if (f) {
        fwrite(&score, sizeof(int), 1, f);
        fclose(f);
    }
}

int load_high_score() {
    int score = 0;
    FILE *f = fopen(HIGHSCORE_FILE, "rb");
    if (f) {
        fread(&score, sizeof(int), 1, f);
        fclose(f);
    }
    return score;
}

void piece_bag_init(Game *game) {
    for (int i = 0; i < NUM_PIECE_TYPES; i++) game->bag[i] = i;
    for (int i = NUM_PIECE_TYPES - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int tmp = game->bag[i]; game->bag[i] = game->bag[j]; game->bag[j] = tmp;
    }
    game->bag_index = 0;
}

Piece piece_get_next(Game *game) {
    if (game->bag_index >= NUM_PIECE_TYPES) piece_bag_init(game);
    Piece p = { .type = game->bag[game->bag_index++], .rotation = 0, .x = BOARD_WIDTH/2 - 2, .y = 0 };
    return p;
}

bool board_piece_fits(Board *b, Piece *p) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (!shapes[p->type][p->rotation][r][c]) continue;
            int bx = p->x + c, by = p->y + r;
            if (bx < 0 || bx >= BOARD_WIDTH || by >= BOARD_HEIGHT + 4) return false;
            if (by >= 0 && b->cells[by][bx]) return false;
        }
    }
    return true;
}

void game_spawn_next(Game *game) {
    game->current = game->next;
    game->next = piece_get_next(game);
    game->last_drop_time = SDL_GetTicks();
    if (!board_piece_fits(&game->board, &game->current)) {
        game->state = STATE_GAME_OVER;
        if (game->score > game->high_score) {
            game->high_score = game->score;
            save_high_score(game->high_score);
        }
    }
}

void game_reset(Game *game) {
    memset(&game->board, 0, sizeof(Board));
    game->score = 0;
    game->level = 1;
    game->lines_cleared = 0;
    game->drop_interval = INITIAL_DROP_INTERVAL;
    game->high_score = load_high_score();
    piece_bag_init(game);
    game->next = piece_get_next(game);
    game_spawn_next(game);
    game->state = STATE_PLAYING;
}

void clear_lines(Game *game) {
    int count = 0;
    for (int y = BOARD_HEIGHT + 3; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_WIDTH; x++) { if (game->board.cells[y][x] == 0) { full = false; break; } }
        if (full) {
            count++;
            for (int r = y; r > 0; r--) memcpy(game->board.cells[r], game->board.cells[r-1], sizeof(int)*BOARD_WIDTH);
            memset(game->board.cells[0], 0, sizeof(int)*BOARD_WIDTH);
            y++;
        }
    }

    if (count > 0) {
        game->score += (count * 100);
        game->lines_cleared += count;
        game->level = (game->lines_cleared / 3) + 1;
        if (game->score > game->high_score) {
            game->high_score = game->score;
            save_high_score(game->high_score);
        }
        game->drop_interval = fmax(MIN_DROP_INTERVAL, INITIAL_DROP_INTERVAL - (game->level - 1) * 60);
    }
    game_spawn_next(game);
}

void lock_piece(Game *game) {
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            if (shapes[game->current.type][game->current.rotation][r][c]) {
                int by = game->current.y + r, bx = game->current.x + c;
                if (by >= 0) game->board.cells[by][bx] = game->current.type + 1;
            }
        }
    }
    clear_lines(game);
}

void draw_text(SDL_Renderer *r, TTF_Font *f, const char *text, int x, int y, SDL_Color color) {
    if (!f || !text[0]) return;
    SDL_Surface *s = TTF_RenderText_Blended(f, text, color);
    SDL_Texture *t = SDL_CreateTextureFromSurface(r, s);
    SDL_Rect d = { x, y, s->w, s->h };
    SDL_RenderCopy(r, t, NULL, &d);
    SDL_FreeSurface(s); SDL_DestroyTexture(t);
}

void render_frame(Game *game) {
    SDL_Renderer *r = game->renderer;
    SDL_SetRenderDrawColor(r, 242, 242, 247, 255); SDL_RenderClear(r);

  
    SDL_SetRenderDrawColor(r, 200, 200, 200, 255);
    SDL_Rect b_rect = { BOARD_OFFSET_X - 2, BOARD_OFFSET_Y - 2, BOARD_WIDTH * CELL_SIZE + 4, BOARD_HEIGHT * CELL_SIZE + 4 };
    SDL_RenderDrawRect(r, &b_rect);
    for (int y = 4; y < BOARD_HEIGHT + 4; y++) {
        for (int x = 0; x < BOARD_WIDTH; x++) {
            if (game->board.cells[y][x]) {
                SDL_Color c = piece_colors[game->board.cells[y][x] - 1];
                SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
                SDL_Rect cell = { BOARD_OFFSET_X + x * CELL_SIZE, BOARD_OFFSET_Y + (y - 4) * CELL_SIZE, CELL_SIZE - 1, CELL_SIZE - 1 };
                SDL_RenderFillRect(r, &cell);
            }
        }
    }

    if (game->state == STATE_PLAYING) {
        SDL_Color c = piece_colors[game->current.type];
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255);
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (shapes[game->current.type][game->current.rotation][i][j]) {
                    int py = game->current.y + i - 4;
                    if (py >= 0) {
                        SDL_Rect cell = { BOARD_OFFSET_X + (game->current.x + j) * CELL_SIZE, BOARD_OFFSET_Y + py * CELL_SIZE, CELL_SIZE - 1, CELL_SIZE - 1 };
                        SDL_RenderFillRect(r, &cell);
                    }
                }
            }
        }
    }

    SDL_Color gray = {142, 142, 147, 255}, dark = {28, 28, 30, 255}, blue = {0, 122, 255, 255};
    char buf[32];

    draw_text(r, game->font_label, "SCORE", SIDEBAR_X, 60, gray);
    sprintf(buf, "%d", game->score); draw_text(r, game->font_score, buf, SIDEBAR_X, 85, dark);

    draw_text(r, game->font_label, "HIGH SCORE", SIDEBAR_X, 150, gray);
    sprintf(buf, "%d", game->high_score); draw_text(r, game->font_score, buf, SIDEBAR_X, 175, blue);

    draw_text(r, game->font_label, "NEXT", SIDEBAR_X, 260, gray);
    SDL_Rect next_card = { SIDEBAR_X, 290, 120, 120 };
    SDL_SetRenderDrawColor(r, 255, 255, 255, 255); SDL_RenderFillRect(r, &next_card);
    SDL_SetRenderDrawColor(r, 230, 230, 235, 255); SDL_RenderDrawRect(r, &next_card);

    if (game->state != STATE_MENU) {
        SDL_Color next_c = piece_colors[game->next.type];
        SDL_SetRenderDrawColor(r, next_c.r, next_c.g, next_c.b, 255);
        int ps = 25, ox = SIDEBAR_X + 10, oy = 300;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                if (shapes[game->next.type][0][i][j]) {
                    SDL_Rect cell = { ox + j * ps, oy + i * ps, ps - 1, ps - 1 };
                    SDL_RenderFillRect(r, &cell);
                }
            }
        }
    }

    draw_text(r, game->font_label, "LEVEL", SIDEBAR_X, 450, gray);
    sprintf(buf, "%d", game->level); draw_text(r, game->font_score, buf, SIDEBAR_X, 475, dark);

    if (game->state == STATE_GAME_OVER) {
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 180);
        SDL_Rect full = { 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT }; SDL_RenderFillRect(r, &full);
        draw_text(r, game->font_title, "GAME OVER", 180, 280, (SDL_Color){255, 59, 48, 255});
        draw_text(r, game->font_label, "PRESS 'R' TO RETRY", 210, 360, (SDL_Color){255, 255, 255, 255});
    }

    SDL_RenderPresent(r);
}

int main(int argc, char *argv[]) {
    srand(time(NULL)); SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER); TTF_Init();
    Game game = {0};
    game.window = SDL_CreateWindow("Tetris Pro", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    game.renderer = SDL_CreateRenderer(game.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    game.font_label = TTF_OpenFont("assets/font.ttf", 18);
    game.font_score = TTF_OpenFont("assets/font.ttf", 28);
    game.font_title = TTF_OpenFont("assets/font.ttf", 48);

    game_reset(&game);
    game.running = true;

    SDL_Event e;
    while (game.running) {
        Uint32 now = SDL_GetTicks();
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) game.running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_r) game_reset(&game);
                if (game.state == STATE_PLAYING) {
                    Piece next_pos = game.current;
                    switch (e.key.keysym.sym) {
                        case SDLK_LEFT:  next_pos.x--; if (board_piece_fits(&game.board, &next_pos)) game.current.x--; break;
                        case SDLK_RIGHT: next_pos.x++; if (board_piece_fits(&game.board, &next_pos)) game.current.x++; break;
                        case SDLK_UP:    next_pos.rotation = (next_pos.rotation + 1) % 4; if (board_piece_fits(&game.board, &next_pos)) game.current.rotation = next_pos.rotation; break;
                        case SDLK_DOWN:  game.soft_dropping = true; break;
                        case SDLK_SPACE: while(board_piece_fits(&game.board, &next_pos)) { next_pos.y++; } game.current.y = next_pos.y - 1; lock_piece(&game); break;
                    }
                }
            }
            if (e.type == SDL_KEYUP && e.key.keysym.sym == SDLK_DOWN) game.soft_dropping = false;
        }

        if (game.state == STATE_PLAYING) {
            int interval = game.soft_dropping ? 50 : game.drop_interval;
            if (now - game.last_drop_time > (Uint32)interval) {
                Piece down = game.current; down.y++;
                if (board_piece_fits(&game.board, &down)) { game.current.y++; game.last_drop_time = now; }
                else { lock_piece(&game); }
            }
        }
        render_frame(&game);
        SDL_Delay(16);
    }
    TTF_Quit(); SDL_Quit();
    return 0;
}
