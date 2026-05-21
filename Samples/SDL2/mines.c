//usr/bin/env drc "$0" "$@"; exit
#ifdef __linux__
#pragma pkg_config "sdl2"
#else
#pragma lib "SDL2"
#endif
#include <SDL2/SDL.h> <SDL.h>
#include "bitmap_font.h"

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Texture* font_tex;
unsigned rng;
enum {MAX_X=30, MAX_Y=30, INIT_X=20, INIT_Y=20};
int WIDTH=401, HEIGHT=401,
    num_mines,
    cells_x = INIT_Y, cells_y=INIT_Y,
    cur_x=INIT_X/2-1, cur_y=INIT_Y/2-1,
    n_revealed = 0;
struct {
    _Bool mine: 1, flag: 1, revealed: 1;
} board[MAX_X*MAX_Y];
enum {
    GAME_NOT_STARTED,
    GAME_STARTED,
    GAME_WON,
    GAME_LOST,
} state = GAME_NOT_STARTED;
constexpr float border = 1.f;
_Bool darkmode;

unsigned rand32(void);
void draw(void),
     draw_char(unsigned char ch, float x, float y, float sz, SDL_Color),
     click(int x, int y, _Bool shift),
     reveal(int x, int y),
     scan(int x, int y, int *mines, int *flagged, int *hidden, int hi[8]),
     autoplay(void),
     cheat(void);

_Bool check_subset(int ahi[8], int ahidden, int bhi[8], int bhidden),
      contains(int hi[8], int h, int v);

void init_font(void){
    int atlas_w = FONT_COUNT * FONT_W;
    SDL_Surface* surf = SDL_CreateRGBSurface(0, atlas_w, FONT_H, 32,
        0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    SDL_LockSurface(surf);
    unsigned* pixels = (unsigned*)surf->pixels;
    for(int ch = 0; ch < FONT_COUNT; ch++){
        for(int row = 0; row < FONT_H; row++){
            unsigned char bits = font8x16[ch][row];
            for(int col = 0; col < FONT_W; col++){
                int px = ch * FONT_W + col;
                if(bits & (0x80 >> col))
                    pixels[row * atlas_w + px] = 0xffffffff;
                else
                    pixels[row * atlas_w + px] = 0x00000000;
            }
        }
    }
    SDL_UnlockSurface(surf);
    font_tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_FreeSurface(surf);
    SDL_SetTextureBlendMode(font_tex, SDL_BLENDMODE_BLEND);
}

unsigned rand32(void){
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
void draw(void){
    SDL_Color base  = darkmode ? (SDL_Color){0x40, 0x40, 0x40, 0xff} : (SDL_Color){0xc0, 0xc0, 0xc0, 0xff};
    SDL_Color hi    = darkmode ? (SDL_Color){0x60, 0x60, 0x60, 0xff} : (SDL_Color){0xf0, 0xf0, 0xf0, 0xff};
    SDL_Color lo    = darkmode ? (SDL_Color){0x20, 0x20, 0x20, 0xff} : (SDL_Color){0x90, 0x90, 0x90, 0xff};
    SDL_Color flagc = darkmode ? (SDL_Color){0xff, 0x40, 0x40, 0xff} : (SDL_Color){0xc0, 0x00, 0x00, 0xff};
    SDL_Color minec = darkmode ? (SDL_Color){0xff, 0xff, 0xff, 0xff} : (SDL_Color){0x00, 0x00, 0x00, 0xff};
    static const SDL_Color light_nums[8] = {
        {0x00, 0x00, 0xff, 0xff}, // blue
        {0x00, 0x80, 0x00, 0xff}, // green
        {0xff, 0x00, 0x00, 0xff}, // red
        {0x00, 0x00, 0x80, 0xff}, // navy
        {0x80, 0x00, 0x00, 0xff}, // maroon
        {0x00, 0x80, 0x80, 0xff}, // teal
        {0x00, 0x00, 0x00, 0xff}, // black
        {0x80, 0x80, 0x80, 0xff}, // gray
    };
    static const SDL_Color dark_nums[8] = {
        {0x60, 0xa0, 0xff, 0xff},
        {0x40, 0xff, 0x40, 0xff},
        {0xff, 0x60, 0x60, 0xff},
        {0xa0, 0xa0, 0xff, 0xff},
        {0xff, 0xa0, 0x40, 0xff},
        {0x40, 0xff, 0xff, 0xff},
        {0xff, 0xff, 0xff, 0xff},
        {0xc0, 0xc0, 0xc0, 0xff},
    };
    const SDL_Color* nums = darkmode ? dark_nums : light_nums;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xff);
    SDL_RenderClear(renderer);
    float side = (float)(WIDTH-border)/(float)cells_x;
    SDL_FRect r;
    SDL_SetRenderDrawColor(renderer, 0, 0xff, 0xff, 0xff);
    r = (SDL_FRect){cur_x*side, cur_y*side, side+border, side+border};
    SDL_RenderFillRectF(renderer, &r);
    for(int y = 0, i=0; y < cells_y; y++)
    for(int x = 0; x < cells_x; x++, i++){
        SDL_SetRenderDrawColor(renderer, base.r, base.g, base.b, 0xff);
        r = (SDL_FRect){x*side+border, y*side+border, side-border, side-border};
        SDL_RenderFillRectF(renderer, &r);
        if((state == GAME_STARTED && !board[i].revealed) || state == GAME_NOT_STARTED){
            SDL_SetRenderDrawColor(renderer, hi.r, hi.g, hi.b, 0xff);
            r = (SDL_FRect){x*side+border, y*side+border, border, side-border};
            SDL_RenderFillRectF(renderer, &r);
            r = (SDL_FRect){x*side+border, y*side+border, side-border, border};
            SDL_RenderFillRectF(renderer, &r);
            SDL_SetRenderDrawColor(renderer, lo.r, lo.g, lo.b, 0xff);
            r = (SDL_FRect){x*side+border+border, y*side+side-border, side-border-border, border};
            SDL_RenderFillRectF(renderer, &r);
            r = (SDL_FRect){x*side+side-border, y*side+border+border, border, side-border-border};
            SDL_RenderFillRectF(renderer, &r);
            if(state == GAME_STARTED && board[i].flag)
                draw_char('F', x*side+border+side/4.f, y*side+border, side/2.f, flagc);
        }
        else if(board[i].mine){
            if(state == GAME_WON || board[i].flag)
                SDL_SetRenderDrawColor(renderer, 0x30, 0xf0, 0x30, 0xff);
            else
                SDL_SetRenderDrawColor(renderer, 0xf0, 0x30, 0x30, 0xff);
            SDL_RenderFillRectF(renderer, &r);
            draw_char(board[i].flag?'F':'M', x*side+border+side/4.f, y*side+border, side/2.f, minec);
        }
        else {
            int neighbors = 0;
            for(int dy = -1; dy <= 1; dy++)
            for(int dx = -1; dx <= 1; dx++){
                if(!dx && !dy) continue;
                int ix = x + dx;
                if(ix < 0) continue;
                if(ix >= cells_x) continue;
                int iy = y + dy;
                if(iy < 0) continue;
                if(iy >= cells_y) continue;
                neighbors += board[iy*cells_x+ix].mine;
            }
            if(neighbors){
                int glyph = '0' + neighbors;
                draw_char((unsigned char)(unsigned)glyph, x*side+border+side/4.f, y*side+border, side/2.f, nums[neighbors-1]);
            }
        }
    }
}

void click(int x, int y, _Bool shift){
    if(state == GAME_NOT_STARTED){
        if(shift) return;
        SDL_memset(board, 0, sizeof board);
        num_mines = 0;
        n_revealed = 0;
        int target = (cells_x*cells_y*15/100) + (rand32() % (cells_x*cells_y*6/100));
        while(num_mines < target){
            int cx = rand32() % cells_x;
            int cy = rand32() % cells_y;
            if(cx >= x-1 && cx <= x+1 && cy >= y-1 && cy <= y+1)
                continue;
            if(board[cy*cells_x+cx].mine) continue;
            board[cy*cells_x+cx].mine = 1;
            num_mines++;
        }
        state = GAME_STARTED;
    }
    if(state != GAME_STARTED)
        return;
    if(shift){
        if(board[y*cells_x+x].revealed) return;
        board[y*cells_x+x].flag = !board[y*cells_x+x].flag;
        return;
    }
    if(board[y*cells_x+x].flag)
        return;
    reveal(x, y);
}

void reveal(int x, int y){
    if(board[y*cells_x+x].revealed) return;
    board[y*cells_x+x].revealed = 1;
    n_revealed++;
    if(board[y*cells_x+x].mine){
        state = GAME_LOST;
        return;
    }
    int neighbors = 0;
    for(int dy = -1; dy <= 1; dy++){
        int cy = y + dy;
        if(cy < 0) continue;
        if(cy >= cells_y) continue;
        for(int dx = -1; dx <= 1; dx++){
            if(dx == 0 && dy == 0) continue;
            int cx = x + dx;
            if(cx < 0) continue;
            if(cx >= cells_x) continue;
            if(board[cy*cells_x+cx].mine) neighbors++;
        }
    }
    if(!neighbors){
        for(int dy = -1; dy <= 1; dy++){
            int cy = y + dy;
            if(cy < 0) continue;
            if(cy >= cells_y) continue;
            for(int dx = -1; dx <= 1; dx++){
                if(dx == 0 && dy == 0) continue;
                int cx = x + dx;
                if(cx < 0) continue;
                if(cx >= cells_x) continue;
                reveal(cx, cy);
            }
        }
    }
    if(n_revealed + num_mines == cells_x * cells_y)
        state = GAME_WON;
}

void draw_char(unsigned char ch, float x, float y, float sz, SDL_Color color){
    int idx = ch - FONT_FIRST;
    SDL_Rect src = {idx * FONT_W, 0, FONT_W, FONT_H};
    SDL_Rect dst = {(int)x, (int)y, (int)sz, (int)(2*sz)};
    SDL_SetTextureColorMod(font_tex, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font_tex, color.a);
    SDL_RenderCopy(renderer, font_tex, &src, &dst);
}
_Bool contains(int hi[8], int h, int v){
    for(int i = 0; i < h; i++) if(hi[i] == v) return 1;
    return 0;
}
_Bool check_subset(int ahi[8], int ahidden, int bhi[8], int bhidden){
    for(int a = 0; a < ahidden; a++) if(!contains(bhi, bhidden, ahi[a])) return 0;
    return 1;
}
void scan(int x, int y, int *mines, int *flagged, int *hidden, int hi[8]){
    *mines = 0; *flagged = 0; *hidden = 0;
    for(int dy = -1; dy <= 1; dy++){
        int iy = y + dy;
        if(iy < 0 || iy >= cells_y) continue;
        for(int dx = -1; dx <= 1; dx++){
            if(!dx && !dy) continue;
            int ix = x + dx;
            if(ix < 0 || ix >= cells_x) continue;
            int idx = iy * cells_x + ix;
            if(board[idx].mine) ++*mines;
            if(board[idx].revealed) continue;
            if(board[idx].flag){
                ++*flagged;
                continue;
            }
            int h = (*hidden)++;
            hi[h] = idx;
        }
    }
}
void autoplay(void){
    if(state == GAME_NOT_STARTED){
        click(cur_x, cur_y, 0);
        return;
    }
    if(state != GAME_STARTED)
        return;
    int mines, flagged, hidden, hi[8];
    for(int y = 0, i = 0; y < cells_y; y++)
    for(int x = 0; x < cells_x; x++, i++){
        if(!board[i].revealed) continue;
        scan(x, y, &mines, &flagged, &hidden, hi);
        if(!hidden) continue;
        if(mines == flagged){
            for(int k = 0; k < hidden; k++)
                click(hi[k] % cells_x, hi[k] / cells_x, 0);
            return;
        }
        if(mines == flagged + hidden){
            for(int k = 0; k < hidden; k++)
                click(hi[k] % cells_x, hi[k] / cells_x, 1);
            return;
        }
    }
    int amines, aflagged, ahidden, ahi[8],
        bmines, bflagged, bhidden, bhi[8],
        extras[8], nextras;
    for(int ay = 0, ai = 0; ay < cells_y; ay++)
    for(int ax = 0; ax < cells_x; ax++, ai++){
        if(!board[ai].revealed) continue;
        scan(ax, ay, &amines, &aflagged, &ahidden, ahi);
        if(!ahidden) continue;
        int need_a = amines - aflagged;
        for(int by = ay-2; by <= ay+2; by++){
            if(by < 0 || by >= cells_y) continue;
            for(int bx = ax-2; bx <= ax+2; bx++){
                if(bx < 0 || bx >= cells_x) continue;
                if(!board[by*cells_x+bx].revealed) continue;
                scan(bx, by, &bmines, &bflagged, &bhidden, bhi);
                if(bhidden <= ahidden) continue;
                if(!check_subset(ahi, ahidden, bhi, bhidden))
                    continue;
                nextras = 0;
                for(int b = 0; b < bhidden; b++)
                    if(!contains(ahi, ahidden, bhi[b]))
                        extras[nextras++] = bhi[b];
                int need_diff = bmines - bflagged - need_a;
                if(need_diff == 0){
                    for(int k = 0; k < nextras; k++)
                        click(extras[k] % cells_x, extras[k] / cells_x, 0);
                    return;
                }
                if(need_diff == nextras){
                    for(int k = 0; k < nextras; k++)
                        click(extras[k] % cells_x, extras[k] / cells_x, 1);
                    return;
                }
            }
        }
    }
}

void cheat(void){
    if(state != GAME_STARTED) return;
    int idx = cur_y * cells_x + cur_x;
    if(!board[idx].flag) click(cur_x, cur_y, board[idx].mine);
}

int main(int argc, char** argv){
    {
        const char* lm = SDL_getenv("LIGHTMODE");
        if(lm && *lm && !SDL_atoi(lm))
            darkmode = 1;
        const char* dm = SDL_getenv("DARKMODE");
        if(dm && *dm && SDL_atoi(dm))
            darkmode = 1;
    }
    for(int i = 1; i < argc; i++){
        if(SDL_strcmp(argv[i], "-d") == 0)
            darkmode = 1;
        else if(SDL_strcmp(argv[i], "--dark-mode") == 0)
            darkmode = 1;
        else if(SDL_strcmp(argv[i], "-l") == 0)
            darkmode = 0;
        else if(SDL_strcmp(argv[i], "--light-mode") == 0)
            darkmode = 0;
    }
    while((rng = (unsigned)SDL_GetPerformanceCounter()) == 0){ }
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow("Mines",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH*2, HEIGHT*2,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    init_font();
    for(;;){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            switch(ev.type){
                case SDL_WINDOWEVENT:
                    switch(ev.window.event){
                        case SDL_WINDOWEVENT_RESIZED:{
                            int w = ev.window.data1, h = ev.window.data2;
                            if(w*HEIGHT > h*WIDTH) w = h * WIDTH/HEIGHT;
                            else h = w*HEIGHT/WIDTH;
                            SDL_SetWindowSize(window, w, h);
                            break;
                        }
                    }
                    break;
                case SDL_QUIT:
                    goto finally;
                case SDL_KEYDOWN:
                    switch(ev.key.keysym.sym){
                        case SDLK_MINUS:
                            if(state != GAME_NOT_STARTED)
                                break;
                            if(cells_y < 10) break;
                            cells_y--;
                            cells_x--;
                            break;
                        case SDLK_EQUALS:
                            if(state != GAME_NOT_STARTED)
                                break;
                            if(cells_y == MAX_Y) break;
                            cells_y++;
                            cells_x++;
                            break;
                        case SDLK_0:
                            if(state != GAME_NOT_STARTED)
                                break;
                            cells_y = INIT_Y;
                            cells_x = INIT_X;
                            break;
                        case SDLK_d:
                            darkmode = !darkmode;
                            break;
                        case SDLK_p:
                            autoplay();
                            break;
                        case SDLK_q:
                            goto finally;
                        case SDLK_c:
                            cheat();
                            break;
                        case SDLK_n:
                        case SDLK_r:
                            state = GAME_NOT_STARTED;
                            break;
                        case SDLK_k:
                        case SDLK_UP:
                            if(cur_y > 0) cur_y--;
                            break;
                        case SDLK_j:
                        case SDLK_DOWN:
                            if(cur_y < cells_y-1) cur_y++;
                            break;
                        case SDLK_h:
                        case SDLK_LEFT:
                            if(cur_x > 0) cur_x--;
                            break;
                        case SDLK_l:
                        case SDLK_RIGHT:
                            if(cur_x < cells_x-1) cur_x++;
                            break;
                        case SDLK_f:
                            click(cur_x, cur_y, 1);
                            break;
                        case SDLK_SPACE:
                        case SDLK_RETURN:
                            click(cur_x, cur_y, SDL_GetModState() & KMOD_SHIFT);
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN: {
                    float side = (float)(WIDTH-border)/(float)cells_x;
                    int cx = (int)(ev.button.x / side);
                    int cy = (int)(ev.button.y / side);
                    if(cx >= 0 && cx < cells_x && cy >= 0 && cy < cells_y){
                        cur_x = cx;
                        cur_y = cy;
                        click(cx, cy, SDL_GetModState() & KMOD_SHIFT);
                    }
                } break;
            }
        }
        draw();
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    finally:
    SDL_DestroyTexture(font_tex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    #ifdef __APPLE__
    SDL_PumpEvents();
    #endif
    SDL_Quit();
    return 0;
}
