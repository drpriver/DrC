#include <std.h>

// Conway's Game of Life with ANSI animation

enum { W = 60, H = 30, STEPS = 200 };

unsigned char grid[H][W];
unsigned char next[H][W];
memset(grid, 0, sizeof grid);

// seed: R-pentomino (a classic methuselah pattern)
int cx = W / 2, cy = H / 2;
grid[cy-1][cx]   = 1;
grid[cy-1][cx+1] = 1;
grid[cy][cx-1]   = 1;
grid[cy][cx]     = 1;
grid[cy+1][cx]   = 1;

for(int step = 0; step < STEPS; step++){
    // move cursor home
    printf("\033[H");
    // draw
    for(int y = 0; y < H; y++){
        for(int x = 0; x < W; x++)
            putchar(grid[y][x] ? '#' : ' ');
        putchar('\n');
    }
    printf("step %d/%d\n", step + 1, STEPS);
    // compute next generation
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
    // small delay
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 80000000};
    nanosleep(&ts, (struct timespec*)0);
}
