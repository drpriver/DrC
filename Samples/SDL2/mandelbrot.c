#ifdef __linux__
#pragma pkg_config "sdl2"
#endif
#pragma lib "SDL2"
#include <SDL2/SDL.h>

// This is AI slop but I am too lazy to write one of these myself.
// I didn't feel like cleaning it up.
//
// Interactive Mandelbrot set explorer.
// Scroll to zoom, click and drag to pan. Press r to reset, q/Escape to quit.
// Rendering is multithreaded using a pool of SDL threads + semaphores.
// Uses Mariani-Silver border tracing to skip uniform regions.

enum { RW = 400, RH = 300, SCALE = 2, MAX_ITER = 128, NTHREADS = __mixin(__env("NTHREADS", "8"))};
enum { WIDTH = RW * SCALE, HEIGHT = RH * SCALE };
enum { MIN_BLOCK = 4 }; // minimum block size for subdivision

SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("Mandelbrot",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
SDL_Texture* tex = SDL_CreateTexture(ren,
    SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, RW, RH);

uint32_t pixels[RH][RW];
int iters[RH][RW]; // iteration counts, -1 = not yet computed

// Compute iteration count for a single pixel
int compute_iter(double cr, double ci){
    // Cardioid check
    double q = (cr - 0.25) * (cr - 0.25) + ci * ci;
    if(q * (q + (cr - 0.25)) <= 0.25 * ci * ci) return MAX_ITER;
    // Period-2 bulb check
    if((cr + 1.0) * (cr + 1.0) + ci * ci <= 0.0625) return MAX_ITER;

    double zr = 0.0, zi = 0.0;
    double zr_sq = 0.0, zi_sq = 0.0;
    double zr2 = 0.0, zi2 = 0.0;
    int iter = 0;
    int check = 3, check_counter = 0;
    while(zr_sq + zi_sq < 4.0 && iter < MAX_ITER){
        zi = 2.0 * zr * zi + ci;
        zr = zr_sq - zi_sq + cr;
        zr_sq = zr * zr;
        zi_sq = zi * zi;
        iter++;
        if(zr == zr2 && zi == zi2){ iter = MAX_ITER; break; }
        check_counter++;
        if(check_counter == check){
            check_counter = 0;
            check *= 2;
            zr2 = zr;
            zi2 = zi;
        }
    }
    return iter;
}

uint32_t iter_to_color(int iter){
    if(iter == MAX_ITER) return 0xFF000000u;
    double mu = (double)iter;
    double t = mu / MAX_ITER;
    if(t < 0.0) t = 0.0;
    if(t > 1.0) t = 1.0;
    int r = (int)(9.0 * (1.0 - t) * t * t * t * 255.0);
    int g = (int)(15.0 * (1.0 - t) * (1.0 - t) * t * t * 255.0);
    int bl = (int)(8.5 * (1.0 - t) * (1.0 - t) * (1.0 - t) * t * 255.0);
    if(r > 255) r = 255;
    if(g > 255) g = 255;
    if(bl > 255) bl = 255;
    return 0xFF000000u | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)bl;
}

typedef struct Worker Worker;
struct Worker {
    int id;
    int y_start;
    int y_end;
    double cx, cy, vscale;
    SDL_sem* work_ready;
    SDL_sem* work_done;
    _Bool quit;
};

// Ensure pixel at (row,col) has been computed
int ensure_iter(int row, int col, double view_cx, double view_cy, double view_vscale){
    if(iters[row][col] < 0){
        double cr = view_cx + (col - RW / 2) * view_vscale;
        double ci = view_cy + (row - RH / 2) * view_vscale;
        iters[row][col] = compute_iter(cr, ci);
    }
    return iters[row][col];
}

// Mariani-Silver: render rectangle [y0,y1) x [x0,x1)
// Computes border pixels, if all same iter count, flood-fills interior.
// Otherwise subdivides.
void fill_rect(int y0, int x0, int y1, int x1, double vcx, double vcy, double vsc){
    int w = x1 - x0;
    int h = y1 - y0;
    if(w <= 0 || h <= 0) return;

    // Compute all border pixels
    int border_val = ensure_iter(y0, x0, vcx, vcy, vsc);
    _Bool uniform = 1;

    // Top and bottom rows
    for(int x = x0; x < x1; x++){
        if(ensure_iter(y0, x, vcx, vcy, vsc) != border_val) uniform = 0;
        if(ensure_iter(y1 - 1, x, vcx, vcy, vsc) != border_val) uniform = 0;
    }
    // Left and right columns
    for(int y = y0; y < y1; y++){
        if(ensure_iter(y, x0, vcx, vcy, vsc) != border_val) uniform = 0;
        if(ensure_iter(y, x1 - 1, vcx, vcy, vsc) != border_val) uniform = 0;
    }

    if(uniform){
        // Fill interior
        uint32_t color = iter_to_color(border_val);
        for(int y = y0; y < y1; y++)
            for(int x = x0; x < x1; x++){
                iters[y][x] = border_val;
                pixels[y][x] = color;
            }
        return;
    }

    // Too small to subdivide — compute everything
    if(w <= MIN_BLOCK || h <= MIN_BLOCK){
        for(int y = y0; y < y1; y++)
            for(int x = x0; x < x1; x++){
                int it = ensure_iter(y, x, vcx, vcy, vsc);
                pixels[y][x] = iter_to_color(it);
            }
        return;
    }

    // Subdivide into 4 quadrants
    int mx = x0 + w / 2;
    int my = y0 + h / 2;
    fill_rect(y0, x0, my, mx, vcx, vcy, vsc);
    fill_rect(y0, mx, my, x1, vcx, vcy, vsc);
    fill_rect(my, x0, y1, mx, vcx, vcy, vsc);
    fill_rect(my, mx, y1, x1, vcx, vcy, vsc);
}

int render_worker(void* arg){
    Worker* w = (Worker*)arg;
    for(;;){
        SDL_SemWait(w->work_ready);
        if(w->quit) return 0;
        fill_rect(w->y_start, 0, w->y_end, RW, w->cx, w->cy, w->vscale);
        SDL_SemPost(w->work_done);
    }
}

// Create thread pool
Worker workers[NTHREADS];
SDL_Thread* threads[NTHREADS];
int rows_per = RH / NTHREADS;
for(int i = 0; i < NTHREADS; i++){
    workers[i].id = i;
    workers[i].y_start = i * rows_per;
    workers[i].y_end = (i == NTHREADS - 1) ? RH : (i + 1) * rows_per;
    workers[i].quit = 0;
    workers[i].work_ready = SDL_CreateSemaphore(0);
    workers[i].work_done = SDL_CreateSemaphore(0);
    threads[i] = SDL_CreateThread(render_worker, "render", &workers[i]);
}

// Show the window on the current space before doing heavy computation.
SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
SDL_RenderClear(ren);
SDL_RenderPresent(ren);
SDL_PumpEvents();
SDL_RaiseWindow(win);

// Viewport
double cx = -0.5, cy = 0.0;
double vscale = 3.0 / RH;

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
        // Clear iteration cache
        SDL_memset(iters, -1, sizeof iters);
        // Dispatch work to pool
        for(int i = 0; i < NTHREADS; i++){
            workers[i].cx = cx;
            workers[i].cy = cy;
            workers[i].vscale = vscale;
            SDL_SemPost(workers[i].work_ready);
        }
        // Wait for all to finish
        for(int i = 0; i < NTHREADS; i++)
            SDL_SemWait(workers[i].work_done);
        SDL_UpdateTexture(tex, (const SDL_Rect*)0, pixels, RW * 4);
    }

    SDL_RenderCopy(ren, tex, (const SDL_Rect*)0, (const SDL_Rect*)0);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
}

// Shut down thread pool
for(int i = 0; i < NTHREADS; i++){
    workers[i].quit = 1;
    SDL_SemPost(workers[i].work_ready);
}
for(int i = 0; i < NTHREADS; i++){
    SDL_WaitThread(threads[i], (int*)0);
    SDL_DestroySemaphore(workers[i].work_ready);
    SDL_DestroySemaphore(workers[i].work_done);
}

SDL_DestroyTexture(tex);
SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
SDL_Quit();
