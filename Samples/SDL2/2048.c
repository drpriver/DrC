#pragma lib "SDL2"
#include <SDL2/SDL.h>
enum { N = 4, TILE = 120, PAD = 12 };
enum { WIDTH = N * TILE + (N + 1) * PAD, HEIGHT = N * TILE + (N + 1) * PAD + 60 };

int board[N][N];
int score, best, won, over;
SDL_Window* window; SDL_Renderer* renderer;
void place_random(void);
void draw(void);
_Bool can_move(void);
void slide(int dr, int dc);
unsigned rng(void);

unsigned char tile_r[] = {0xee,0xed,0xf2,0xf5,0xf6,0xf6,0xed,0xed,0xed,0xed,0xed};
unsigned char tile_g[] = {0xe4,0xe0,0xb1,0x89,0x7d,0x65,0xcf,0xcc,0xc8,0xc4,0xc0};
unsigned char tile_b[] = {0xda,0xc8,0x79,0x4d,0x3f,0x3b,0x72,0x63,0x53,0x41,0x2e};
int text_dark[] = {1,1,0,0,0,0,0,0,0,0,0};
int log2i(int v){
    int r = 0;
    while(v > 1){ v >>= 1; r++; }
    return r;
}
unsigned short digits[10] = {
    0b111101101101111, // 0
    0b010110010010111, // 1
    0b111001111100111, // 2
    0b111001111001111, // 3
    0b101101111001001, // 4
    0b111100111001111, // 5
    0b111100111101111, // 6
    0b111001001001001, // 7
    0b111101111101111, // 8
    0b111101111001111, // 9
};
unsigned short letters[26] = {
    0b010101111101101, // A
    0b110101110101110, // B
    0b111100100100111, // C
    0b110101101101110, // D
    0b111100110100111, // E
    0b111100110100100, // F
    0b111100101101111, // G
    0b101101111101101, // H
    0b111010010010111, // I
    0b001001001101111, // J
    0b101110100110101, // K
    0b100100100100111, // L
    0b101111111101101, // M
    0b101111111101101, // N
    0b111101101101111, // O
    0b111101111100100, // P
    0b111101101111001, // Q
    0b111101110101101, // R
    0b111100111001111, // S
    0b111010010010010, // T
    0b101101101101111, // U
    0b101101101101010, // V
    0b101101111111101, // W
    0b101101010101101, // X
    0b101101010010010, // Y
    0b111001010100111, // Z
};

void draw_char(unsigned short bm, int x, int y, int sz){
    for(int r = 0; r < 5; r++)
        for(int c = 0; c < 3; c++)
            if((bm >> (14 - r * 3 - c)) & 1){
                SDL_Rect rc = {x + c * sz, y + r * sz, sz - 1, sz - 1};
                SDL_RenderFillRect(renderer, &rc);
            }
}

void draw_text(const char* text, int cx, int cy, int sz){
    int len = (int)SDL_strlen(text);
    int charw = 3 * sz + sz;
    int totalw = len * charw - sz;
    int x = cx - totalw / 2;
    int y = cy - (5 * sz) / 2;
    for(int i = 0; i < len; i++){
        char ch = text[i];
        unsigned short bm = 0;
        if(ch >= 'A' && ch <= 'Z') bm = letters[ch - 'A'];
        else if(ch >= 'a' && ch <= 'z') bm = letters[ch - 'a'];
        else if(ch >= '0' && ch <= '9') bm = digits[ch - '0'];
        if(bm) draw_char(bm, x + i * charw, y, sz);
    }
}

void draw_number(int num, int cx, int cy, int sz){
    char buf[12];
    SDL_snprintf(buf, sizeof buf, "%d", num);
    int len = (int)SDL_strlen(buf);
    int charw = 3 * sz + sz;
    int totalw = len * charw - sz;
    int x = cx - totalw / 2;
    int y = cy - (5 * sz) / 2;
    for(int i = 0; i < len; i++){
        draw_char(digits[buf[i] - '0'], x + i * charw, y, sz);
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
    int idx = empty[rng() % ne];
    board[idx / N][idx % N] = (rng() % 10 < 9) ? 2 : 4;
}

void slide(int dr, int dc){
    int moved = 0;
    for(int i = 0; i < N; i++){
        int vals[N];
        int n = 0;
        for(int j = 0; j < N; j++){
            int r = dr ? (dr > 0 ? N - 1 - j : j) : i;
            int c = dc ? (dc > 0 ? N - 1 - j : j) : i;
            if(board[r][c]) vals[n++] = board[r][c];
        }
        // Merge adjacent equal tiles
        int merged[N];
        int mn = 0;
        for(int k = 0; k < n; k++){
            if(k + 1 < n && vals[k] == vals[k + 1]){
                merged[mn++] = vals[k] * 2;
                score += vals[k] * 2;
                if(vals[k] * 2 == 2048) won = 1;
                k++;
            }
            else {
                merged[mn++] = vals[k];
            }
        }
        for(int j = 0; j < N; j++){
            int r = dr ? (dr > 0 ? N - 1 - j : j) : i;
            int c = dc ? (dc > 0 ? N - 1 - j : j) : i;
            int newval = j < mn ? merged[j] : 0;
            if(board[r][c] != newval) moved = 1;
            board[r][c] = newval;
        }
    }
    if(moved){
        place_random();
        if(!can_move()) over = 1;
    }
}

_Bool can_move(void){
    for(int r = 0; r < N; r++)
        for(int c = 0; c < N; c++){
            if(!board[r][c]) return 1;
            if(c + 1 < N && board[r][c] == board[r][c + 1]) return 1;
            if(r + 1 < N && board[r][c] == board[r + 1][c]) return 1;
        }
    return 0;
}

void draw(void){
    SDL_SetRenderDrawColor(renderer, 0xfa, 0xf8, 0xef, 0xff);
    SDL_RenderClear(renderer);
    SDL_Rect r;

    // Score area
    int top = 10;
    SDL_SetRenderDrawColor(renderer, 0xbb, 0xad, 0xa0, 0xff);
    r = {PAD, top, 100, 40};
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    draw_number(score, PAD + 50, top + 20, 3);

    // Best
    SDL_SetRenderDrawColor(renderer, 0xbb, 0xad, 0xa0, 0xff);
    r = {PAD + 110, top, 100, 40};
    SDL_RenderFillRect(renderer, &r);
    SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);
    draw_number(best, PAD + 160, top + 20, 3);

    int offy = 60;
    // Board background
    SDL_SetRenderDrawColor(renderer, 0xbb, 0xad, 0xa0, 0xff);
    r = {0, offy, WIDTH, HEIGHT - offy};
    SDL_RenderFillRect(renderer, &r);

    for(int r = 0; r < N; r++){
        for(int c = 0; c < N; c++){
            int x = PAD + c * (TILE + PAD);
            int y = offy + PAD + r * (TILE + PAD);
            int v = board[r][c];
            if(!v){
                SDL_SetRenderDrawColor(renderer, 0xcd, 0xc1, 0xb4, 0xff);
            }
            else {
                int idx = log2i(v) - 1;
                if(idx < 0) idx = 0;
                if(idx > 10) idx = 10;
                SDL_SetRenderDrawColor(renderer, tile_r[idx], tile_g[idx], tile_b[idx], 0xff);
            }
            SDL_Rect rec = {x, y, TILE, TILE};
            SDL_RenderFillRect(renderer, &rec);
            if(v){
                int idx = log2i(v) - 1;
                if(idx < 0) idx = 0;
                if(idx > 10) idx = 10;
                if(text_dark[idx])
                    SDL_SetRenderDrawColor(renderer, 0x77, 0x6e, 0x65, 0xff);
                else
                    SDL_SetRenderDrawColor(renderer, 0xf9, 0xf6, 0xf2, 0xff);
                int sz = v < 100 ? 6 : (v < 1000 ? 5 : 4);
                draw_number(v, x + TILE / 2, y + TILE / 2, sz);
            }
        }
    }
    if(over || won){
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        if(won)
            SDL_SetRenderDrawColor(renderer, 0xed, 0xc2, 0x2e, 0x99);
        else
            SDL_SetRenderDrawColor(renderer, 0xee, 0xe4, 0xda, 0x99);
        SDL_Rect r = {0, offy, WIDTH, HEIGHT - offy};
        SDL_RenderFillRect(renderer, &r);
        int cy = offy + (HEIGHT - offy) / 2;
        if(won){
            SDL_SetRenderDrawColor(renderer, 0xf9, 0xf6, 0xf2, 0xff);
            draw_text("YOU", WIDTH / 2, cy - 30, 7);
            draw_text("WIN", WIDTH / 2, cy + 30, 7);
        }
        else {
            SDL_SetRenderDrawColor(renderer, 0x77, 0x6e, 0x65, 0xff);
            draw_text("GAME", WIDTH / 2, cy - 30, 7);
            draw_text("OVER", WIDTH / 2, cy + 30, 7);
        }
    }
}
unsigned rng_state = rng_state = (unsigned)SDL_GetPerformanceCounter();
unsigned rng(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

SDL_Init(SDL_INIT_VIDEO);
window = SDL_CreateWindow("2048",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH, HEIGHT, SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE);
renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
reset();

for(;;){
    SDL_Event ev;
    while(SDL_PollEvent(&ev)){
        switch(ev.type){
        case SDL_QUIT:
            goto finish;
            break;
        case SDL_KEYDOWN:
            switch(ev.key.keysym.sym){
            case SDLK_ESCAPE: case SDLK_q:
                goto finish;
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
    draw();
    SDL_RenderPresent(renderer);
    SDL_Delay(16);
}
finish:
SDL_DestroyRenderer(renderer);
SDL_DestroyWindow(window);
#ifdef __APPLE__
SDL_PumpEvents();
#endif
SDL_Quit();
