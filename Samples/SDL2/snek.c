// Snek game
#ifdef __linux__
#pragma pkg_config "sdl2"
#endif
#pragma lib "SDL2"
#include <SDL2/SDL.h>

SDL_Window* window;
SDL_Renderer* renderer;
typedef signed char Tile;
enum { NONE, WIN, LOSE} gwinlose = NONE;
int glen;
int gpaused;
int gautoplay;
int gtick;

void main_loop(void);
void render_and_present(int sx, int sy, int dx, int dy);
int find_direction_to(int sx, int sy, int tx, int ty, int* out_dx, int* out_dy);
int find_safe_direction(int sx, int sy, int* out_dx, int* out_dy);
void autoplay_choose_direction(int sx, int sy, int* dx, int* dy);
_Bool has_apple(void);
void place_apple(void);
unsigned rng(void);
void init_rng(void);

enum {BOARD_SIZE=10, HALF_BOARD_SIZE=5, PAD=2};
Tile* snake[BOARD_SIZE*BOARD_SIZE];
Tile board[BOARD_SIZE*BOARD_SIZE];
void open_window_and_renderer(int width, int height){
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow(
        "Snek!",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    if(!window){
        SDL_Log("No window!\n");
        return;
    }
    renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer) {SDL_Log("No renderer!\n");return;}
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);
    int rw, rh;
    SDL_GetRendererOutputSize(renderer, &rw, &rh);
    if(rw != ww || rh != wh){
        SDL_RenderSetScale(renderer, (float)rw/ww, (float)rh/wh);
    }
}
int main(){
    int width = 640; int height = 640;
    init_rng();
    if(width < 400) width = 400;
    if(width > 1200) width = 1200;
    if(height < 400) height = 400;
    if(height > 1200) height = 1200;
    open_window_and_renderer(width, height);
    if(window && renderer){
        main_loop();
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void grow_window(void){
    int w; int h;
    SDL_GetWindowSize(window, &w, &h);
    w = w + 50;
    h = h + 50;
    if(w > 1200) w = 1200;
    if(h > 1200) h = 1200;
    SDL_SetWindowSize(window, w, h);
}

void shrink_window(void){
    int w; int h;
    SDL_GetWindowSize(window, &w, &h);
    w = w - 50;
    h = h - 50;
    if(w < 100) w = 100;
    if(h < 100) h = 100;
    SDL_SetWindowSize(window, w, h);
}

_Bool has_apple(void){
    for(int y = 0; y < BOARD_SIZE; y = y + 1){
        for(int x = 0; x < BOARD_SIZE; x = x + 1){
            Tile* p = y*BOARD_SIZE+x+board;
            if(*p == -1){
                return 1;
            }
        }
    }
    return 0;
}

void place_apple(void){
    for(;;){
        int y = rng() % BOARD_SIZE;
        int x = rng() % BOARD_SIZE;
        Tile* p = y*BOARD_SIZE+x+board;
        if(!*p){
            *p = -1;
            return;
        }
    }
}

void move_snake(int x, int y){
    Tile* p;
    p = board + BOARD_SIZE*y+x;
    Tile b = *p;
    *p = 1;
    if(b == -1){
        snake[glen] = p;
        glen = glen + 1;
        return;
    }
    if(b == 1){
        if(glen >= BOARD_SIZE * BOARD_SIZE / 4){
            gwinlose = WIN;
            return;
        }
        gwinlose = LOSE;
        return;
    }
    int l = glen-1;
    Tile* ps = snake[0];
    *ps = 0;
    for(int i = 0; i < l; i = i + 1){
        snake[i] = snake[i+1];
    }
    snake[glen-1] = p;
}

void simulate(int x, int y){
    move_snake(x, y);
    if(gwinlose) return;
    int apple = has_apple();
    if(!apple) place_apple();
}

int find_direction_to(int sx, int sy, int tx, int ty, int* out_dx, int* out_dy){
    signed char visited[BOARD_SIZE*BOARD_SIZE];
    int qx[BOARD_SIZE*BOARD_SIZE];
    int qy[BOARD_SIZE*BOARD_SIZE];
    int qdx[BOARD_SIZE*BOARD_SIZE];
    int qdy[BOARD_SIZE*BOARD_SIZE];
    int qh = 0, qt = 0;
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    SDL_memset(visited, 0, sizeof visited);
    visited[sy*BOARD_SIZE+sx] = 1;
    for(int d = 0; d < 4; d = d + 1){
        int nx = sx + dirs[d][0];
        int ny = sy + dirs[d][1];
        if(nx < 0) nx = BOARD_SIZE-1;
        if(ny < 0) ny = BOARD_SIZE-1;
        if(nx >= BOARD_SIZE) nx = 0;
        if(ny >= BOARD_SIZE) ny = 0;
        if(visited[ny*BOARD_SIZE+nx]) continue;
        visited[ny*BOARD_SIZE+nx] = 1;
        if(nx == tx && ny == ty){
            *out_dx = dirs[d][0];
            *out_dy = dirs[d][1];
            return 1;
        }
        Tile val = board[ny*BOARD_SIZE+nx];
        if(val == 1) continue;
        qx[qt] = nx;
        qy[qt] = ny;
        qdx[qt] = dirs[d][0];
        qdy[qt] = dirs[d][1];
        qt = qt + 1;
    }
    while(qh < qt){
        int cx = qx[qh];
        int cy = qy[qh];
        int fdx = qdx[qh];
        int fdy = qdy[qh];
        qh = qh + 1;
        for(int d = 0; d < 4; d = d + 1){
            int nx = cx + dirs[d][0];
            int ny = cy + dirs[d][1];
            if(nx < 0) nx = BOARD_SIZE-1;
            if(ny < 0) ny = BOARD_SIZE-1;
            if(nx >= BOARD_SIZE) nx = 0;
            if(ny >= BOARD_SIZE) ny = 0;
            if(visited[ny*BOARD_SIZE+nx]) continue;
            visited[ny*BOARD_SIZE+nx] = 1;
            if(nx == tx && ny == ty){
                *out_dx = fdx;
                *out_dy = fdy;
                return 1;
            }
            Tile val = board[ny*BOARD_SIZE+nx];
            if(val == 1) continue;
            qx[qt] = nx;
            qy[qt] = ny;
            qdx[qt] = fdx;
            qdy[qt] = fdy;
            qt = qt + 1;
        }
    }
    return 0;
}

int find_safe_direction(int sx, int sy, int* out_dx, int* out_dy){
    int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    for(int d = 0; d < 4; d = d + 1){
        int nx = sx + dirs[d][0];
        int ny = sy + dirs[d][1];
        if(nx < 0) nx = BOARD_SIZE-1;
        if(ny < 0) ny = BOARD_SIZE-1;
        if(nx >= BOARD_SIZE) nx = 0;
        if(ny >= BOARD_SIZE) ny = 0;
        Tile val = board[ny*BOARD_SIZE+nx];
        if(val != 1){
            *out_dx = dirs[d][0];
            *out_dy = dirs[d][1];
            return 1;
        }
    }
    return 0;
}

void autoplay_choose_direction(int sx, int sy, int* dx, int* dy){
    // Find apple position
    int ax = -1, ay = -1;
    for(int y = 0; y < BOARD_SIZE; y = y + 1){
        for(int x = 0; x < BOARD_SIZE; x = x + 1){
            if(board[y*BOARD_SIZE+x] == -1){
                ax = x;
                ay = y;
            }
        }
    }
    int ndx, ndy;
    // Try to path to apple
    if(ax >= 0 && find_direction_to(sx, sy, ax, ay, &ndx, &ndy)){
        *dx = ndx;
        *dy = ndy;
        return;
    }
    Tile* tail = snake[0];
    int ti = (int)(tail - board);
    int tx = ti % BOARD_SIZE;
    int ty = ti / BOARD_SIZE;
    if(find_direction_to(sx, sy, tx, ty, &ndx, &ndy)){
        *dx = ndx;
        *dy = ndy;
        return;
    }
    if(find_safe_direction(sx, sy, &ndx, &ndy)){
        *dx = ndx;
        *dy = ndy;
    }
}

void main_loop(void){
    SDL_Event event;
    int poll = 1;
    int t = SDL_GetTicks();
    int trigger = 10;
    int tick = trigger-1;
    board[HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE] = 1;
    snake[0] = board +HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE;
    glen = 1;
    int dx = 0;
    int dy = 1;
    int x = HALF_BOARD_SIZE;
    int y = HALF_BOARD_SIZE;
    int turned = 0;
    for(int i = 0;;i++){
        int got_event = 0;
        if(poll){
            if(!SDL_PollEvent(&event)){
                tick = tick + 1;
                if(tick >= trigger){
                    tick = 0;
                    if(!gwinlose){
                        if(!gpaused){
                            if(gautoplay)
                                autoplay_choose_direction(x, y, &dx, &dy);
                            x = x + dx;
                            y = y + dy;
                            if(x == -1) x = BOARD_SIZE-1;
                            if(y == -1) y = BOARD_SIZE-1;
                            if(x == BOARD_SIZE) x = 0;
                            if(y == BOARD_SIZE) y = 0;
                            simulate(x, y);
                            turned = 0;
                        }
                        gtick = gtick + 1;
                        render_and_present(x, y, dx, dy);
                    }
                }
                int t2 = SDL_GetTicks();
                int diff = t2-t;
                if(diff < 16) SDL_Delay(16-diff);
                t = SDL_GetTicks();
                poll = 1;
                continue;
            }
        }
        else {
            SDL_WaitEvent(&event);
            poll = 1;
        }
        int type = event.type;
        if(type == SDL_QUIT){
            break;
        }
        else if(type == SDL_KEYDOWN){
            SDL_Keycode code = event.key.keysym.sym;
            if(code == 'q') break;
            else if(code == ' ') gpaused = !gpaused;
            else if(code == 'p') gautoplay = !gautoplay;
            else if(code == '=') grow_window();
            else if(code == '-') shrink_window();
            else if(code == 'r'){
                gwinlose = NONE;
                tick = trigger-1;
                SDL_memset(board, 0, BOARD_SIZE*BOARD_SIZE* sizeof *board);
                x = HALF_BOARD_SIZE;
                y = HALF_BOARD_SIZE;
                board[HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE] = 1;
                snake[0] = board +HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE;
                glen = 1;
            }
            else if(!turned){
                switch(code){
                    case SDLK_LEFT:
                    case 'a':
                        if(dx != 1){
                            dx = -1;
                            dy = 0;
                            turned = 1;
                        }
                        break;
                    case SDLK_RIGHT:
                    case 'd':
                        if(dx != -1){
                            dx = 1;
                            dy = 0;
                            turned = 1;
                        }
                        break;
                    case SDLK_UP:
                    case 'w':
                        if(dy != 1){
                            dx = 0;
                            dy = -1;
                            turned = 1;
                        }
                        break;
                    case SDLK_DOWN:
                    case 's':
                        if(dy != -1){
                            dx = 0;
                            dy = 1;
                            turned = 1;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
}

void draw_rect(int x, int y, int w, int h){
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void draw_banner(const char* banner, int cols, int rows, int ww, int hh){
    for(int r = 0; r < rows; r = r + 1)
        for(int c = 0; c < cols; c = c + 1)
            if(banner[r * cols + c] == 'x')
                draw_rect(c * ww, r * hh, ww, hh);
}

int snake_index(int bx, int by){
    Tile* p = board + by*BOARD_SIZE + bx;
    for(int i = 0; i < glen; i = i + 1){
        if(snake[i] == p) return i;
    }
    return -1;
}

void render_and_present(int sx, int sy, int dx, int dy){
    int w = 640;
    int h = 480;
    int winlose = gwinlose;
    SDL_GetWindowSize(window, &w, &h);
    // Dark background
    SDL_SetRenderDrawColor(renderer, 0x1a, 0x1a, 0x2e, 0xff);
    SDL_RenderClear(renderer);

    int ww = w/20;
    int hh = h/20;
    const char* win_banner =
        "                                        "
        "                                        "
        "                                        "
        "        x x xxx x x         "
        "        x x x x x x         "
        "        x x x x x x         "
        "         x    x x x x         "
        "         x    xxx xxx         "
        "                                        "
        "     x     x xxx x     x    "
        "     x     x    x    xx    x    "
        "     x x x    x    x x x    "
        "     x x x    x    x    xx    "
        "        x x    xxx x     x    "
        "                                        "
        "                                        "
        "                                        "
        "                                        "
        "                                        "
        "                                        ";
    const char* lose_banner =
        "                                        "
        "                                        "
        "                                        "
        "                                        "
        "        x x xxx x x         "
        "        x x x x x x         "
        "        x x x x x x         "
        "         x    x x x x         "
        "         x    xxx xxx         "
        "                                        "
        "    x     xxx xxx xxx     "
        "    x     x x x     x         "
        "    x     x x xxx xxx     "
        "    x     x x     x x         "
        "    xxx xxx xxx xxx     "
        "                                        "
        "                                        "
        "                                        "
        "                                        "
        "                                        ";
    if(winlose == WIN){
        SDL_SetRenderDrawColor(renderer, 0x0, 0xff, 0x0, 0xff);
        draw_banner(win_banner, 20, 20, ww, hh);
    }
    else if(winlose == LOSE){
        SDL_SetRenderDrawColor(renderer, 0xff, 0x44, 0x44, 0xff);
        draw_banner(lose_banner, 20, 20, ww, hh);
    }
    else {
        int rw = w/BOARD_SIZE;
        int rh = h/BOARD_SIZE;

        // grid background
        for(int y = 0; y < BOARD_SIZE; y = y + 1){
            for(int x = 0; x < BOARD_SIZE; x = x + 1){
                Tile val = *(y*BOARD_SIZE+x+board);
                int cx = x*rw + PAD;
                int cy = y*rh + PAD;
                int cw = rw - PAD*2;
                int ch = rh - PAD*2;

                if((x == sx) & (y == sy)){
                    // Head
                    SDL_SetRenderDrawColor(renderer, 0x0, 0xee, 0xff, 0xff);
                    draw_rect(x*rw + 1, y*rh + 1, rw - 2, rh - 2);
                    int esz = cw / 5;
                    if(esz < 2) esz = 2;
                    SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 0xff);
                    if(dx == 1){
                        draw_rect(cx + cw - esz*2, cy + ch/4, esz, esz);
                        draw_rect(cx + cw - esz*2, cy + ch*3/4 - esz, esz, esz);
                    }
                    else if(dx == -1){
                        draw_rect(cx + esz, cy + ch/4, esz, esz);
                        draw_rect(cx + esz, cy + ch*3/4 - esz, esz, esz);
                    }
                    else if(dy == -1){
                        draw_rect(cx + cw/4, cy + esz, esz, esz);
                        draw_rect(cx + cw*3/4 - esz, cy + esz, esz, esz);
                    }
                    else {
                        draw_rect(cx + cw/4, cy + ch - esz*2, esz, esz);
                        draw_rect(cx + cw*3/4 - esz, cy + ch - esz*2, esz, esz);
                    }
                }
                else if(!val) {
                    // checkerboard
                    if((x + y) % 2 == 0)
                        SDL_SetRenderDrawColor(renderer, 0x16, 0x16, 0x3a, 0xff);
                    else
                        SDL_SetRenderDrawColor(renderer, 0x1e, 0x1e, 0x46, 0xff);
                    draw_rect(cx, cy, cw, ch);
                }
                else if(val == -1){
                    // Apple
                    int pulse = gtick % 8;
                    int shrink = 0;
                    if(pulse < 4) shrink = pulse;
                    else shrink = 8 - pulse;
                    SDL_SetRenderDrawColor(renderer, 0xff, 0x22, 0x22, 0xff);
                    draw_rect(cx + shrink, cy + shrink, cw - shrink*2, ch - shrink*2);
                    // Apple highlight
                    SDL_SetRenderDrawColor(renderer, 0xff, 0x88, 0x88, 0x90);
                    int hlsz = cw / 4;
                    if(hlsz < 2) hlsz = 2;
                    draw_rect(cx + shrink + 2, cy + shrink + 2, hlsz, hlsz);
                }
                else {
                    // Snake body
                    int idx = snake_index(x, y);
                    int g = 0x60;
                    if(glen > 1 && idx >= 0)
                        g = 0x60 + (0x99 * idx) / glen;
                    if(g > 0xff) g = 0xff;
                    SDL_SetRenderDrawColor(renderer, 0x0, g, 0x20, 0xff);
                    draw_rect(cx, cy, cw, ch);
                    // Inner highlight
                    SDL_SetRenderDrawColor(renderer, 0x20, g + 0x20 > 0xff ? 0xff : g + 0x20, 0x40, 0x80);
                    draw_rect(cx + 2, cy + 2, cw/2 - 1, ch/2 - 1);
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 0xff, 0xcc, 0x0, 0xc0);
        int score = glen - 1;
        for(int s = 0; s < score; s = s + 1){
            int scx = 4 + s * 10;
            int scy = 4;
            draw_rect(scx, scy, 7, 7);
        }
    }
    SDL_RenderPresent(renderer);
}

unsigned rng_state;
void init_rng(void){ rng_state = (unsigned)SDL_GetPerformanceCounter(); }

unsigned
rng(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
