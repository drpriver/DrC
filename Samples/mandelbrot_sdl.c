#pragma lib "SDL2"
#include <std.h>
#include <SDL2/SDL.h>

// Interactive Mandelbrot set explorer.
// Scroll to zoom, click and drag to pan. Press r to reset, q/Escape to quit.

// Compute at low res, stretch to window size for speed in the interpreter.
enum { RW = 200, RH = 150, SCALE = 4, MAX_ITER = 64 };
enum { WIDTH = RW * SCALE, HEIGHT = RH * SCALE };

SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("Mandelbrot",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SDL_Texture* tex = SDL_CreateTexture(ren,
    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RW, RH);

uint32_t pixels[RH][RW];

// Show the window on the current space before doing heavy computation.
SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
SDL_RenderClear(ren);
SDL_RenderPresent(ren);
SDL_PumpEvents();
SDL_RaiseWindow(win);

// Viewport
double cx = -0.5, cy = 0.0;
double vscale = 3.0 / RH; // units per pixel in the compute buffer

_Bool dirty = 1;
_Bool running = 1;
int drag = 0;
int drag_x = 0, drag_y = 0;

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
                cx = -0.5; cy = 0.0;
                vscale = 3.0 / RH;
                dirty = 1;
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if(ev.button.button == SDL_BUTTON_LEFT){
                drag = 1;
                drag_x = ev.button.x;
                drag_y = ev.button.y;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            if(ev.button.button == SDL_BUTTON_LEFT)
                drag = 0;
            break;
        case SDL_MOUSEMOTION:
            if(drag){
                // Convert window pixel drag to compute-buffer units
                int dx = ev.motion.x - drag_x;
                int dy = ev.motion.y - drag_y;
                cx -= (dx / (double)SCALE) * vscale;
                cy -= (dy / (double)SCALE) * vscale;
                drag_x = ev.motion.x;
                drag_y = ev.motion.y;
                dirty = 1;
            }
            break;
        case SDL_MOUSEWHEEL: {
            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            // Zoom centered on mouse (convert window coords to buffer coords)
            double bx = mx / (double)SCALE;
            double by = my / (double)SCALE;
            double before_x = cx + (bx - RW / 2) * vscale;
            double before_y = cy + (by - RH / 2) * vscale;
            double factor = ev.wheel.y > 0 ? 0.8 : 1.25;
            vscale *= factor;
            cx = before_x - (bx - RW / 2) * vscale;
            cy = before_y - (by - RH / 2) * vscale;
            dirty = 1;
            break;
        }
        }
    }

    if(dirty){
        dirty = 0;
        for(int row = 0; row < RH; row++){
            double ci = cy + (row - RH / 2) * vscale;
            for(int col = 0; col < RW; col++){
                double cr = cx + (col - RW / 2) * vscale;
                double zr = 0.0, zi = 0.0;
                int iter = 0;
                while(zr * zr + zi * zi < 4.0 && iter < MAX_ITER){
                    double tmp = zr * zr - zi * zi + cr;
                    zi = 2.0 * zr * zi + ci;
                    zr = tmp;
                    iter++;
                }
                uint32_t color;
                if(iter == MAX_ITER){
                    color = 0xFF000000u;
                } else {
                    // Smooth iteration count to eliminate banding
                    double mu = iter + 1.0 - log(log(sqrt(zr*zr + zi*zi))) / log(2.0);
                    double t = mu / MAX_ITER;
                    if(t < 0.0) t = 0.0;
                    if(t > 1.0) t = 1.0;
                    int r = (int)(9.0 * (1.0 - t) * t * t * t * 255.0);
                    int g = (int)(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0);
                    int b = (int)(8.5 * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0);
                    if(r > 255) r = 255;
                    if(g > 255) g = 255;
                    if(b > 255) b = 255;
                    color = 0xFF000000u | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)b;
                }
                pixels[row][col] = color;
            }
        }
        SDL_UpdateTexture(tex, (const SDL_Rect*)0, pixels, RW * 4);
    }

    // SDL stretches the small texture to fill the window
    SDL_RenderCopy(ren, tex, (const SDL_Rect*)0, (const SDL_Rect*)0);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
}

SDL_DestroyTexture(tex);
SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
SDL_Quit();
