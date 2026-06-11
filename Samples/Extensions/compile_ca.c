//usr/bin/env drc "$0" "$@"; exit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { W = 48, H = 20 };

typedef int RuleFn(int alive, int n, int x, int y, int t);

unsigned char grid[H][W];
unsigned char next_grid[H][W];
int tick;
char status[1024] = "blank steps; type a rule expression to compile it";

int rule(int alive, int n, int x, int y, int t){
    return n == 3 || (alive && n == 2);
}

void seed(void){
    for(int y = 0; y < H; y++)
        for(int x = 0; x < W; x++)
            grid[y][x] = rand() % 5 == 0;
}

int neighbors(int x, int y){
    int n = 0;
    for(int dy = -1; dy <= 1; dy++){
        for(int dx = -1; dx <= 1; dx++){
            if(!dx && !dy) continue;
            int xx = (x + dx + W) % W;
            int yy = (y + dy + H) % H;
            n += grid[yy][xx] != 0;
        }
    }
    return n;
}

void step(void){
    for(int y = 0; y < H; y++){
        for(int x = 0; x < W; x++){
            int alive = grid[y][x] != 0;
            int n = neighbors(x, y);
            next_grid[y][x] = rule(alive, n, x, y, tick) != 0;
        }
    }
    memcpy(grid, next_grid, sizeof grid);
    tick++;
}

void draw(void){
    printf("\033[H\033[J");
    printf("tick %d\n", tick);
    for(int y = 0; y < H; y++){
        for(int x = 0; x < W; x++)
            putchar(grid[y][x] ? '#' : '.');
        putchar('\n');
    }
    puts("");
    puts("Enter a rule expression using: alive, n, x, y, t");
    puts("Examples:");
    puts("  n == 3 || (alive && n == 2) // Life");
    puts("  n == 3 || (alive && (n == 2 || n == 4))");
    puts("  (n == 3) || (!alive && n == 2 && ((x + y + t) & 1))");
    puts("Commands: blank=step, random, quit");
    printf("status: %s\n", status);
}

int install_rule(const char* expr){
    char src[4096];
    int n = snprintf(src, sizeof src,
        "int user_rule(int alive, int n, int x, int y, int t){\n"
        "    return (%s) ? 1 : 0;\n"
        "}\n",
        expr);
    if(n < 0 || (size_t)n >= sizeof src){
        snprintf(status, sizeof status, "rule is too long");
        return 1;
    }

    _Module m = __compile(src);
    if(!m){
        snprintf(status, sizeof status, "compile failed; keeping old rule");
        return 1;
    }

    auto fp = m.symbol("user_rule", RuleFn);
    if(!fp){
        snprintf(status, sizeof status, "compiled module did not export user_rule");
        return 1;
    }

    if(__hotswap(rule, fp)){
        snprintf(status, sizeof status, "hotswap failed");
        return 1;
    }
    snprintf(status, sizeof status, "rule installed: %s", expr);
    return 0;
}

char line[1024];
seed();
for(;;){
    draw();
    printf("> ");
    fflush(stdout);
    if(!fgets(line, sizeof line, stdin))
        break;
    size_t len = strlen(line);
    if(len && line[len - 1] == '\n')
        line[--len] = 0;

    if(strcmp(line, "quit") == 0)
        break;
    if(strcmp(line, "random") == 0){
        seed();
        snprintf(status, sizeof status, "randomized grid");
        continue;
    }
    if(line[0])
        install_rule(line);
    step();
}
return 0;
