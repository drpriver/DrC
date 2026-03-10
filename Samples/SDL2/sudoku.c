#ifdef __linux__
#pragma pkg_config "sdl2"
#endif
#pragma lib "SDL2"
#include <SDL2/SDL.h>

// Sudoku game with SDL2 and no other dependencies.

// Simple xorshift32 RNG
unsigned int rng_state;
unsigned int rng(void){
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
// Click a cell, type 1-9 to fill, 0/Backspace/Delete to clear.
// Arrow keys to move. R to reset, Q/Escape to quit.

enum { CELL = 56, PAD = 2, THICK = 4 };
enum { BOARD = 9 * CELL + 2 * THICK + 6 * PAD };
enum { NUMROW_Y = BOARD + 20 + 8 };
enum { NUMROW_PAD = 4 };
enum { NUMROW_CELL = (BOARD - 8 * NUMROW_PAD) / 9 };
enum { BTNROW_Y = NUMROW_Y + NUMROW_CELL + 8 };
enum { BTN_H = 36, BTN_PAD = 8, NBTN = 3 };
enum { BTN_W = (BOARD - (NBTN - 1) * BTN_PAD) / NBTN };
enum { WIDTH = BOARD + 20, HEIGHT = BTNROW_Y + BTN_H + 10 };

int puzzle[9][9];  // original clues (0 = empty)
int board[9][9];   // current state
int sel_r = 0, sel_c = 0;

// Digit bitmaps (5 rows x 3 cols)
const char* digits[10] = {
    "..."
    "..."
    "..."
    "..."
    "...",  // 0 (blank)

    ".x."
    "xx."
    ".x."
    ".x."
    "xxx",  // 1

    "xxx"
    "..x"
    "xxx"
    "x.."
    "xxx",  // 2

    "xxx"
    "..x"
    "xxx"
    "..x"
    "xxx",  // 3

    "x.x"
    "x.x"
    "xxx"
    "..x"
    "..x",  // 4

    "xxx"
    "x.."
    "xxx"
    "..x"
    "xxx",  // 5

    "xxx"
    "x.."
    "xxx"
    "x.x"
    "xxx",  // 6

    "xxx"
    "..x"
    "..x"
    "..x"
    "..x",  // 7

    "xxx"
    "x.x"
    "xxx"
    "x.x"
    "xxx",  // 8

    "xxx"
    "x.x"
    "xxx"
    "..x"
    "xxx",  // 9
};

// Letter bitmaps (5x3) for button labels
const char* letters[26] = {
    ".x." "x.x" "xxx" "x.x" "x.x",  // A
    "xx." "x.x" "xx." "x.x" "xx.",  // B
    "xxx" "x.." "x.." "x.." "xxx",  // C
    "xx." "x.x" "x.x" "x.x" "xx.",  // D
    "xxx" "x.." "xx." "x.." "xxx",  // E
    "xxx" "x.." "xx." "x.." "x..",  // F
    "xxx" "x.." "x.x" "x.x" "xxx",  // G
    "x.x" "x.x" "xxx" "x.x" "x.x",  // H
    "xxx" ".x." ".x." ".x." "xxx",  // I
    "..x" "..x" "..x" "x.x" "xxx",  // J
    "x.x" "xx." "x.." "xx." "x.x",  // K
    "x.." "x.." "x.." "x.." "xxx",  // L
    "x.x" "xxx" "xxx" "x.x" "x.x",  // M
    "x.x" "xxx" "xxx" "x.x" "x.x",  // N
    "xxx" "x.x" "x.x" "x.x" "xxx",  // O
    "xxx" "x.x" "xxx" "x.." "x..",  // P
    "xxx" "x.x" "x.x" "xxx" "..x",  // Q
    "xxx" "x.x" "xx." "x.x" "x.x",  // R
    "xxx" "x.." "xxx" "..x" "xxx",  // S
    "xxx" ".x." ".x." ".x." ".x.",  // T
    "x.x" "x.x" "x.x" "x.x" "xxx",  // U
    "x.x" "x.x" "x.x" "x.x" ".x.",  // V
    "x.x" "x.x" "xxx" "xxx" "x.x",  // W
    "x.x" "x.x" ".x." "x.x" "x.x",  // X
    "x.x" "x.x" ".x." ".x." ".x.",  // Y
    "xxx" "..x" ".x." "x.." "xxx",  // Z
};

void draw_bm(SDL_Renderer* ren, const char* bm, int cx, int cy, int sz){
    int x0 = cx - (3 * sz) / 2;
    int y0 = cy - (5 * sz) / 2;
    for(int r = 0; r < 5; r++)
        for(int c = 0; c < 3; c++)
            if(bm[r * 3 + c] == 'x'){
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
        const char* bm = (void*)0;
        if(ch >= 'A' && ch <= 'Z') bm = letters[ch - 'A'];
        else if(ch >= 'a' && ch <= 'z') bm = letters[ch - 'a'];
        else if(ch >= '1' && ch <= '9') bm = digits[ch - '0'];
        if(bm) draw_bm(ren, bm, x + i * charw + (3 * sz) / 2, y, sz);
    }
}

void draw_digit(SDL_Renderer* ren, int d, int cx, int cy, int sz){
    if(d < 1 || d > 9) return;
    const char* bm = digits[d];
    int x0 = cx - (3 * sz) / 2;
    int y0 = cy - (5 * sz) / 2;
    for(int r = 0; r < 5; r++){
        for(int c = 0; c < 3; c++){
            if(bm[r * 3 + c] == 'x'){
                SDL_Rect rc = {x0 + c * sz, y0 + r * sz, sz, sz};
                SDL_RenderFillRect(ren, &rc);
            }
        }
    }
}

// Convert grid row,col to pixel position
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

// Check if placing val at (r,c) conflicts
int has_conflict(int r, int c){
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

int is_solved(void){
    for(int r = 0; r < 9; r++)
        for(int c = 0; c < 9; c++){
            if(!board[r][c]) return 0;
            if(has_conflict(r, c)) return 0;
        }
    return 1;
}

// Number row position
int numrow_x(int i){
    return 10 + i * (NUMROW_CELL + NUMROW_PAD);
}

// Hit test for number row: returns 1-9, or 0 if miss
int numrow_hit(int mx, int my){
    if(my < NUMROW_Y || my >= NUMROW_Y + NUMROW_CELL) return 0;
    for(int i = 0; i < 9; i++){
        int x = numrow_x(i);
        if(mx >= x && mx < x + NUMROW_CELL)
            return i + 1;
    }
    return 0;
}

// Hit test: which cell was clicked?
int hit_test(int mx, int my, int* out_r, int* out_c){
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

void draw(SDL_Renderer* ren){
    int solved = is_solved();

    // Background
    SDL_SetRenderDrawColor(ren, 0xfa, 0xf8, 0xef, 0xff);
    SDL_RenderClear(ren);

    for(int r = 0; r < 9; r++){
        for(int c = 0; c < 9; c++){
            int x = cell_x(c), y = cell_y(r);
            int selected = (r == sel_r && c == sel_c);
            int given = puzzle[r][c] != 0;
            int conflict = has_conflict(r, c);

            // Cell background
            if(solved){
                SDL_SetRenderDrawColor(ren, 0xd4, 0xed, 0xda, 0xff);
            } else if(selected){
                SDL_SetRenderDrawColor(ren, 0xbb, 0xde, 0xfb, 0xff);
            } else if(conflict && board[r][c]){
                SDL_SetRenderDrawColor(ren, 0xfc, 0xd5, 0xd5, 0xff);
            } else {
                SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
            }
            SDL_Rect cr = {x, y, CELL, CELL};
            SDL_RenderFillRect(ren, &cr);

            // Number
            if(board[r][c]){
                if(given)
                    SDL_SetRenderDrawColor(ren, 0x33, 0x33, 0x33, 0xff);
                else if(conflict)
                    SDL_SetRenderDrawColor(ren, 0xcc, 0x00, 0x00, 0xff);
                else
                    SDL_SetRenderDrawColor(ren, 0x33, 0x66, 0xcc, 0xff);
                draw_digit(ren, board[r][c], x + CELL / 2, y + CELL / 2, 5);
            }
        }
    }

    // Grid lines
    // Thin lines between cells within each box
    SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
    for(int br = 0; br < 3; br++){
        for(int bc = 0; bc < 3; bc++){
            // Vertical thin lines within this box
            for(int j = 1; j < 3; j++){
                int x = cell_x(bc * 3 + j) - PAD;
                int y0 = cell_y(br * 3);
                int y1 = cell_y(br * 3 + 2) + CELL;
                SDL_Rect vr = {x, y0, PAD, y1 - y0};
                SDL_RenderFillRect(ren, &vr);
            }
            // Horizontal thin lines within this box
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
    SDL_SetRenderDrawColor(ren, 0x33, 0x33, 0x33, 0xff);
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
        // Check if this digit is allowed at the selected cell
        int allowed = !puzzle[sel_r][sel_c]; // can't edit givens
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
        if(allowed){
            SDL_SetRenderDrawColor(ren, 0xe8, 0xe8, 0xe8, 0xff);
        } else {
            SDL_SetRenderDrawColor(ren, 0xf0, 0xf0, 0xf0, 0xff);
        }
        SDL_Rect nr = {x, NUMROW_Y, NUMROW_CELL, NUMROW_CELL};
        SDL_RenderFillRect(ren, &nr);
        if(allowed){
            SDL_SetRenderDrawColor(ren, 0x33, 0x66, 0xcc, 0xff);
        } else {
            SDL_SetRenderDrawColor(ren, 0xcc, 0xcc, 0xcc, 0xff);
        }
        draw_digit(ren, v, x + NUMROW_CELL / 2, NUMROW_Y + NUMROW_CELL / 2, 5);
    }

    // Button row
    const char* btn_labels[] = {"RESET", "CHEAT", "NEXT"};
    for(int i = 0; i < NBTN; i++){
        int x = 10 + i * (BTN_W + BTN_PAD);
        SDL_SetRenderDrawColor(ren, 0xbb, 0xad, 0xa0, 0xff);
        SDL_Rect br = {x, BTNROW_Y, BTN_W, BTN_H};
        SDL_RenderFillRect(ren, &br);
        SDL_SetRenderDrawColor(ren, 0xff, 0xff, 0xff, 0xff);
        draw_text(ren, btn_labels[i], x + BTN_W / 2, BTNROW_Y + BTN_H / 2, 3);
    }
}

// Built-in puzzles
const char* puzzles[] = {
    "53..7....6..195....98....6.8...6...34..8.3..17...2...6.6....28....419..5....8..79",
    "..9748...7.........2.1.9.....7...24..64.1.59..98...3.....8.3.2.........6...2759..",
    "..53.....8......2..7..1.5..4....53...1..7...6..32...8..6.5....9..4....3......97..",
};
enum { NPUZZLES = sizeof puzzles / sizeof puzzles[0] };

// --- Puzzle generator ---

int gen[9][9]; // scratch board for generator
// Bitmasks: bit i set means digit i+1 is used
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

int popcount9(int x){
    int n = 0;
    while(x){ n++; x &= x - 1; }
    return n;
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

// Fill board with a random complete solution
int gen_fill(int pos){
    if(pos == 81) return 1;
    int r = pos / 9, c = pos % 9;
    if(gen[r][c]) return gen_fill(pos + 1);
    int cands = gen_candidates(r, c);
    // Shuffle candidates
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
            int n = popcount9(cands);
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
        if(gen_count != 1){
            puzzle[r][c] = saved; // put it back
        } else {
            removed++;
        }
    }
    unsigned int t2 = SDL_GetTicks();
    SDL_Log("generated puzzle: fill %ums, holes %ums, total %ums, %d removed",
        t1 - t0, t2 - t1, t2 - t0, removed);
    SDL_memcpy(board, puzzle, sizeof puzzle);
}

void reset_board(void){
    SDL_memcpy(board, puzzle, sizeof puzzle);
}

void load_puzzle(int idx){
    if(idx < NPUZZLES){
        const char* p = puzzles[idx];
        for(int i = 0; i < 81; i++){
            int v = (p[i] >= '1' && p[i] <= '9') ? p[i] - '0' : 0;
            puzzle[i / 9][i % 9] = v;
        }
    } else {
        generate_puzzle();
    }
    reset_board();
}

void cheat(void){
    if(puzzle[sel_r][sel_c]) return; // already a given
    SDL_memcpy(gen, board, sizeof gen);
    gen_init_masks();
    if(gen_solve(0))
        board[sel_r][sel_c] = gen[sel_r][sel_c];
}

// Button hit test: returns 0=reset, 1=solve, 2=next, -1=miss
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

_Bool running = 1;
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
                reset_board();
                break;
            case SDLK_c:
                cheat();
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

SDL_DestroyRenderer(ren);
SDL_DestroyWindow(win);
SDL_Quit();
