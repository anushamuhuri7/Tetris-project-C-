#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>

// Game Constants
#define COLS 10
#define ROWS 20
#define CELL_SIZE 30
#define SCREEN_WIDTH (COLS * CELL_SIZE + 200)
#define SCREEN_HEIGHT (ROWS * CELL_SIZE)

// Tetromino definitions (7 shapes, 4 rotations, 4x4 grid)
const int SHAPES[7][4][16] = {
    {{0,0,0,0, 1,1,1,1, 0,0,0,0, 0,0,0,0}, {0,0,1,0, 0,0,1,0, 0,0,1,0, 0,0,1,0}, {0,0,0,0, 0,0,0,0, 1,1,1,1, 0,0,0,0}, {0,1,0,0, 0,1,0,0, 0,1,0,0, 0,1,0,0}}, // I
    {{0,0,0,0, 0,1,1,0, 0,1,1,0, 0,0,0,0}, {0,0,0,0, 0,1,1,0, 0,1,1,0, 0,0,0,0}, {0,0,0,0, 0,1,1,0, 0,1,1,0, 0,0,0,0}, {0,0,0,0, 0,1,1,0, 0,1,1,0, 0,0,0,0}}, // O
    {{0,1,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0}, {0,1,0,0, 0,1,1,0, 0,1,0,0, 0,0,0,0}, {0,0,0,0, 1,1,1,0, 0,1,0,0, 0,0,0,0}, {0,1,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0}}, // T
    {{0,1,1,0, 1,1,0,0, 0,0,0,0, 0,0,0,0}, {0,1,0,0, 0,1,1,0, 0,0,1,0, 0,0,0,0}, {0,0,0,0, 0,1,1,0, 1,1,0,0, 0,0,0,0}, {1,0,0,0, 1,1,0,0, 0,1,0,0, 0,0,0,0}}, // S
    {{1,1,0,0, 0,1,1,0, 0,0,0,0, 0,0,0,0}, {0,0,1,0, 0,1,1,0, 0,1,0,0, 0,0,0,0}, {0,0,0,0, 1,1,0,0, 0,1,1,0, 0,0,0,0}, {0,1,0,0, 1,1,0,0, 1,0,0,0, 0,0,0,0}}, // Z
    {{1,0,0,0, 1,1,1,0, 0,0,0,0, 0,0,0,0}, {0,1,1,0, 0,1,0,0, 0,1,0,0, 0,0,0,0}, {0,0,0,0, 1,1,1,0, 0,0,1,0, 0,0,0,0}, {0,1,0,0, 0,1,0,0, 1,1,0,0, 0,0,0,0}}, // J
    {{0,0,1,0, 1,1,1,0, 0,0,0,0, 0,0,0,0}, {0,1,0,0, 0,1,0,0, 0,1,1,0, 0,0,0,0}, {0,0,0,0, 1,1,1,0, 1,0,0,0, 0,0,0,0}, {1,1,0,0, 0,1,0,0, 0,1,0,0, 0,0,0,0}}  // L
};

typedef struct { Uint8 r, g, b; } Color;
Color SHAPE_COLORS[] = {
    {128, 128, 128}, {0, 255, 255}, {255, 255, 0}, {128, 0, 128}, 
    {0, 255, 0}, {255, 0, 0}, {0, 0, 255}, {255, 165, 0}
};

// Fixed: Using Uint32 to prevent overflow warnings for 20-bit font data
const Uint32 FONT_NUMS[10] = {
    0xF999F, 0x22222, 0xF1F8F, 0xF1F1F, 0x99F11, 0xF8F1F, 0xF8F9F, 0xF1111, 0xF9F9F, 0xF9F1F
};

// Bitmaps for "PRESS R TO RETRY" (Simplified 4x5 characters)
const Uint32 FONT_P = 0xF9F88;
const Uint32 FONT_R = 0xF9F99;
const Uint32 FONT_E = 0xF8F8F;
const Uint32 FONT_S = 0xF8F1F;
const Uint32 FONT_T = 0xF4444;
const Uint32 FONT_O = 0xF999F;
const Uint32 FONT_Y = 0x99F44;

typedef struct { int type, rotation, x, y; } Piece;

int board[ROWS][COLS] = {0};
Piece currentPiece;
int score = 0;
bool gameOver = false;

void SpawnPiece() {
    currentPiece.type = rand() % 7;
    currentPiece.rotation = 0;
    currentPiece.x = COLS / 2 - 2;
    currentPiece.y = 0;
}

void ResetGame() {
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            board[y][x] = 0;
        }
    }
    score = 0;
    gameOver = false;
    SpawnPiece();
}

bool CheckCollision(int tx, int ty, int tr) {
    for (int i = 0; i < 16; i++) {
        if (SHAPES[currentPiece.type][tr][i]) {
            int px = tx + (i % 4);
            int py = ty + (i / 4);
            if (px < 0 || px >= COLS || py >= ROWS) return true;
            if (py >= 0 && board[py][px]) return true;
        }
    }
    return false;
}

void LockPiece() {
    for (int i = 0; i < 16; i++) {
        if (SHAPES[currentPiece.type][currentPiece.rotation][i]) {
            int px = currentPiece.x + (i % 4);
            int py = currentPiece.y + (i / 4);
            if (py >= 0) board[py][px] = currentPiece.type + 1;
        }
    }
}

void ClearLines() {
    int linesCleared = 0;
    for (int y = ROWS - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < COLS; x++) if (!board[y][x]) { full = false; break; }
        if (full) {
            linesCleared++;
            for (int yy = y; yy > 0; yy--)
                for (int x = 0; x < COLS; x++) board[yy][x] = board[yy - 1][x];
            for (int x = 0; x < COLS; x++) board[0][x] = 0;
            y++;
        }
    }
    if (linesCleared > 0) score += linesCleared * 100;
}

void DrawBlock(SDL_Renderer* renderer, int x, int y, Color color) {
    SDL_Rect rect = { x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE - 1, CELL_SIZE - 1 };
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &rect);
}

// Fixed digit renderer for 4x5 font
void DrawGlyph(SDL_Renderer* renderer, Uint32 data, int x, int y, int size, Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int i = 0; i < 20; i++) {
        if (data & (1 << (19 - i))) {
            SDL_Rect r = { x + (i % 4) * size, y + (i / 4) * size, size, size };
            SDL_RenderFillRect(renderer, &r);
        }
    }
}

void DrawDigit(SDL_Renderer* renderer, int digit, int x, int y, int size, Color color) {
    DrawGlyph(renderer, FONT_NUMS[digit % 10], x, y, size, color);
}

void DrawScore(SDL_Renderer* renderer, int val, int x, int y, int size, Color color) {
    char buf[16];
    sprintf(buf, "%d", val);
    for (int i = 0; buf[i] != '\0'; i++) {
        DrawDigit(renderer, buf[i] - '0', x + i * (size * 5), y, size, color);
    }
}

void DrawUISidebar(SDL_Renderer* renderer) {
    SDL_Rect sidebarRect = { COLS * CELL_SIZE, 0, 200, SCREEN_HEIGHT };
    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
    SDL_RenderFillRect(renderer, &sidebarRect);

    // Draw Score Header
    SDL_Rect header = { COLS * CELL_SIZE + 20, 20, 160, 5 };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderFillRect(renderer, &header);

    // Draw Numerical Score
    DrawScore(renderer, score, COLS * CELL_SIZE + 30, 40, 6, (Color){255, 255, 255});
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* window = SDL_CreateWindow("SDL2 Tetris", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    
    srand(time(NULL));
    ResetGame();

    bool running = true;
    Uint32 lastTime = SDL_GetTicks();
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN) {
                if (!gameOver) {
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:  if (!CheckCollision(currentPiece.x - 1, currentPiece.y, currentPiece.rotation)) currentPiece.x--; break;
                        case SDLK_RIGHT: if (!CheckCollision(currentPiece.x + 1, currentPiece.y, currentPiece.rotation)) currentPiece.x++; break;
                        case SDLK_UP: {
                            int nextRot = (currentPiece.rotation + 1) % 4;
                            if (!CheckCollision(currentPiece.x, currentPiece.y, nextRot)) currentPiece.rotation = nextRot;
                        } break;
                        case SDLK_DOWN: {
                            int nextRot = (currentPiece.rotation + 3) % 4;
                            if (!CheckCollision(currentPiece.x, currentPiece.y, nextRot)) currentPiece.rotation = nextRot;
                        } break;
                    }
                } else if (event.key.keysym.sym == SDLK_r) {
                    ResetGame();
                }
            }
        }

        Uint32 currentTime = SDL_GetTicks();
        int dropSpeed = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_SPACE] ? 40 : 400;
        
        if (currentTime - lastTime > (Uint32)dropSpeed && !gameOver) {
            if (!CheckCollision(currentPiece.x, currentPiece.y + 1, currentPiece.rotation)) {
                currentPiece.y++;
            } else {
                LockPiece();
                ClearLines();
                SpawnPiece();
                if (CheckCollision(currentPiece.x, currentPiece.y, currentPiece.rotation)) gameOver = true;
            }
            lastTime = currentTime;
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        // Grid
        SDL_SetRenderDrawColor(renderer, 35, 35, 35, 255);
        for(int x=0; x<=COLS; x++) SDL_RenderDrawLine(renderer, x*CELL_SIZE, 0, x*CELL_SIZE, SCREEN_HEIGHT);

        for (int y = 0; y < ROWS; y++)
            for (int x = 0; x < COLS; x++)
                if (board[y][x]) DrawBlock(renderer, x, y, SHAPE_COLORS[board[y][x]]);

        if (!gameOver) {
            for (int i = 0; i < 16; i++)
                if (SHAPES[currentPiece.type][currentPiece.rotation][i])
                    DrawBlock(renderer, currentPiece.x + (i % 4), currentPiece.y + (i / 4), SHAPE_COLORS[currentPiece.type + 1]);
        }

        DrawUISidebar(renderer);
        
        if (gameOver) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
            SDL_Rect overlay = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
            SDL_RenderFillRect(renderer, &overlay);
            
            Color white = {255, 255, 255};
            int centerX = SCREEN_WIDTH / 2;
            int centerY = SCREEN_HEIGHT / 2;

            // Display Final Score
            DrawScore(renderer, score, centerX - 40, centerY - 60, 8, white);

            // Display "PRESS R TO RETRY"
            // P R E S S
            int ty = centerY + 20;
            int tx = centerX - 90;
            DrawGlyph(renderer, FONT_P, tx,      ty, 3, white);
            DrawGlyph(renderer, FONT_R, tx + 15, ty, 3, white);
            DrawGlyph(renderer, FONT_E, tx + 30, ty, 3, white);
            DrawGlyph(renderer, FONT_S, tx + 45, ty, 3, white);
            DrawGlyph(renderer, FONT_S, tx + 60, ty, 3, white);

            // R
            DrawGlyph(renderer, FONT_R, tx + 85, ty, 3, white);

            // T O
            DrawGlyph(renderer, FONT_T, tx + 110, ty, 3, white);
            DrawGlyph(renderer, FONT_O, tx + 125, ty, 3, white);

            // R E T R Y
            DrawGlyph(renderer, FONT_R, tx + 150, ty, 3, white);
            DrawGlyph(renderer, FONT_E, tx + 165, ty, 3, white);
            DrawGlyph(renderer, FONT_T, tx + 180, ty, 3, white);
            DrawGlyph(renderer, FONT_R, tx + 195, ty, 3, white);
            DrawGlyph(renderer, FONT_Y, tx + 210, ty, 3, white);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
