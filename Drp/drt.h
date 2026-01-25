//
// Copyright © 2024, David Priver <david@davidpriver.com>
//
#ifndef DRT_LL_H
#define DRT_LL_H

#include <stddef.h>
#include "Allocators/allocator.h"

#ifndef DRT_API
#define DRT_API static
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct Drt Drt;

DRT_API
int
drt_paint(Drt* drt);

DRT_API
void
drt_init(Drt* drt);

DRT_API
int
drt_init_alloc(Drt* drt, Allocator a, int w, int h);

DRT_API
void
drt_cleanup(Drt* drt);

DRT_API
void
drt_invalidate(Drt* drt);

DRT_API
void
drt_move(Drt* drt, int x, int y);

DRT_API
void
drt_cursor(Drt* drt, int* x, int* y);

DRT_API
void
drt_update_drawable_area(Drt* drt, int x, int y, int w, int h);

DRT_API
void
drt_update_terminal_size(Drt* drt, int w, int h);

DRT_API
void
drt_push_state(Drt* drt);

DRT_API
void
drt_pop_state(Drt* drt);

DRT_API
void
drt_pop_all_states(Drt* drt);

DRT_API
void
drt_clear_state(Drt* drt);

DRT_API
void
drt_scissor(Drt* drt, int x, int y, int w, int h);

enum DrtStyle {
    DRT_STYLE_NONE = 0x0,
    DRT_STYLE_BOLD = 0x1,
    DRT_STYLE_ITALIC = 0x2,
    DRT_STYLE_UNDERLINE = 0x4,
    DRT_STYLE_STRIKETHROUGH = 0x8,
    DRT_STYLE_ALL = DRT_STYLE_BOLD|DRT_STYLE_ITALIC|DRT_STYLE_UNDERLINE|DRT_STYLE_STRIKETHROUGH,
};

DRT_API
void
drt_set_style(Drt* drt, unsigned style);

DRT_API
void
drt_clear_color(Drt* drt);

DRT_API
void
drt_set_24bit_color(Drt* drt, unsigned char r, unsigned char g, unsigned char b);

DRT_API
void
drt_bg_clear_color(Drt* drt);

DRT_API
void
drt_bg_set_24bit_color(Drt* drt, unsigned char r, unsigned char g, unsigned char b);

DRT_API
void
drt_bg_set_color_rgba(Drt* drt, unsigned char r, unsigned char g, unsigned char b, unsigned char a);


DRT_API
void
drt_setc(Drt* drt, char c);

DRT_API
void
drt_setc_at(Drt* drt, int x, int y, char c);

DRT_API
void
drt_putc(Drt* drt, char c);

DRT_API
void
drt_putc_mb(Drt* drt, const char* c, size_t length, size_t rendwith);

DRT_API
void
drt_puts(Drt* drt, const char* txt, size_t length);

DRT_API
void
drt_puts_utf8(Drt* drt, const char* str, size_t length);

DRT_API
void
drt_set_cursor_visible(Drt* drt, _Bool show);

DRT_API
void
drt_clear_screen(Drt* drt);

DRT_API
void
drt_move_cursor(Drt* drt, int x, int y);

DRT_API
void
#ifdef __GNUC__
__attribute__((format(printf,2, 3)))
#endif
drt_printf(Drt* drt, const char* fmt, ...);

DRT_API
void
drt_clear_to_end_of_row(Drt*drt);

// DRT_API
// void
// drt_setc_mb(Drt* drt, const char* txt, size_t length, size_t render_length);




#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
