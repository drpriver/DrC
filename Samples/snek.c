// Snek game
#ifdef __linux__
#error "FIXME: pragma pkg_config or something?"
#elif defined __APPLE__
#pragma lib "SDL2"
#else
#warning "FIXME: idk how to find sdl on windows, maybe user has to call cli args?"
#endif
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

SDL_Window* gwindow;
SDL_Renderer* grenderer;
typedef signed char Tile;
Tile* gboard;
enum { NONE, WIN, LOSE} gwinlose = NONE;
Tile** gsnake;
int glen;
int gpaused;
int gtick; // global tick counter for animations
void main_loop(void);
void render_and_present(int sx, int sy, int dx, int dy);
enum {BOARD_SIZE=10, HALF_BOARD_SIZE=5, PAD=2};
void open_window_and_renderer(int width, int height){
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow(
        "Snek!",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        width, height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE
    );
    if(!window){
        printf("No window!\n");
        abort();
    }
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    if(!renderer) {printf("No renderer!\n"); abort();}
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    int ww, wh;
    SDL_GetWindowSize(window, &ww, &wh);
    int rw, rh;
    SDL_GetRendererOutputSize(renderer, &rw, &rh);
    if(rw != ww || rh != wh){
        SDL_RenderSetScale(renderer, (float)rw/ww, (float)rh/wh);
    }
    gwindow = window;
    grenderer = renderer;
}
int start(int width, int height){
  srand(time(NULL));
  void* window;
  void* renderer;
  if(width < 400) width = 400;
  if(width > 1200) width = 1200;
  if(height < 400) height = 400;
  if(height > 1200) height = 1200;
  open_window_and_renderer(width, height);
  Tile* board = calloc(BOARD_SIZE*BOARD_SIZE, sizeof *board);
  gboard = board;
  Tile** snake = calloc(BOARD_SIZE*BOARD_SIZE, sizeof *snake);
  gsnake = snake;
  main_loop();
  free(board);
  free(snake);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

void grow_window(){
  int w; int h;
  SDL_GetWindowSize(gwindow, &w, &h);
  w = w + 50;
  h = h + 50;
  if(w > 1200) w = 1200;
  if(h > 1200) h = 1200;
  SDL_SetWindowSize(gwindow, w, h);
}

void shrink_window(){
  int w; int h;
  SDL_GetWindowSize(gwindow, &w, &h);
  w = w - 50;
  h = h - 50;
  if(w < 100) w = 100;
  if(h < 100) h = 100;
  SDL_SetWindowSize(gwindow, w, h);
}

int has_apple(Tile* board){
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
void place_apple(Tile* board){
  for(;;){
    int y = rand() % BOARD_SIZE;
    int x = rand() % BOARD_SIZE;
    Tile* p = y*BOARD_SIZE+x+board;
    if(!*p){
      *p = -1;
      return;
    }
  }
}

void move_snake(Tile* board, Tile** snake, int x, int y){
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
    if(glen >= BOARD_SIZE * BOARD_SIZE / 8){
      gwinlose = WIN;
      return;
    }
    gwinlose = LOSE;
    return;
  }
  int l = glen-1;
  Tile* ps = *snake;
  *ps = 0;
  for(int i = 0; i < l; i = i + 1){
    snake[i] = snake[i+1];
  }
  snake[glen-1] = p;
}

void simulate(Tile* board, int x, int y){
  move_snake(board, gsnake, x, y);
  if(gwinlose) return;
  int apple = has_apple(board);
  if(!apple) place_apple(board);
}

void main_loop(void){
  Tile* board = gboard;
  SDL_Event* event = calloc(1, sizeof(SDL_Event));
  int poll = 1;
  int t = SDL_GetTicks();
  int trigger = 10;
  int tick = trigger-1;
  board[HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE] = 1;
  gsnake[0] = board +HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE;
  glen = 1;
  int dx = 0;
  int dy = 1;
  int x = HALF_BOARD_SIZE;
  int y = HALF_BOARD_SIZE;
  int turned = 0;
  for(int i = 0;;i++){
    int got_event = 0;
    if(poll){
      if(!SDL_PollEvent(event)){
        tick = tick + 1;
        if(tick >= trigger){
          tick = 0;
          if(!gwinlose){
            if(!gpaused){
              x = x + dx;
              y = y + dy;
              if(x == -1) x = BOARD_SIZE-1;
              if(y == -1) y = BOARD_SIZE-1;
              if(x == BOARD_SIZE) x = 0;
              if(y == BOARD_SIZE) y = 0;
              simulate(board, x, y);
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
      SDL_WaitEvent(event);
      poll = 1;
    }
    int type = event->type;
    if(type == SDL_QUIT){
      break;
    }
    else if(type == SDL_KEYDOWN){
      SDL_Keycode code = event->key.keysym.sym;
      if(code == 'q') break;
      else if(code == ' ') gpaused = !gpaused;
      else if(code == '=') grow_window();
      else if(code == '-') shrink_window();
      else if(code == 'a'){
        if(dx != 1){
          dx = -1;
          dy = 0;
        }
      }
      else if(code == 'r'){
        gwinlose = NONE;
        tick = trigger-1;
        memset(board, 0, BOARD_SIZE*BOARD_SIZE* sizeof *board);
        x = HALF_BOARD_SIZE;
        y = HALF_BOARD_SIZE;
        board[HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE] = 1;
        gsnake[0] = board +HALF_BOARD_SIZE*BOARD_SIZE+HALF_BOARD_SIZE;
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
  free(event);
}

void draw_rect(void* renderer, int x, int y, int w, int h){
    SDL_Rect r = {x, y, w, h};
    SDL_RenderFillRect(renderer, &r);
}

void draw_banner(void* renderer, const char* banner, int cols, int rows, int ww, int hh){
    for(int r = 0; r < rows; r = r + 1){
        for(int c = 0; c < cols; c = c + 1){
            if(banner[r * cols + c] == 'x')
                draw_rect(renderer, c * ww, r * hh, ww, hh);
        }
    }
}

// Find position of a snake segment in the gsnake array.
// Returns 0 for tail, glen-1 for segment just behind head, -1 if not found.
int snake_index(Tile* board, int bx, int by){
  Tile* p = board + by*BOARD_SIZE + bx;
  for(int i = 0; i < glen; i = i + 1){
    if(gsnake[i] == p) return i;
  }
  return -1;
}

void render_and_present(int sx, int sy, int dx, int dy){
  int w = 640;
  int h = 480;
  void* window = gwindow;
  void* renderer = grenderer;
  Tile* board = gboard;
  int winlose = gwinlose;
  SDL_GetWindowSize(window, &w, &h);
  // Dark background
  SDL_SetRenderDrawColor(renderer, 0x1a, 0x1a, 0x2e, 0xff);
  SDL_RenderClear(renderer);

  int ww = w/20;
  int hh = h/20;
  const char* win_banner =
    "                    "
    "                    "
    "                    "
    "    x x xxx x x     "
    "    x x x x x x     "
    "    x x x x x x     "
    "     x  x x x x     "
    "     x  xxx xxx     "
    "                    "
    "   x   x xxx x   x  "
    "   x   x  x  xx  x  "
    "   x x x  x  x x x  "
    "   x x x  x  x  xx  "
    "    x x  xxx x   x  "
    "                    "
    "                    "
    "                    "
    "                    "
    "                    "
    "                    ";
  const char* lose_banner =
    "                    "
    "                    "
    "                    "
    "                    "
    "    x x xxx x x     "
    "    x x x x x x     "
    "    x x x x x x     "
    "     x  x x x x     "
    "     x  xxx xxx     "
    "                    "
    "  x   xxx xxx xxx   "
    "  x   x x x   x     "
    "  x   x x xxx xxx   "
    "  x   x x   x x     "
    "  xxx xxx xxx xxx   "
    "                    "
    "                    "
    "                    "
    "                    "
    "                    ";
  if(winlose == WIN){
    SDL_SetRenderDrawColor(renderer, 0x0, 0xff, 0x0, 0xff);
    draw_banner(renderer, win_banner, 20, 20, ww, hh);
  }
  else if(winlose == LOSE){
    SDL_SetRenderDrawColor(renderer, 0xff, 0x44, 0x44, 0xff);
    draw_banner(renderer, lose_banner, 20, 20, ww, hh);
  }
  else {
    int rw = w/BOARD_SIZE;
    int rh = h/BOARD_SIZE;

    // Draw grid background (dark lines between cells)
    // We draw slightly smaller cells with padding to create a grid effect
    for(int y = 0; y < BOARD_SIZE; y = y + 1){
      for(int x = 0; x < BOARD_SIZE; x = x + 1){
        Tile val = *(y*BOARD_SIZE+x+board);
        int cx = x*rw + PAD;
        int cy = y*rh + PAD;
        int cw = rw - PAD*2;
        int ch = rh - PAD*2;

        if((x == sx) & (y == sy)){
          // Head - bright cyan with a slightly larger cell
          SDL_SetRenderDrawColor(renderer, 0x0, 0xee, 0xff, 0xff);
          draw_rect(renderer, x*rw + 1, y*rh + 1, rw - 2, rh - 2);
          // Eyes: two dark squares on the head
          int esz = cw / 5;
          if(esz < 2) esz = 2;
          SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x10, 0xff);
          if(dx == 1){
            // facing right
            draw_rect(renderer, cx + cw - esz*2, cy + ch/4, esz, esz);
            draw_rect(renderer, cx + cw - esz*2, cy + ch*3/4 - esz, esz, esz);
          }
          else if(dx == -1){
            // facing left
            draw_rect(renderer, cx + esz, cy + ch/4, esz, esz);
            draw_rect(renderer, cx + esz, cy + ch*3/4 - esz, esz, esz);
          }
          else if(dy == -1){
            // facing up
            draw_rect(renderer, cx + cw/4, cy + esz, esz, esz);
            draw_rect(renderer, cx + cw*3/4 - esz, cy + esz, esz, esz);
          }
          else {
            // facing down
            draw_rect(renderer, cx + cw/4, cy + ch - esz*2, esz, esz);
            draw_rect(renderer, cx + cw*3/4 - esz, cy + ch - esz*2, esz, esz);
          }
        }
        else if(!val) {
          // Empty - dark blue/navy with subtle checkerboard
          if((x + y) % 2 == 0)
            SDL_SetRenderDrawColor(renderer, 0x16, 0x16, 0x3a, 0xff);
          else
            SDL_SetRenderDrawColor(renderer, 0x1e, 0x1e, 0x46, 0xff);
          draw_rect(renderer, cx, cy, cw, ch);
        }
        else if(val == -1){
          // Apple - red with a pulsing size effect
          int pulse = gtick % 8;
          int shrink = 0;
          if(pulse < 4) shrink = pulse;
          else shrink = 8 - pulse;
          SDL_SetRenderDrawColor(renderer, 0xff, 0x22, 0x22, 0xff);
          draw_rect(renderer, cx + shrink, cy + shrink, cw - shrink*2, ch - shrink*2);
          // Apple highlight
          SDL_SetRenderDrawColor(renderer, 0xff, 0x88, 0x88, 0x90);
          int hlsz = cw / 4;
          if(hlsz < 2) hlsz = 2;
          draw_rect(renderer, cx + shrink + 2, cy + shrink + 2, hlsz, hlsz);
        }
        else {
          // Snake body - gradient from dark green (tail) to bright green (near head)
          int idx = snake_index(board, x, y);
          int g = 0x60;
          if(glen > 1 && idx >= 0)
            g = 0x60 + (0x99 * idx) / glen;
          if(g > 0xff) g = 0xff;
          SDL_SetRenderDrawColor(renderer, 0x0, g, 0x20, 0xff);
          draw_rect(renderer, cx, cy, cw, ch);
          // Inner highlight for 3d-ish look
          SDL_SetRenderDrawColor(renderer, 0x20, g + 0x20 > 0xff ? 0xff : g + 0x20, 0x40, 0x80);
          draw_rect(renderer, cx + 2, cy + 2, cw/2 - 1, ch/2 - 1);
        }
      }
    }

    // Score display: draw small squares in top-left for each apple eaten
    SDL_SetRenderDrawColor(renderer, 0xff, 0xcc, 0x0, 0xc0);
    int score = glen - 1;
    for(int s = 0; s < score; s = s + 1){
      int scx = 4 + s * 10;
      int scy = 4;
      draw_rect(renderer, scx, scy, 7, 7);
    }
  }
  SDL_RenderPresent(renderer);
}
int main(){
    start(640, 640);
}
