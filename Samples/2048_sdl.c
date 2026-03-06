#pragma lib "SDL2"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <SDL2/SDL.h>

// 2048 game with SDL2
// Arrow keys to slide tiles. R to restart, Q/Escape to quit.

enum { N = 4, TILE = 120, PAD = 12 };
enum { WIDTH = N * TILE + (N + 1) * PAD, HEIGHT = N * TILE + (N + 1) * PAD + 60 };

int board[N][N];
int score;
int best;
int won;    // reached 2048
int over;   // no moves left

void place_random(void);
void draw(SDL_Renderer* ren);
int can_move(void);
void slide(int dr, int dc);

// Tile colors: index = log2(value)
// bg colors (r,g,b) for 2,4,8,16,32,64,128,256,512,1024,2048
unsigned char tile_r[] = {0xee,0xed,0xf2,0xf5,0xf6,0xf6,0xed,0xed,0xed,0xed,0xed};
unsigned char tile_g[] = {0xe4,0xe0,0xb1,0x89,0x7d,0x65,0xcf,0xcc,0xc8,0xc4,0xc0};
unsigned char tile_b[] = {0xda,0xc8,0x79,0x4d,0x3f,0x3b,0x72,0x63,0x53,0x41,0x2e};
// text dark for 2,4; white for the rest
int text_dark[] = {1,1,0,0,0,0,0,0,0,0,0};

int log2i(int v){
    int r = 0;
    while(v > 1){ v >>= 1; r++; }
    return r;
}

// Digit bitmaps (5 rows x 3 cols each)
const char* digits[10] = {
    "xxx"
    "x.x"
    "x.x"
    "x.x"
    "xxx",  // 0

    ".x."
    "xx."
    ".x."
    ".x."
    "xxx",  // 1

    "xxx"
    "..x"
    "xxx"
    "x.."
    "xxx",  // 2

    "xxx"
    "..x"
    "xxx"
    "..x"
    "xxx",  // 3

    "x.x"
    "x.x"
    "xxx"
    "..x"
    "..x",  // 4

    "xxx"
    "x.."
    "xxx"
    "..x"
    "xxx",  // 5

    "xxx"
    "x.."
    "xxx"
    "x.x"
    "xxx",  // 6

    "xxx"
    "..x"
    "..x"
    "..x"
    "..x",  // 7

    "xxx"
    "x.x"
    "xxx"
    "x.x"
    "xxx",  // 8

    "xxx"
    "x.x"
    "xxx"
    "..x"
    "xxx",  // 9
};

// Letter bitmaps (5 rows x 3 cols) for banner text
const char* letters[26] = {
    ".x."
    "x.x"
    "xxx"
    "x.x"
    "x.x",  // A
    "xx."
    "x.x"
    "xx."
    "x.x"
    "xx.",  // B
    "xxx"
    "x.."
    "x.."
    "x.."
    "xxx",  // C
    "xx."
    "x.x"
    "x.x"
    "x.x"
    "xx.",  // D
    "xxx"
    "x.."
    "xx."
    "x.."
    "xxx",  // E
    "xxx"
    "x.."
    "xx."
    "x.."
    "x..",  // F
    "xxx"
    "x.."
    "x.x"
    "x.x"
    "xxx",  // G
    "x.x"
    "x.x"
    "xxx"
    "x.x"
    "x.x",  // H
    "xxx"
    ".x."
    ".x."
    ".x."
    "xxx",  // I
    "..x"
    "..x"
    "..x"
    "x.x"
    "xxx",  // J
    "x.x"
    "xx."
    "x.."
    "xx."
    "x.x",  // K
    "x.."
    "x.."
    "x.."
    "x.."
    "xxx",  // L
    "x.x"
    "xxx"
    "xxx"
    "x.x"
    "x.x",  // M
    "x.x"
    "xxx"
    "xxx"
    "x.x"
    "x.x",  // N
    "xxx"
    "x.x"
    "x.x"
    "x.x"
    "xxx",  // O
    "xxx"
    "x.x"
    "xxx"
    "x.."
    "x..",  // P
    "xxx"
    "x.x"
    "x.x"
    "xxx"
    "..x",  // Q
    "xxx"
    "x.x"
    "xx."
    "x.x"
    "x.x",  // R
    "xxx"
    "x.."
    "xxx"
    "..x"
    "xxx",  // S
    "xxx"
    ".x."
    ".x."
    ".x."
    ".x.",  // T
    "x.x"
    "x.x"
    "x.x"
    "x.x"
    "xxx",  // U
    "x.x"
    "x.x"
    "x.x"
    "x.x"
    ".x.",  // V
    "x.x"
    "x.x"
    "xxx"
    "xxx"
    "x.x",  // W
    "x.x"
    "x.x"
    ".x."
    "x.x"
    "x.x",  // X
    "x.x"
    "x.x"
    ".x."
    ".x."
    ".x.",  // Y
    "xxx"
    "..x"
    ".x."
    "x.."
    "xxx",  // Z
};

void draw_char(SDL_Renderer* ren, const char* bm, int x, int y, int sz){
    for(int r = 0; r < 5; r++){
        for(int c = 0; c < 3; c++){
            if(bm[r * 3 + c] == 'x'){
                SDL_Rect rc = {x + c * sz, y + r * sz, sz - 1, sz - 1};
                SDL_RenderFillRect(ren, &rc);
            }
        }
    }
}

void draw_text(SDL_Renderer* ren, const char* text, int cx, int cy, int sz){
    int len = (int)strlen(text);
    int charw = 3 * sz + sz;
    int totalw = len * charw - sz;
    int x = cx - totalw / 2;
    int y = cy - (5 * sz) / 2;
    for(int i = 0; i < len; i++){
        char ch = text[i];
        const char* bm = NULL;
        if(ch >= 'A' && ch <= 'Z') bm = letters[ch - 'A'];
        else if(ch >= 'a' && ch <= 'z') bm = letters[ch - 'a'];
        else if(ch >= '0' && ch <= '9') bm = digits[ch - '0'];
        if(bm) draw_char(ren, bm, x + i * charw, y, sz);
    }
}

void draw_digit(SDL_Renderer* ren, int d, int x, int y, int sz){
    const char* bm = digits[d];
    for(int r = 0; r < 5; r++){
        for(int c = 0; c < 3; c++){
            if(bm[r * 3 + c] == 'x'){
                SDL_Rect rc = {x + c * sz, y + r * sz, sz - 1, sz - 1};
                SDL_RenderFillRect(ren, &rc);
            }
        }
    }
}

void draw_number(SDL_Renderer* ren, int num, int cx, int cy, int sz){
    // Render number centered at (cx, cy)
    char buf[12];
    SDL_snprintf(buf, sizeof buf, "%d", num);
    int len = (int)SDL_strlen(buf);
    int charw = 3 * sz + sz; // 3 pixel cols + 1 gap
    int totalw = len * charw - sz;
    int x = cx - totalw / 2;
    int y = cy - (5 * sz) / 2;
    for(int i = 0; i < len; i++){
        draw_digit(ren, buf[i] - '0', x + i * charw, y, sz);
    }
}

void reset(void){
    SDL_memset(board, 0, sizeof board);
    score = 0;
    won = 0;
    over = 0;
    place_random();
    place_random();
}

void place_random(void){
    int empty[N * N];
    int ne = 0;
    for(int r = 0; r < N; r++)
        for(int c = 0; c < N; c++)
            if(!board[r][c]) empty[ne++] = r * N + c;
    if(!ne) return;
    int idx = empty[rand() % ne];
    board[idx / N][idx % N] = (rand() % 10 < 9) ? 2 : 4;
}

void slide(int dr, int dc){
    int moved = 0;
    // Process rows/cols in the right order
    for(int i = 0; i < N; i++){
        int vals[N];
        int nv = 0;
        // Extract non-zero values in slide direction
        for(int j = 0; j < N; j++){
            int r = dr ? (dr > 0 ? N - 1 - j : j) : i;
            int c = dc ? (dc > 0 ? N - 1 - j : j) : i;
            if(board[r][c]) vals[nv++] = board[r][c];
        }
        // Merge adjacent equal tiles
        int merged[N];
        int nm = 0;
        for(int k = 0; k < nv; k++){
            if(k + 1 < nv && vals[k] == vals[k + 1]){
                merged[nm++] = vals[k] * 2;
                score += vals[k] * 2;
                if(vals[k] * 2 == 2048) won = 1;
                k++;
            }
            else {
                merged[nm++] = vals[k];
            }
        }
        // Write back
        for(int j = 0; j < N; j++){
            int r = dr ? (dr > 0 ? N - 1 - j : j) : i;
            int c = dc ? (dc > 0 ? N - 1 - j : j) : i;
            int newval = j < nm ? merged[j] : 0;
            if(board[r][c] != newval) moved = 1;
            board[r][c] = newval;
        }
    }
    if(moved){
        place_random();
        if(!can_move()) over = 1;
    }
}

int can_move(void){
    for(int r = 0; r < N; r++)
        for(int c = 0; c < N; c++){
            if(!board[r][c]) return 1;
            if(c + 1 < N && board[r][c] == board[r][c + 1]) return 1;
            if(r + 1 < N && board[r][c] == board[r + 1][c]) return 1;
        }
    return 0;
}

void draw(SDL_Renderer* ren){
    // Background
    SDL_SetRenderDrawColor(ren, 0xfa, 0xf8, 0xef, 0xff);
    SDL_RenderClear(ren);

    // Score area
    int top = 10;
    // "SCORE" label area
    SDL_SetRenderDrawColor(ren, 0xbb, 0xad, 0xa0, 0xff);
    SDL_Rect sr = {PAD, top, 100, 40};
    SDL_RenderFillRect(ren, &sr);
    SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
    draw_number(ren, score, PAD + 50, top + 20, 3);

    // Best
    SDL_SetRenderDrawColor(ren, 0xbb, 0xad, 0xa0, 0xff);
    SDL_Rect br = {PAD + 110, top, 100, 40};
    SDL_RenderFillRect(ren, &br);
    SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
    draw_number(ren, best, PAD + 160, top + 20, 3);

    int offy = 60;
    // Board background
    SDL_SetRenderDrawColor(ren, 0xbb, 0xad, 0xa0, 0xff);
    SDL_Rect bgr = {0, offy, WIDTH, HEIGHT - offy};
    SDL_RenderFillRect(ren, &bgr);

    for(int r = 0; r < N; r++){
        for(int c = 0; c < N; c++){
            int x = PAD + c * (TILE + PAD);
            int y = offy + PAD + r * (TILE + PAD);
            int v = board[r][c];
            if(!v){
                // Empty cell
                SDL_SetRenderDrawColor(ren, 0xcd, 0xc1, 0xb4, 0xff);
            }
            else {
                int idx = log2i(v) - 1;
                if(idx < 0) idx = 0;
                if(idx > 10) idx = 10;
                SDL_SetRenderDrawColor(ren, tile_r[idx], tile_g[idx], tile_b[idx], 0xff);
            }
            SDL_Rect tr = {x, y, TILE, TILE};
            SDL_RenderFillRect(ren, &tr);

            if(v){
                int idx = log2i(v) - 1;
                if(idx < 0) idx = 0;
                if(idx > 10) idx = 10;
                if(text_dark[idx])
                    SDL_SetRenderDrawColor(ren, 0x77, 0x6e, 0x65, 0xff);
                else
                    SDL_SetRenderDrawColor(ren, 0xf9, 0xf6, 0xf2, 0xff);
                int sz = v < 100 ? 6 : (v < 1000 ? 5 : 4);
                draw_number(ren, v, x + TILE / 2, y + TILE / 2, sz);
            }
        }
    }

    // Game over / win overlay
    if(over || won){
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        if(won){
            SDL_SetRenderDrawColor(ren, 0xed, 0xc2, 0x2e, 0x99);
        }
        else {
            SDL_SetRenderDrawColor(ren, 0xee, 0xe4, 0xda, 0x99);
        }
        SDL_Rect ovr = {0, offy, WIDTH, HEIGHT - offy};
        SDL_RenderFillRect(ren, &ovr);
        int cy = offy + (HEIGHT - offy) / 2;
        if(won){
            SDL_SetRenderDrawColor(ren, 0xf9, 0xf6, 0xf2, 0xff);
            draw_text(ren, "YOU", WIDTH / 2, cy - 30, 7);
            draw_text(ren, "WIN", WIDTH / 2, cy + 30, 7);
        }
        else {
            SDL_SetRenderDrawColor(ren, 0x77, 0x6e, 0x65, 0xff);
            draw_text(ren, "GAME", WIDTH / 2, cy - 30, 7);
            draw_text(ren, "OVER", WIDTH / 2, cy + 30, 7);
        }
    }
}

srand((unsigned)time(NULL));

SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("2048",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

reset();

_Bool running = 1;
while(running){
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        switch(ev.type){
        case SDL_QUIT:
            running = 0;
            break;
        case SDL_KEYDOWN:
            switch(ev.key.keysym.sym){
            case SDLK_ESCAPE: case SDLK_q:
                running = 0;
                break;
            case SDLK_r:
                if(score > best) best = score;
                reset();
                break;
            case SDLK_UP:    if(!over) slide(-1, 0); break;
            case SDLK_DOWN:  if(!over) slide(1, 0);  break;
            case SDLK_LEFT:  if(!over) slide(0, -1); break;
            case SDLK_RIGHT: if(!over) slide(0, 1);  break;
            }
            break;
        }
    }
    draw(ren);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
}

if(score > best) best = score;
SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
SDL_Quit();
