#pragma lib "SDL2"
#include <std.h>
#include <SDL2/SDL.h>

enum { CELL = 8, W = 100, H = 75 };

unsigned char grid[H][W], next[H][W];

// Seed: R-pentomino
int cx = W / 2, cy = H / 2;
grid[cy-1][cx]   = 1;
grid[cy-1][cx+1] = 1;
grid[cy][cx-1]   = 1;
grid[cy][cx]     = 1;
grid[cy+1][cx]   = 1;

SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("Game of Life",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    W * CELL, H * CELL, 0);
SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

_Bool running = 1;
_Bool paused = 0;
enum {
    NONE,
    DRAWING,
    ERASING,
} mouse_down = NONE;

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
            case SDLK_SPACE:
                paused = !paused;
                break;
            case SDLK_c:
                memset(grid, 0, sizeof grid);
                break;
            case SDLK_r:
                for(int y = 0; y < H; y++)
                    for(int x = 0; x < W; x++)
                        grid[y][x] = (unsigned char)(rand() % 5 == 0);
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN: {
            int gx = ev.button.x / CELL;
            int gy = ev.button.y / CELL;
            if(gx >= 0 && gx < W && gy >= 0 && gy < H){
                mouse_down = grid[gy][gx] ? ERASING : DRAWING;
                grid[gy][gx] = (unsigned char)(mouse_down == DRAWING);
            }
            break;
        }
        case SDL_MOUSEBUTTONUP:
            mouse_down = NONE;
            break;
        case SDL_MOUSEMOTION:
            if(mouse_down){
                int gx = ev.motion.x / CELL;
                int gy = ev.motion.y / CELL;
                if(gx >= 0 && gx < W && gy >= 0 && gy < H)
                    grid[gy][gx] = (unsigned char)(mouse_down == DRAWING);
            }
            break;
        }
    }

    if(!paused){
        // Compute next generation
        memset(next, 0, sizeof next);
        for(int y = 0; y < H; y++){
            for(int x = 0; x < W; x++){
                int n = 0;
                for(int dy = -1; dy <= 1; dy++){
                    for(int dx = -1; dx <= 1; dx++){
                        if(dy == 0 && dx == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if(ny >= 0 && ny < H && nx >= 0 && nx < W)
                            n += grid[ny][nx];
                    }
                }
                if(grid[y][x])
                    next[y][x] = (unsigned char)(n == 2 || n == 3);
                else
                    next[y][x] = (unsigned char)(n == 3);
            }
        }
        memcpy(grid, next, sizeof grid);
    }

    // Draw
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);
    SDL_SetRenderDrawColor(ren, 0, 200, 80, 255);
    for(int y = 0; y < H; y++){
        for(int x = 0; x < W; x++){
            if(grid[y][x]){
                SDL_Rect r = {x * CELL, y * CELL, CELL - 1, CELL - 1};
                SDL_RenderFillRect(ren, &r);
            }
        }
    }
    SDL_RenderPresent(ren);
    SDL_Delay(30);
}

SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
SDL_Quit();
