//
// Copyright © 2024, David Priver <david@davidpriver.com>
//
#ifndef DRT_LL_C
#define DRT_LL_C

#include "drt.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct DrtColor DrtColor;
struct DrtColor {
    union {
        struct {
            unsigned char r, g, b;
            unsigned char is_set;
        };
        unsigned bits;
    };
};
_Static_assert(sizeof(DrtColor) == sizeof(unsigned), "");

typedef struct DrtState DrtState;
struct DrtState {
    unsigned style;
    DrtColor color;
    DrtColor bg_color;
    unsigned char bg_alpha; // 255 = opaque, 0 = transparent
    struct {
        int x, y, w, h;
    } scissor;
};

typedef struct DrtCell DrtCell;
struct DrtCell {
    DrtColor color;
    DrtColor bg_color;
    unsigned char style:5;
    unsigned char rend_width:3;
    char txt[7];
};

struct Drt {
    DrtState state_stack[100];
    size_t state_cursor;
    int term_w, term_h;
    struct {
        int x, y, w, h;
    } draw_area;
    int x, y;
    DrtCell* _Nullable cells[2];
    int alloc_w, alloc_h;
    Allocator allocator;
    _Bool active_cells;
    _Bool dirty;
    _Bool force_paint;
    _Bool cursor_visible;
    char* _Nullable buff;
    size_t buff_cap;
    size_t buff_cursor;
    int cur_x, cur_y;
};

static inline
DrtCell* _Nullable
drt_current_cell(Drt* drt){
    if(!drt->cells[drt->active_cells]) return NULL;
    return &drt->cells[drt->active_cells][drt->x+drt->y*drt->alloc_w];
}
static inline
DrtCell* _Nullable
drt_old_cell(Drt* drt){
    if(!drt->cells[!drt->active_cells]) return NULL;
    return &drt->cells[!drt->active_cells][drt->x+drt->y*drt->alloc_w];
}

static inline
DrtState*
drt_current_state(Drt* drt){
    return &drt->state_stack[drt->state_cursor];
}

static inline
void
drt_sprintf(Drt* drt, const char* fmt, ...){
    if(!drt->buff) return;
    va_list va;
    va_start(va, fmt);
    char* buff = drt->buff + drt->buff_cursor;
    size_t remainder = drt->buff_cap - drt->buff_cursor;
    size_t n = vsnprintf(buff, remainder, fmt, va);
    if(n > remainder)
        n = remainder - 1;
    drt->buff_cursor += n;
    va_end(va);
}

static inline
void
drt_flush(Drt* drt){
    drt_sprintf(drt, "\x1b[%d;%dH", drt->cur_y+drt->draw_area.y+1, drt->cur_x+drt->draw_area.x+1);
    if(0){
        static FILE* fp;
        if(!fp) fp = fopen("drtlog.txt", "a");
        fprintf(fp, "\nflush\n-----\n\n");
        for(size_t i = 0; i < drt->buff_cursor; i++){
            char c = drt->buff[i];
            if((unsigned)(unsigned char)c < 0x20)
                fprintf(fp, "\\x%x", c);
            else
                fputc(c, fp);
        }
        fflush(fp);
    }
    fwrite(drt->buff, drt->buff_cursor, 1, stdout);
    fflush(stdout);
    drt->buff_cursor = 0;
    drt->force_paint = 0;
}

DRT_API
void
drt_init(Drt* drt){
    drt_sprintf(drt, "\x1b[?1049h");
}

DRT_API
int
drt_init_alloc(Drt* drt, Allocator a, int w, int h){
    memset(drt, 0, sizeof *drt);
    drt->state_stack[0].bg_alpha = 255;
    drt->allocator = a;
    drt->alloc_w = w;
    drt->alloc_h = h;
    size_t cell_count = (size_t)w * h;
    size_t cell_size = cell_count * sizeof(DrtCell);
    drt->cells[0] = Allocator_zalloc(a, cell_size);
    if(!drt->cells[0]) return -1;
    drt->cells[1] = Allocator_zalloc(a, cell_size);
    if(!drt->cells[1]){
        Allocator_free(a, drt->cells[0], cell_size);
        drt->cells[0] = NULL;
        return -1;
    }
    // Output buffer: ~32 bytes per cell for escape codes
    drt->buff_cap = 32 * cell_count;
    drt->buff = Allocator_alloc(a, drt->buff_cap);
    if(!drt->buff){
        Allocator_free(a, drt->cells[0], cell_size);
        Allocator_free(a, drt->cells[1], cell_size);
        drt->cells[0] = NULL;
        drt->cells[1] = NULL;
        return -1;
    }
    drt->term_w = w;
    drt->term_h = h;
    drt_init(drt);
    return 0;
}

DRT_API
void
drt_cleanup(Drt* drt){
    size_t cell_count = (size_t)drt->alloc_w * drt->alloc_h;
    size_t cell_size = cell_count * sizeof(DrtCell);
    if(drt->cells[0]) Allocator_free(drt->allocator, drt->cells[0], cell_size);
    if(drt->cells[1]) Allocator_free(drt->allocator, drt->cells[1], cell_size);
    if(drt->buff) Allocator_free(drt->allocator, drt->buff, drt->buff_cap);
    drt->cells[0] = NULL;
    drt->cells[1] = NULL;
    drt->buff = NULL;
}

DRT_API
void
drt_end(Drt* drt){
    drt_sprintf(drt, "\x1b[?25h");
    drt_sprintf(drt, "\x1b[?1049l");
    drt_sprintf(drt, "\n");
    drt_flush(drt);
}

typedef struct DrtPaint DrtPaint;
struct DrtPaint {
    DrtState state;
    int x, y;
};

static inline
void
drt_paint_update(Drt* drt, DrtPaint* p, int x, int y, DrtCell* new){
    if(x != p->x || y != p->y){
        // Goto coord
        int term_x = drt->draw_area.x + x+1;
        int term_y = drt->draw_area.y + y+1;
        drt_sprintf(drt, "\x1b[%d;%dH", term_y, term_x);
    }
    _Bool started = 0;
    if(p->state.style != new->style){
        // set style
        drt_sprintf(drt, "\x1b[0;");
        started = 1;
        p->state.color = (DrtColor){0};
        p->state.bg_color = (DrtColor){0};
        if(new->style & DRT_STYLE_BOLD){
            drt_sprintf(drt, "1;");
        }
        if(new->style & DRT_STYLE_ITALIC){
            drt_sprintf(drt, "3;");
        }
        if(new->style & DRT_STYLE_UNDERLINE){
            drt_sprintf(drt, "4;");
        }
        if(new->style & DRT_STYLE_STRIKETHROUGH){
            drt_sprintf(drt, "9;");
        }
    }
    if(p->state.color.bits != new->color.bits){
        if(!started){
            started = 1;
            drt_sprintf(drt, "\x1b[");
        }
        // set foreground color
        if(!new->color.is_set){
            drt_sprintf(drt, "39;");
        }
        else {
            drt_sprintf(drt, "38;2;%d;%d;%d;", new->color.r, new->color.g, new->color.b);
        }
    }
    if(p->state.bg_color.bits != new->bg_color.bits){
        if(!started){
            started = 1;
            drt_sprintf(drt, "\x1b[");
        }
        // set background color
        if(!new->bg_color.is_set){
            drt_sprintf(drt, "49;");
        }
        else {
            drt_sprintf(drt, "48;2;%d;%d;%d;", new->bg_color.r, new->bg_color.g, new->bg_color.b);
        }
    }
    if(started){
        drt->buff[drt->buff_cursor-1] = 'm';
    }
    // write char
    if((unsigned)(unsigned char)new->txt[0] <= 0x20u)
        drt_sprintf(drt, " ");
    else if((unsigned)(unsigned char)new->txt[0] == 0x7fu)
        drt_sprintf(drt, " ");
    else
        drt_sprintf(drt, "%s", new->txt);
    p->x = x+(new->rend_width?new->rend_width:1);
    p->y = y;
    p->state.style = new->style;
    p->state.color = new->color;
    p->state.bg_color = new->bg_color;
}

DRT_API
int
drt_paint(Drt* drt){
    if(!drt->cells[0] || !drt->cells[1]) return -1;
    if(!drt->dirty && !drt->force_paint) return 0;
    drt_sprintf(drt, "\x1b[?25l");
    drt_sprintf(drt, "\x1b[?2026h");
    if(drt->force_paint){
        drt_sprintf(drt, "\x1b[%d;%dH\x1b[0J", drt->draw_area.y+1, drt->draw_area.x+1);
    }
    DrtPaint paint = {.x = -1, .y=-1};
    for(int y = 0; y < drt->draw_area.h; y++)
        for(int x = 0; x < drt->draw_area.w; x++){
            DrtCell* old = &drt->cells[!drt->active_cells][x+y*drt->alloc_w];
            DrtCell* new = &drt->cells[drt->active_cells][x+y*drt->alloc_w];
            if(!old->txt[0]) old->txt[0] = ' ';
            if(!new->txt[0]) new->txt[0] = ' ';
            if(!drt->force_paint){
                if(memcmp(old, new, sizeof *new) == 0) continue;
            }
            drt_paint_update(drt, &paint, x, y, new);
            *old = *new;
            if(new->rend_width > 1)
                x += new->rend_width-1;
        }
    if(drt->cursor_visible){
        drt_sprintf(drt, "\x1b[?25h");
    }
    drt_sprintf(drt, "\x1b[0m");
    drt_sprintf(drt, "\x1b[?2026l");
    drt_flush(drt);
    drt->active_cells = !drt->active_cells;
    drt->dirty = 0;
    return 0;
}

DRT_API
void
drt_clear_screen(Drt* drt){
    if(!drt->cells[drt->active_cells]) return;
    size_t cell_count = (size_t)drt->alloc_w * drt->alloc_h;
    memset(drt->cells[drt->active_cells], 0, cell_count * sizeof(DrtCell));
}

DRT_API
void
drt_invalidate(Drt* drt){
    drt->force_paint = 1;
}

DRT_API
void
drt_move(Drt* drt, int x, int y){
    if(x > -1){
        if(x >= drt->draw_area.w)
            x = drt->draw_area.w-1;
        drt->x = x;
    }
    if(y > -1){
        if(y >= drt->draw_area.h)
            y = drt->draw_area.h-1;
        drt->y = y;
    }
}

DRT_API
void
drt_cursor(Drt* drt, int* x, int* y){
    *x = drt->x;
    *y = drt->y;
}

DRT_API
void
drt_update_drawable_area(Drt* drt, int x, int y, int w, int h){
    if(w < 0) w = 0;
    if(h < 0) h = 0;
    if(x+w>drt->term_w) w = drt->term_w - x;
    if(y+h>drt->term_h) h = drt->term_h - y;
    if(drt->draw_area.x == x && drt->draw_area.y == y && drt->draw_area.w == w && drt->draw_area.h == h)
        return;
    drt->force_paint = 1;
    drt->draw_area.x = x;
    drt->draw_area.y = y;
    drt->draw_area.w = w;
    drt->draw_area.h = h;
    if(drt->x >= w) drt->x = w?w-1:0;
    if(drt->y >= h) drt->y = h?h-1:0;

}

DRT_API
void
drt_update_terminal_size(Drt* drt, int w, int h){
    if(drt->term_w == w && drt->term_h == h)
        return;
    // Reallocate if needed
    if(w > drt->alloc_w || h > drt->alloc_h){
        int new_w = w > drt->alloc_w ? w : drt->alloc_w;
        int new_h = h > drt->alloc_h ? h : drt->alloc_h;
        size_t old_count = (size_t)drt->alloc_w * drt->alloc_h;
        size_t old_size = old_count * sizeof(DrtCell);
        size_t new_count = (size_t)new_w * new_h;
        size_t new_size = new_count * sizeof(DrtCell);
        DrtCell* new0 = Allocator_zalloc(drt->allocator, new_size);
        DrtCell* new1 = Allocator_zalloc(drt->allocator, new_size);
        if(new0 && new1){
            if(drt->cells[0]) Allocator_free(drt->allocator, drt->cells[0], old_size);
            if(drt->cells[1]) Allocator_free(drt->allocator, drt->cells[1], old_size);
            drt->cells[0] = new0;
            drt->cells[1] = new1;
            drt->alloc_w = new_w;
            drt->alloc_h = new_h;
            // Resize output buffer too
            size_t new_buff_cap = 32 * new_count;
            char* new_buff = Allocator_alloc(drt->allocator, new_buff_cap);
            if(new_buff){
                if(drt->buff) Allocator_free(drt->allocator, drt->buff, drt->buff_cap);
                drt->buff = new_buff;
                drt->buff_cap = new_buff_cap;
            }
        }
        else {
            if(new0) Allocator_free(drt->allocator, new0, new_size);
            if(new1) Allocator_free(drt->allocator, new1, new_size);
        }
    }
    drt->force_paint = 1;
    drt->term_w = w;
    drt->term_h = h;
    if(drt->draw_area.x + drt->draw_area.w > w)
        drt_update_drawable_area(drt, drt->draw_area.x, drt->draw_area.y, w - drt->draw_area.x, h);
    if(drt->draw_area.y + drt->draw_area.h > h)
        drt_update_drawable_area(drt, drt->draw_area.x, drt->draw_area.y, drt->draw_area.w, h - drt->draw_area.y);
}

DRT_API
void
drt_push_state(Drt* drt){
    if(drt->state_cursor +1 >= sizeof drt->state_stack / sizeof drt->state_stack[0]){
        // __builtin_debugtrap();
        return;
    }
    drt->state_stack[drt->state_cursor+1] = drt->state_stack[drt->state_cursor];
    drt->state_cursor++;
}

DRT_API
void
drt_pop_state(Drt* drt){
    if(drt->state_cursor) --drt->state_cursor;
    // else __builtin_debugtrap();
}

DRT_API
void
drt_pop_all_states(Drt* drt){
    drt->state_cursor = 0;
    *drt_current_state(drt) = (DrtState){.bg_alpha=255};
}

DRT_API
void
drt_clear_state(Drt* drt){
    *drt_current_state(drt) = (DrtState){.bg_alpha=255};
}


DRT_API
void
drt_scissor(Drt* drt, int x, int y, int w, int h){
    DrtState* s = drt_current_state(drt);
    s->scissor.x = x;
    s->scissor.y = y;
    s->scissor.w = w;
    s->scissor.h = h;
}

DRT_API
void
drt_set_style(Drt* drt, unsigned style){
    // if(drt->state_cursor == 0) __builtin_debugtrap();
    drt_current_state(drt)->style = style & DRT_STYLE_ALL;
}

DRT_API
void
drt_clear_color(Drt* drt){
    drt_current_state(drt)->color = (DrtColor){0};
}

DRT_API
void
drt_set_24bit_color(Drt* drt, unsigned char r, unsigned char g, unsigned char b){
    drt_current_state(drt)->color = (DrtColor){.r=r, .g=g, .b=b, .is_set=1};
}

DRT_API
void
drt_bg_clear_color(Drt* drt){
    drt_current_state(drt)->bg_color = (DrtColor){0};
}

DRT_API
void
drt_bg_set_24bit_color(Drt* drt, unsigned char r, unsigned char g, unsigned char b){
    DrtState* s = drt_current_state(drt);
    s->bg_color = (DrtColor){.r=r, .g=g, .b=b, .is_set=1};
    s->bg_alpha = 255;
}

DRT_API
void
drt_bg_set_color_rgba(Drt* drt, unsigned char r, unsigned char g, unsigned char b, unsigned char a){
    DrtState* s = drt_current_state(drt);
    s->bg_color = (DrtColor){.r=r, .g=g, .b=b, .is_set=1};
    s->bg_alpha = a;
}

static inline
DrtColor
drt_blend_bg(DrtColor new_bg, unsigned char alpha, DrtColor old_bg){
    if(alpha == 255) return new_bg;
    if(alpha == 0 || !new_bg.is_set) return old_bg;
    if(!old_bg.is_set) return new_bg;
    // Blend: result = new * alpha + old * (255 - alpha)
    unsigned a = alpha;
    unsigned inv_a = 255 - alpha;
    unsigned char r = (unsigned char)((new_bg.r * a + old_bg.r * inv_a) / 255);
    unsigned char g = (unsigned char)((new_bg.g * a + old_bg.g * inv_a) / 255);
    unsigned char b = (unsigned char)((new_bg.b * a + old_bg.b * inv_a) / 255);
    return (DrtColor){.r=r, .g=g, .b=b, .is_set=1};
}

static inline
_Bool
drt_scissor_test(DrtState* state, int x, int y){
    if(state->scissor.w <= 0 || state->scissor.h <= 0) return 1; // No scissor
    if(x < state->scissor.x) return 0;
    if(y < state->scissor.y) return 0;
    if(x >= state->scissor.x + state->scissor.w) return 0;
    if(y >= state->scissor.y + state->scissor.h) return 0;
    return 1;
}

DRT_API
void
drt_setc(Drt* drt, char c){
    DrtState* state = drt_current_state(drt);
    if(!drt_scissor_test(state, drt->x, drt->y)) return;
    DrtCell* cell = drt_current_cell(drt);
    DrtCell* old = drt_old_cell(drt);
    if(!cell) return;
    memset(cell->txt, 0, sizeof cell->txt);
    cell->txt[0] = c;
    cell->color = state->color;
    cell->bg_color = drt_blend_bg(state->bg_color, state->bg_alpha, cell->bg_color);
    cell->style = (unsigned char)state->style;
    cell->rend_width = 1;
    if(old && memcmp(cell, old, sizeof *cell) != 0)
        drt->dirty = 1;
}

DRT_API
void
drt_setc_at(Drt* drt, int x, int y, char c){
    if(x >= drt->draw_area.w) return;
    if(y >= drt->draw_area.h) return;
    if(!drt->cells[drt->active_cells]) return;
    DrtState* state = drt_current_state(drt);
    if(!drt_scissor_test(state, x, y)) return;
    DrtCell* cell = &drt->cells[drt->active_cells][x+y*drt->alloc_w];
    DrtCell* old = &drt->cells[!drt->active_cells][x+y*drt->alloc_w];
    memset(cell->txt, 0, sizeof cell->txt);
    cell->txt[0] = c;
    cell->color = state->color;
    cell->bg_color = drt_blend_bg(state->bg_color, state->bg_alpha, cell->bg_color);
    cell->style = (unsigned char)state->style;
    cell->rend_width = 1;
    if(memcmp(cell, old, sizeof *cell) != 0)
        drt->dirty = 1;
}

DRT_API
void
drt_putc(Drt* drt, char c){
    drt_setc(drt, c);
    drt_move(drt, drt->x+1, -1);
}

DRT_API
void
drt_setc_mb(Drt* drt, const char* c, size_t length, size_t rendwidth){
    DrtState* state = drt_current_state(drt);
    if(!drt_scissor_test(state, drt->x, drt->y)) return;
    DrtCell* cell = drt_current_cell(drt);
    DrtCell* old = drt_old_cell(drt);
    if(!cell) return;
    if(length > 7) return;
    memset(cell->txt, 0, sizeof cell->txt);
    memcpy(cell->txt, c, length);
    cell->color = state->color;
    cell->bg_color = drt_blend_bg(state->bg_color, state->bg_alpha, cell->bg_color);
    cell->style = (unsigned char)state->style;
    cell->rend_width = (unsigned char)rendwidth;
    if(old && memcmp(cell, old, sizeof *cell) != 0)
        drt->dirty = 1;
}
DRT_API
void
drt_putc_mb(Drt* drt, const char* c, size_t length, size_t rend_width){
    drt_setc_mb(drt, c, length, rend_width);
    drt_move(drt, drt->x+(int)rend_width, -1);
}

DRT_API
void
drt_puts(Drt* drt, const char* txt, size_t length){
    drt_puts_utf8(drt, txt, length);
}

DRT_API
void
drt_puts_utf8(Drt* drt, const char* str, size_t length){
    const unsigned char* s = (const unsigned char*)str;
    const unsigned char* end = s + length;
    while(s != end){
        if(*s < 128){
            // ASCII character
            drt_putc(drt, (char)*s);
            s++;
        }
        else {
            // Multi-byte UTF-8 sequence
            size_t len = 0;
            if((*s & 0xe0) == 0xc0) len = 2;
            else if((*s & 0xf0) == 0xe0) len = 3;
            else if((*s & 0xf8) == 0xf0) len = 4;
            else {
                s++; // Invalid, skip
                continue;
            }

            // Use drt_putc_mb with render width of 1 for most chars
            drt_putc_mb(drt, (const char*)s, len, 1);
            s += len;
        }
    }
}

DRT_API
void
drt_set_cursor_visible(Drt* drt, _Bool show){
    drt->cursor_visible = show;
}

DRT_API
void
drt_move_cursor(Drt* drt, int x, int y){
    drt->cur_x = x;
    drt->cur_y = y;
}

DRT_API
void
drt_printf(Drt* drt, const char* fmt, ...){
    char buff[1024];
    va_list va;
    va_start(va, fmt);
    int n = vsnprintf(buff, sizeof buff, fmt, va);
    va_end(va);
    drt_puts_utf8(drt, buff, n < (int)sizeof buff? n : -1+(int)sizeof buff);
}
DRT_API
void
drt_clear_to_end_of_row(Drt*drt){
    int w = drt->draw_area.w - drt->x;
    if(!w) return;
    if(!drt->cells[drt->active_cells]) return;
    memset(&drt->cells[drt->active_cells][drt->x+drt->y*drt->alloc_w], 0, w * sizeof(DrtCell));
    drt->dirty = 1;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
