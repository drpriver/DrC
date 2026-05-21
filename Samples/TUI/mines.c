//usr/bin/env drc "$0" "$@"; exit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

enum {MAX_X=30, MAX_Y=30, INIT_X=20, INIT_Y=20};
int num_mines,
    cells_x = INIT_X, cells_y = INIT_Y,
    cur_x = INIT_X/2-1, cur_y = INIT_Y/2-1,
    n_revealed = 0;
unsigned rng;
struct {
    _Bool mine: 1, flag: 1, revealed: 1;
} board[MAX_X*MAX_Y];
enum {
    GAME_NOT_STARTED,
    GAME_STARTED,
    GAME_WON,
    GAME_LOST,
} state = GAME_NOT_STARTED;
_Bool darkmode;
struct termios orig_termios;

unsigned rand32(void);
void draw(void),
     click(int x, int y, _Bool shift),
     reveal(int x, int y),
     scan(int x, int y, int *mines, int *flagged, int *hidden, int hi[8]),
     autoplay(void),
     cheat(void),
     restore_terminal(void),
     setup_terminal(void);
_Bool check_subset(int ahi[8], int ahidden, int bhi[8], int bhidden),
      contains(int hi[8], int h, int v);
int read_key(void),
    count_neighbors(int x, int y);

enum {KEY_UP=1000, KEY_DOWN, KEY_LEFT, KEY_RIGHT, MOUSE_LEFT, MOUSE_RIGHT};
int mouse_col, mouse_row;

unsigned
rand32(void){
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

int
count_neighbors(int x, int y){
    int n = 0;
    for(int dy = -1; dy <= 1; dy++)
    for(int dx = -1; dx <= 1; dx++){
        if(!dx && !dy) continue;
        int ix = x + dx, iy = y + dy;
        if(ix < 0 || ix >= cells_x) continue;
        if(iy < 0 || iy >= cells_y) continue;
        n += board[iy*cells_x+ix].mine;
    }
    return n;
}

void
draw(void){
    static const char* light_nums[8] = {
        "\033[38;2;0;0;255m",     // blue
        "\033[38;2;0;128;0m",     // green
        "\033[38;2;255;0;0m",     // red
        "\033[38;2;0;0;128m",     // navy
        "\033[38;2;128;0;0m",     // maroon
        "\033[38;2;0;128;128m",   // teal
        "\033[38;2;0;0;0m",       // black
        "\033[38;2;128;128;128m", // gray
    };
    static const char* dark_nums[8] = {
        "\033[38;2;96;160;255m",
        "\033[38;2;64;255;64m",
        "\033[38;2;255;96;96m",
        "\033[38;2;160;160;255m",
        "\033[38;2;255;160;64m",
        "\033[38;2;64;255;255m",
        "\033[38;2;255;255;255m",
        "\033[38;2;192;192;192m",
    };
    const char** nums     = darkmode? dark_nums : light_nums;
    const char* hidden_bg = darkmode? "\033[48;2;96;96;96m"   : "\033[48;2;192;192;192m";
    const char* flag_fg   = darkmode? "\033[38;2;255;64;64m"  : "\033[38;2;192;0;0m";
    const char* cursor_bg = darkmode? "\033[48;2;0;192;192m"  : "\033[48;2;0;255;255m";
    const char* mine_bad  = "\033[48;2;240;48;48m";
    const char* mine_good = "\033[48;2;48;240;48m";
    const char* mine_fg   = darkmode? "\033[38;2;255;255;255m" : "\033[38;2;0;0;0m";

    printf("\033[H");
    for(int y = 0; y < cells_y; y++){
        for(int x = 0; x < cells_x; x++){
            int i = y*cells_x + x;
            _Bool is_cur = (x == cur_x && y == cur_y);
            const char* bg = "\033[49m";
            const char* fg = "\033[39m";
            const char* text = "  ";
            char numbuf[8];
            if(state == GAME_NOT_STARTED || (state == GAME_STARTED && !board[i].revealed)){
                bg = hidden_bg;
                if(state == GAME_STARTED && board[i].flag){
                    fg = flag_fg;
                    text = "F ";
                }
            }
            else if(board[i].mine){
                bg = (state == GAME_WON || board[i].flag)? mine_good : mine_bad;
                fg = mine_fg;
                text = board[i].flag? "F " : "* ";
            }
            else {
                int n = count_neighbors(x, y);
                if(n){
                    fg = nums[n-1];
                    snprintf(numbuf, sizeof numbuf, "%d ", n);
                    text = numbuf;
                }
            }
            if(is_cur) bg = cursor_bg;
            printf("%s%s%s\033[0m", bg, fg, text);
        }
        printf("\033[K\n");
    }

    const char* status;
    switch(state){
        default:
        case GAME_NOT_STARTED: status = "ready"; break;
        case GAME_STARTED:     status = "playing"; break;
        case GAME_WON:         status = "YOU WON!"; break;
        case GAME_LOST:        status = "GAME OVER"; break;
    }
    printf("\033[K%s  size %dx%d  mines %d  revealed %d/%d\n",
        status, cells_x, cells_y, num_mines, n_revealed,
        cells_x*cells_y - num_mines);
    printf("\033[Khjkl/arrows or mouse: move/dig  rclick/f: flag  p: auto  c: cheat  r: restart  d: dark  q: quit");
    if(state == GAME_NOT_STARTED) printf("  +/-/0: resize");
    printf("\033[K\n");
    fflush(stdout);
}

void
click(int x, int y, _Bool shift){
    if(state == GAME_NOT_STARTED){
        if(shift) return;
        memset(board, 0, sizeof board);
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

void
reveal(int x, int y){
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
        if(cy < 0 || cy >= cells_y) continue;
        for(int dx = -1; dx <= 1; dx++){
            if(dx == 0 && dy == 0) continue;
            int cx = x + dx;
            if(cx < 0 || cx >= cells_x) continue;
            if(board[cy*cells_x+cx].mine) neighbors++;
        }
    }
    if(!neighbors){
        for(int dy = -1; dy <= 1; dy++){
            int cy = y + dy;
            if(cy < 0 || cy >= cells_y) continue;
            for(int dx = -1; dx <= 1; dx++){
                if(dx == 0 && dy == 0) continue;
                int cx = x + dx;
                if(cx < 0 || cx >= cells_x) continue;
                reveal(cx, cy);
            }
        }
    }
    if(n_revealed + num_mines == cells_x * cells_y)
        state = GAME_WON;
}

_Bool
contains(int hi[8], int h, int v){
    for(int i = 0; i < h; i++) if(hi[i] == v) return 1;
    return 0;
}
_Bool
check_subset(int ahi[8], int ahidden, int bhi[8], int bhidden){
    for(int a = 0; a < ahidden; a++) if(!contains(bhi, bhidden, ahi[a])) return 0;
    return 1;
}
void
scan(int x, int y, int *mines, int *flagged, int *hidden, int hi[8]){
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
void
autoplay(void){
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

void
cheat(void){
    if(state != GAME_STARTED) return;
    int idx = cur_y * cells_x + cur_x;
    if(!board[idx].flag) click(cur_x, cur_y, board[idx].mine);
}

void
restore_terminal(void){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    printf("\033[?1006l\033[?1000l\033[?25h\033[?1049l");
    fflush(stdout);
}

void
setup_terminal(void){
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    printf("\033[?1049h\033[?25l\033[?1000h\033[?1006h\033[2J");
    fflush(stdout);
}

int
read_key(void){
    unsigned char c;
    if(read(STDIN_FILENO, &c, 1) != 1) return -1;
    if(c != '\033') return c;
    unsigned char seq[2];
    if(read(STDIN_FILENO, &seq[0], 1) != 1) return '\033';
    if(seq[0] != '[' && seq[0] != 'O') return '\033';
    if(read(STDIN_FILENO, &seq[1], 1) != 1) return '\033';
    if(seq[0] == '[' && seq[1] == '<'){
        // SGR mouse: \033[<b;col;row;M (press) or m (release)
        int parts[3] = {0,0,0};
        int pi = 0;
        unsigned char final = 0;
        for(;;){
            if(read(STDIN_FILENO, &c, 1) != 1) return 0;
            if(c >= '0' && c <= '9'){
                if(pi < 3) parts[pi] = parts[pi]*10 + (c - '0');
            }
            else if(c == ';'){
                if(pi < 2) pi++;
            }
            else if(c == 'M' || c == 'm'){
                final = c;
                break;
            }
            else return 0;
        }
        if(final != 'M') return 0;
        int b = parts[0];
        if(b & 32) return 0;  // motion
        if(b & 64) return 0;  // scroll
        mouse_col = parts[1] - 1;
        mouse_row = parts[2] - 1;
        _Bool shift = (b & 4) != 0;
        int btn = b & 3;
        if(btn == 0) return shift? MOUSE_RIGHT : MOUSE_LEFT;
        if(btn == 2) return MOUSE_RIGHT;
        return 0;
    }
    switch(seq[1]){
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
    }
    return '\033';
}

int
main(int argc, char** argv){
    {
        const char* lm = getenv("LIGHTMODE");
        if(lm && *lm && !atoi(lm)) darkmode = 1;
        const char* dm = getenv("DARKMODE");
        if(dm && *dm && atoi(dm)) darkmode = 1;
    }
    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-d") == 0) darkmode = 1;
        else if(strcmp(argv[i], "--dark-mode") == 0) darkmode = 1;
        else if(strcmp(argv[i], "-l") == 0) darkmode = 0;
        else if(strcmp(argv[i], "--light-mode") == 0) darkmode = 0;
    }
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    rng = (unsigned)(ts.tv_nsec ^ ts.tv_sec);
    if(!rng) rng = 1;

    setup_terminal();
    for(;;){
        draw();
        int k = read_key();
        if(k < 0) break;
        switch(k){
            case 'q': case 3: case 4: // q, ^C, ^D
                goto finally;
            case 'd':
                darkmode = !darkmode;
                break;
            case 'p':
                autoplay();
                break;
            case 'c':
                cheat();
                break;
            case 'n': case 'r':
                state = GAME_NOT_STARTED;
                printf("\033[2J");
                break;
            case 'k': case KEY_UP:
                if(cur_y > 0) cur_y--;
                break;
            case 'j': case KEY_DOWN:
                if(cur_y < cells_y-1) cur_y++;
                break;
            case 'h': case KEY_LEFT:
                if(cur_x > 0) cur_x--;
                break;
            case 'l': case KEY_RIGHT:
                if(cur_x < cells_x-1) cur_x++;
                break;
            case ' ': case '\r': case '\n':
                click(cur_x, cur_y, 0);
                break;
            case 'f': case 'F':
                click(cur_x, cur_y, 1);
                break;
            case MOUSE_LEFT: case MOUSE_RIGHT: {
                int cx = mouse_col / 2, cy = mouse_row;
                if(cx >= 0 && cx < cells_x && cy >= 0 && cy < cells_y){
                    cur_x = cx;
                    cur_y = cy;
                    click(cx, cy, k == MOUSE_RIGHT);
                }
            } break;
            case '-': case '_':
                if(state == GAME_NOT_STARTED && cells_y > 10){
                    cells_y--; cells_x--;
                    if(cur_x >= cells_x) cur_x = cells_x-1;
                    if(cur_y >= cells_y) cur_y = cells_y-1;
                    printf("\033[2J");
                }
                break;
            case '=': case '+':
                if(state == GAME_NOT_STARTED && cells_y < MAX_Y){
                    cells_y++; cells_x++;
                }
                break;
            case '0':
                if(state == GAME_NOT_STARTED){
                    cells_x = INIT_X; cells_y = INIT_Y;
                    printf("\033[2J");
                }
                break;
        }
    }
    finally:
    restore_terminal();
    return 0;
}
