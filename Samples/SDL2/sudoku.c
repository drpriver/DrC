#ifdef __linux__
#pragma pkg_config "sdl2"
#endif
#pragma lib "SDL2"
#include <SDL2/SDL.h>

unsigned int rng_state;
unsigned int rng(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
enum { CELL = 56, PAD = 2, THICK = 4 };
enum { BOARD = 9 * CELL + 2 * THICK + 6 * PAD };
enum { NUMROW_Y = BOARD + 20 + 8 };
enum { NUMROW_PAD = 4 };
enum { NUMROW_CELL = (BOARD - 8 * NUMROW_PAD) / 9 };
enum { BTNROW_Y = NUMROW_Y + NUMROW_CELL + 8 };
enum { BTN_H = 36, BTN_PAD = 8, NBTN = 3 };
enum { BTN_W = (BOARD - (NBTN - 1) * BTN_PAD) / NBTN };
enum { WIDTH = BOARD + 20, HEIGHT = BTNROW_Y + BTN_H + 10 };

int puzzle[9][9];
int board[9][9];
int sel_r = 0, sel_c = 0;

unsigned short digits[10] = {
    0b000000000000000, // (blank)
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

void draw_bm(SDL_Renderer* ren, unsigned short bm, int cx, int cy, int sz){
    int x0 = cx - (3 * sz) / 2;
    int y0 = cy - (5 * sz) / 2;
    for(int r = 0; r < 5; r++)
        for(int c = 0; c < 3; c++)
            if((bm >> (14 - r * 3 - c)) & 1){
                SDL_Rect rc = {x0 + c * sz, y0 + r * sz, sz, sz};
                SDL_RenderFillRect(ren, &rc);
            }
}

void draw_text(SDL_Renderer* ren, const char* text, int cx, int cy, int sz){
    int len = 0;
    while(text[len]) len++;
    int charw = 3 * sz + sz;
    int x = cx - (len * charw - sz) / 2;
    int y = cy;
    for(int i = 0; i < len; i++){
        char ch = text[i];
        unsigned short bm = 0;
        if(ch >= 'A' && ch <= 'Z') bm = letters[ch - 'A'];
        else if(ch >= 'a' && ch <= 'z') bm = letters[ch - 'a'];
        else if(ch >= '1' && ch <= '9') bm = digits[ch - '0'];
        if(bm) draw_bm(ren, bm, x + i * charw + (3 * sz) / 2, y, sz);
    }
}

void draw_digit(SDL_Renderer* ren, int d, int cx, int cy, int sz){
    if(d < 1 || d > 9) return;
    draw_bm(ren, digits[d], cx, cy, sz);
}

int cell_x(int c){
    int box = c / 3;
    int incell = c % 3;
    return 10 + box * (3 * CELL + 2 * PAD + THICK) + incell * (CELL + PAD);
}
int cell_y(int r){
    int box = r / 3;
    int incell = r % 3;
    return 10 + box * (3 * CELL + 2 * PAD + THICK) + incell * (CELL + PAD);
}

_Bool has_conflict(int r, int c){
    int v = board[r][c];
    if(!v) return 0;
    // Row
    for(int i = 0; i < 9; i++)
        if(i != c && board[r][i] == v) return 1;
    // Col
    for(int i = 0; i < 9; i++)
        if(i != r && board[i][c] == v) return 1;
    // Box
    int br = (r / 3) * 3, bc = (c / 3) * 3;
    for(int i = br; i < br + 3; i++)
        for(int j = bc; j < bc + 3; j++)
            if((i != r || j != c) && board[i][j] == v) return 1;
    return 0;
}

_Bool is_solved(void){
    for(int r = 0; r < 9; r++)
        for(int c = 0; c < 9; c++){
            if(!board[r][c]) return 0;
            if(has_conflict(r, c)) return 0;
        }
    return 1;
}

int numrow_x(int i){
    return 10 + i * (NUMROW_CELL + NUMROW_PAD);
}

int numrow_hit(int mx, int my){
    if(my < NUMROW_Y || my >= NUMROW_Y + NUMROW_CELL) return 0;
    for(int i = 0; i < 9; i++){
        int x = numrow_x(i);
        if(mx >= x && mx < x + NUMROW_CELL)
            return i + 1;
    }
    return 0;
}

_Bool hit_test(int mx, int my, int* out_r, int* out_c){
    for(int r = 0; r < 9; r++)
        for(int c = 0; c < 9; c++){
            int x = cell_x(c), y = cell_y(r);
            if(mx >= x && mx < x + CELL && my >= y && my < y + CELL){
                *out_r = r;
                *out_c = c;
                return 1;
            }
        }
    return 0;
}

typedef struct Color Color;
struct Color {
    int r, g, b;
};

typedef struct Theme Theme;
struct Theme {
    Color bg,
          solved,
          selected,
          conflict_bg,
          normal,
          given,
          conflict,
          placed,
          thin_line,
          thick_line,
          numrow_on,
          numrow_off,
          numrow_text_on,
          numrow_text_off,
          btn_bg,
          btn_text;
};
Theme light = {
        {0xfa, 0xf8, 0xef},
        {0xd4, 0xed, 0xda},
        {0xbb, 0xde, 0xfb},
        {0xfc, 0xd5, 0xd5},
        {0xff, 0xff, 0xff},
        {0x33, 0x33, 0x33},
        {0xcc, 0x00, 0x00},
        {0x33, 0x66, 0xcc},
        {0xcc, 0xcc, 0xcc},
        {0x33, 0x33, 0x33},
        {0xe8, 0xe8, 0xe8},
        {0xf0, 0xf0, 0xf0},
        {0x33, 0x66, 0xcc},
        {0xcc, 0xcc, 0xcc},
        {0xbb, 0xad, 0xa0},
        {0xff, 0xff, 0xff},
    },
    dark = {
        {0x1a, 0x1a, 0x2e},
        {0x1a, 0x3a, 0x22},
        {0x2a, 0x5e, 0x8a},
        {0x4a, 0x1a, 0x1a},
        {0x2a, 0x2a, 0x3e},
        {0xc0, 0xc0, 0xc0},
        {0xff, 0x44, 0x44},
        {0x55, 0x99, 0xff},
        {0x44, 0x44, 0x58},
        {0xaa, 0xaa, 0xbb},
        {0x33, 0x33, 0x48},
        {0x22, 0x22, 0x33},
        {0x55, 0x99, 0xff},
        {0x55, 0x55, 0x66},
        {0x44, 0x3e, 0x38},
        {0xe0, 0xe0, 0xe0},
    };
Theme* thm = &dark;

void draw(SDL_Renderer* ren){
    int solved = is_solved();
    // Background
    SDL_SetRenderDrawColor(ren, thm.bg.r, thm.bg.g, thm.bg.b, 0xff);
    SDL_RenderClear(ren);
    for(int r = 0; r < 9; r++){
        for(int c = 0; c < 9; c++){
            int x = cell_x(c), y = cell_y(r);
            int selected = (r == sel_r && c == sel_c);
            int given = puzzle[r][c] != 0;
            int conflict = has_conflict(r, c);
            // Cell background
            if(solved)
                SDL_SetRenderDrawColor(ren, thm.solved.r, thm.solved.g, thm.solved.b, 0xff);
            else if(selected)
                SDL_SetRenderDrawColor(ren, thm.selected.r, thm.selected.g, thm.selected.b, 0xff);
            else if(conflict && board[r][c])
                SDL_SetRenderDrawColor(ren, thm.conflict_bg.r, thm.conflict_bg.g, thm.conflict_bg.b, 0xff);
            else
                SDL_SetRenderDrawColor(ren, thm.normal.r, thm.normal.g, thm.normal.b, 0xff);
            SDL_Rect cr = {x, y, CELL, CELL};
            SDL_RenderFillRect(ren, &cr);
            // Number
            if(board[r][c]){
                if(given)
                    SDL_SetRenderDrawColor(ren, thm.given.r, thm.given.g, thm.given.b, 0xff);
                else if(conflict)
                    SDL_SetRenderDrawColor(ren, thm.conflict.r, thm.conflict.g, thm.conflict.b, 0xff);
                else
                    SDL_SetRenderDrawColor(ren, thm.placed.r, thm.placed.g, thm.placed.b, 0xff);
                draw_digit(ren, board[r][c], x + CELL / 2, y + CELL / 2, 5);
            }
        }
    }
    // Grid lines
    SDL_SetRenderDrawColor(ren, thm.thin_line.r, thm.thin_line.g, thm.thin_line.b, 0xff);
    for(int br = 0; br < 3; br++){
        for(int bc = 0; bc < 3; bc++){
            for(int j = 1; j < 3; j++){
                int x = cell_x(bc * 3 + j) - PAD;
                int y0 = cell_y(br * 3);
                int y1 = cell_y(br * 3 + 2) + CELL;
                SDL_Rect vr = {x, y0, PAD, y1 - y0};
                SDL_RenderFillRect(ren, &vr);
            }
            for(int j = 1; j < 3; j++){
                int y = cell_y(br * 3 + j) - PAD;
                int x0 = cell_x(bc * 3);
                int x1 = cell_x(bc * 3 + 2) + CELL;
                SDL_Rect hr = {x0, y, x1 - x0, PAD};
                SDL_RenderFillRect(ren, &hr);
            }
        }
    }
    // Thick lines between boxes
    SDL_SetRenderDrawColor(ren, thm.thick_line.r, thm.thick_line.g, thm.thick_line.b, 0xff);
    for(int i = 1; i < 3; i++){
        int x = cell_x(i * 3) - THICK;
        SDL_Rect vr = {x, 10, THICK, BOARD};
        SDL_RenderFillRect(ren, &vr);
    }
    for(int i = 1; i < 3; i++){
        int y = cell_y(i * 3) - THICK;
        SDL_Rect hr = {10, y, BOARD, THICK};
        SDL_RenderFillRect(ren, &hr);
    }
    // Outer border
    SDL_Rect top = {10 - THICK, 10 - THICK, BOARD + 2 * THICK, THICK};
    SDL_Rect bot = {10 - THICK, 10 + BOARD, BOARD + 2 * THICK, THICK};
    SDL_Rect lft = {10 - THICK, 10, THICK, BOARD};
    SDL_Rect rgt = {10 + BOARD, 10, THICK, BOARD};
    SDL_RenderFillRect(ren, &top);
    SDL_RenderFillRect(ren, &bot);
    SDL_RenderFillRect(ren, &lft);
    SDL_RenderFillRect(ren, &rgt);
    // Number row at bottom
    for(int i = 0; i < 9; i++){
        int x = numrow_x(i);
        int v = i + 1;
        _Bool allowed = !puzzle[sel_r][sel_c]; // can't edit givens
        if(allowed){
            for(int k = 0; k < 9; k++){
                if(board[sel_r][k] == v) { allowed = 0; break; }
                if(board[k][sel_c] == v) { allowed = 0; break; }
            }
        }
        if(allowed){
            int br = (sel_r / 3) * 3, bc = (sel_c / 3) * 3;
            for(int ri = br; ri < br + 3 && allowed; ri++)
                for(int ci = bc; ci < bc + 3; ci++)
                    if(board[ri][ci] == v) { allowed = 0; break; }
        }
        if(allowed)
            SDL_SetRenderDrawColor(ren, thm.numrow_on.r, thm.numrow_on.g, thm.numrow_on.b, 0xff);
        else
            SDL_SetRenderDrawColor(ren, thm.numrow_off.r, thm.numrow_off.g, thm.numrow_off.b, 0xff);
        SDL_Rect nr = {x, NUMROW_Y, NUMROW_CELL, NUMROW_CELL};
        SDL_RenderFillRect(ren, &nr);
        if(allowed)
            SDL_SetRenderDrawColor(ren, thm.numrow_text_on.r, thm.numrow_text_on.g, thm.numrow_text_on.b, 0xff);
        else
            SDL_SetRenderDrawColor(ren, thm.numrow_text_off.r, thm.numrow_text_off.g, thm.numrow_text_off.b, 0xff);
        draw_digit(ren, v, x + NUMROW_CELL / 2, NUMROW_Y + NUMROW_CELL / 2, 5);
    }
    // Button row
    const char* btn_labels[] = {"RESET", "CHEAT", "NEXT"};
    for(int i = 0; i < NBTN; i++){
        int x = 10 + i * (BTN_W + BTN_PAD);
        SDL_SetRenderDrawColor(ren, thm.btn_bg.r, thm.btn_bg.g, thm.btn_bg.b, 0xff);
        SDL_Rect br = {x, BTNROW_Y, BTN_W, BTN_H};
        SDL_RenderFillRect(ren, &br);
        SDL_SetRenderDrawColor(ren, thm.btn_text.r, thm.btn_text.g, thm.btn_text.b, 0xff);
        draw_text(ren, btn_labels[i], x + BTN_W / 2, BTNROW_Y + BTN_H / 2, 3);
    }
}

const char* puzzles[] = {
    "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79",
    "..9748...7.........2.1.9.....7...24..64.1.59..98...3.....8.3.2.........6...2759..",
    "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
};
enum { NPUZZLES = sizeof puzzles / sizeof puzzles[0] };

int gen[9][9]; // scratch
// bit i set means digit i+1 is used
int row_mask[9], col_mask[9], box_mask[9];

void gen_init_masks(void){
    for(int i = 0; i < 9; i++) row_mask[i] = col_mask[i] = box_mask[i] = 0;
    for(int r = 0; r < 9; r++)
        for(int c = 0; c < 9; c++)
            if(gen[r][c]){
                int bit = 1 << (gen[r][c] - 1);
                row_mask[r] |= bit;
                col_mask[c] |= bit;
                box_mask[(r/3)*3 + c/3] |= bit;
            }
}

int gen_candidates(int r, int c){
    return ~(row_mask[r] | col_mask[c] | box_mask[(r/3)*3 + c/3]) & 0x1ff;
}

void gen_place(int r, int c, int v){
    gen[r][c] = v;
    int bit = 1 << (v - 1);
    row_mask[r] |= bit;
    col_mask[c] |= bit;
    box_mask[(r/3)*3 + c/3] |= bit;
}

void gen_remove(int r, int c, int v){
    gen[r][c] = 0;
    int bit = 1 << (v - 1);
    row_mask[r] &= ~bit;
    col_mask[c] &= ~bit;
    box_mask[(r/3)*3 + c/3] &= ~bit;
}

int gen_solve(int pos){
    if(pos == 81) return 1;
    int r = pos / 9, c = pos % 9;
    if(gen[r][c]) return gen_solve(pos + 1);
    int cands = gen_candidates(r, c);
    while(cands){
        int bit = cands & (-cands); // lowest set bit
        cands &= ~bit;
        int v = 0;
        int tmp = bit;
        while(tmp >>= 1) v++;
        v++;
        gen_place(r, c, v);
        if(gen_solve(pos + 1)) return 1;
        gen_remove(r, c, v);
    }
    return 0;
}

int gen_fill(int pos){
    if(pos == 81) return 1;
    int r = pos / 9, c = pos % 9;
    if(gen[r][c]) return gen_fill(pos + 1);
    int cands = gen_candidates(r, c);
    int vals[9];
    int nv = 0;
    int tmp = cands;
    while(tmp){
        int bit = tmp & (-tmp);
        tmp &= ~bit;
        int v = 0;
        int b = bit;
        while(b >>= 1) v++;
        vals[nv++] = v + 1;
    }
    for(int i = nv - 1; i > 0; i--){
        int j = rng() % (i + 1);
        int t = vals[i]; vals[i] = vals[j]; vals[j] = t;
    }
    for(int i = 0; i < nv; i++){
        gen_place(r, c, vals[i]);
        if(gen_fill(pos + 1)) return 1;
        gen_remove(r, c, vals[i]);
    }
    return 0;
}

// Count solutions (stop at 2), pick most constrained cell first
int gen_count;
void gen_count_solutions(void){
    if(gen_count >= 2) return;
    // Find empty cell with fewest candidates
    int best_r = -1, best_c = -1, best_n = 10, best_cands = 0;
    for(int r = 0; r < 9; r++){
        for(int c = 0; c < 9; c++){
            if(gen[r][c]) continue;
            int cands = gen_candidates(r, c);
            if(!cands) return; // dead end
            int n = __builtin_popcount(cands);
            if(n < best_n){
                best_n = n; best_r = r; best_c = c; best_cands = cands;
            }
            if(n == 1) goto found;
        }
    }
    found:
    if(best_r < 0){ gen_count++; return; }
    int cands = best_cands;
    while(cands && gen_count < 2){
        int bit = cands & (-cands);
        cands &= ~bit;
        int v = 0;
        int tmp = bit;
        while(tmp >>= 1) v++;
        v++;
        gen_place(best_r, best_c, v);
        gen_count_solutions();
        gen_remove(best_r, best_c, v);
    }
}

void generate_puzzle(void){
    unsigned int t0 = SDL_GetTicks();
    SDL_memset(gen, 0, sizeof gen);
    gen_init_masks();
    gen_fill(0);
    unsigned int t1 = SDL_GetTicks();
    // Copy solved board to puzzle
    SDL_memcpy(puzzle, gen, sizeof gen);
    // Remove cells, checking uniqueness
    int order[81];
    for(int i = 0; i < 81; i++) order[i] = i;
    for(int i = 80; i > 0; i--){
        int j = rng() % (i + 1);
        int t = order[i]; order[i] = order[j]; order[j] = t;
    }
    int removed = 0;
    for(int i = 0; i < 81 && removed < 55; i++){
        int r = order[i] / 9, c = order[i] % 9;
        int saved = puzzle[r][c];
        puzzle[r][c] = 0;
        SDL_memcpy(gen, puzzle, sizeof gen);
        gen_init_masks();
        gen_count = 0;
        gen_count_solutions();
        if(gen_count != 1)
            puzzle[r][c] = saved; // put it back
        else
            removed++;
    }
    unsigned int t2 = SDL_GetTicks();
    SDL_Log("generated puzzle: fill %ums, holes %ums, total %ums, %d removed", t1 - t0, t2 - t1, t2 - t0, removed);
    SDL_memcpy(board, puzzle, sizeof puzzle);
}

void reset_board(void){ SDL_memcpy(board, puzzle, sizeof puzzle); }

void load_puzzle(int idx){
    if(idx < NPUZZLES){
        const char* p = puzzles[idx];
        for(int i = 0; i < 81; i++){
            int v = (p[i] >= '1' && p[i] <= '9') ? p[i] - '0' : 0;
            puzzle[i / 9][i % 9] = v;
        }
    }
    else
        generate_puzzle();
    reset_board();
}

void cheat(void){
    if(puzzle[sel_r][sel_c]) return; // already a given
    SDL_memcpy(gen, board, sizeof gen);
    gen_init_masks();
    if(gen_solve(0))
        board[sel_r][sel_c] = gen[sel_r][sel_c];
}

// 0=reset, 1=solve, 2=next, -1=miss
int btn_hit(int mx, int my){
    if(my < BTNROW_Y || my >= BTNROW_Y + BTN_H) return -1;
    for(int i = 0; i < NBTN; i++){
        int x = 10 + i * (BTN_W + BTN_PAD);
        if(mx >= x && mx < x + BTN_W) return i;
    }
    return -1;
}

rng_state = __RANDOM__;
SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
SDL_Init(SDL_INIT_VIDEO);
SDL_Window* win = SDL_CreateWindow("Sudoku",
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    WIDTH, HEIGHT, SDL_WINDOW_SHOWN);
SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

int puzzle_idx = 0;
load_puzzle(puzzle_idx);

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
                reset_board();
                break;
            case SDLK_c:
                cheat();
                break;
            case SDLK_d:
                thm = (thm == &light) ? &dark : &light;
                break;
            case SDLK_n:
                puzzle_idx++;
                load_puzzle(puzzle_idx);
                break;
            case SDLK_UP:    if(sel_r > 0) sel_r--; break;
            case SDLK_DOWN:  if(sel_r < 8) sel_r++; break;
            case SDLK_LEFT:  if(sel_c > 0) sel_c--; break;
            case SDLK_RIGHT: if(sel_c < 8) sel_c++; break;
            case SDLK_BACKSPACE: case SDLK_DELETE: case SDLK_0:
                if(!puzzle[sel_r][sel_c])
                    board[sel_r][sel_c] = 0;
                break;
            default:
                if(ev.key.keysym.sym >= SDLK_1 && ev.key.keysym.sym <= SDLK_9){
                    if(!puzzle[sel_r][sel_c])
                        board[sel_r][sel_c] = ev.key.keysym.sym - SDLK_0;
                }
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN: {
            int r, c;
            if(hit_test(ev.button.x, ev.button.y, &r, &c)){
                sel_r = r;
                sel_c = c;
            }
            int num = numrow_hit(ev.button.x, ev.button.y);
            if(num && !puzzle[sel_r][sel_c])
                board[sel_r][sel_c] = num;
            int btn = btn_hit(ev.button.x, ev.button.y);
            if(btn == 0) reset_board();
            if(btn == 1) cheat();
            if(btn == 2){ puzzle_idx++; load_puzzle(puzzle_idx); }
            break;
        }
        }
    }
    draw(ren);
    SDL_RenderPresent(ren);
    SDL_Delay(16);
}

finish:
SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
#ifdef __APPLE__
SDL_PumpEvents();
#endif
SDL_Quit();
