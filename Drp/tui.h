#ifndef DRP_TUI_H
#define DRP_TUI_H
#include <stdint.h>
#include <stddef.h>
#include "windowsheader.h"
#include "posixheader.h"
#include "typed_enum.h"
#include "stringview.h"
#include "MStringBuilder.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum TuiEventKind TYPED_ENUM(uint32_t){
    TUI_EOF,
    TUI_NO_INPUT,
    TUI_NAV,
    TUI_RESIZE,
    TUI_MOUSE,
    TUI_SCROLL,
    TUI_KEY,
    TUI_CONTROL,
};
TYPEDEF_ENUM(TuiEventKind, uint32_t);

enum TuiNavKind TYPED_ENUM(uint32_t){
    TUI_NAV_UP,
    TUI_NAV_DOWN,
    TUI_NAV_LEFT,
    TUI_NAV_RIGHT,
    TUI_NAV_HOME,
    TUI_NAV_END,
    TUI_NAV_PAGE_UP,
    TUI_NAV_PAGE_DOWN,
};
TYPEDEF_ENUM(TuiNavKind, uint32_t);
enum TuiMouseKind TYPED_ENUM(uint32_t){
    TUI_MOUSE_DOWN,
    TUI_MOUSE_UP,
    TUI_MOUSE_DRAG,
    TUI_MOUSE_MOVE,
};
TYPEDEF_ENUM(TuiMouseKind, uint32_t);
enum TuiControlKind TYPED_ENUM(uint32_t){
    TUI_CONTROL_A,
    TUI_CONTROL_B,
    TUI_CONTROL_C,
    TUI_CONTROL_D,
    TUI_CONTROL_E,
    TUI_CONTROL_F,
    TUI_CONTROL_G,
    TUI_CONTROL_H,
    TUI_CONTROL_I,
    TUI_CONTROL_TAB = TUI_CONTROL_I,
    TUI_CONTROL_J,
    TUI_CONTROL_K,
    TUI_CONTROL_L,
    TUI_CONTROL_M,
    TUI_CONTROL_ENTER = TUI_CONTROL_M,
    TUI_CONTROL_N,
    TUI_CONTROL_O,
    TUI_CONTROL_P,
    TUI_CONTROL_Q,
    TUI_CONTROL_R,
    TUI_CONTROL_S,
    TUI_CONTROL_T,
    TUI_CONTROL_U,
    TUI_CONTROL_V,
    TUI_CONTROL_W,
    TUI_CONTROL_X,
    TUI_CONTROL_Y,
    TUI_CONTROL_Z,
    TUI_CONTROL_ESC,
    TUI_CONTROL_BACKSPACE,
    TUI_CONTROL_DELETE,
    TUI_CONTROL_F1,
    TUI_CONTROL_F2,
    TUI_CONTROL_F3,
    TUI_CONTROL_F4,
    TUI_CONTROL_F5,
    TUI_CONTROL_F6,
    TUI_CONTROL_F7,
    TUI_CONTROL_F8,
    TUI_CONTROL_F9,
    TUI_CONTROL_F10,
    TUI_CONTROL_F11,
    TUI_CONTROL_F12,
    TUI_CONTROL_F13,
    TUI_CONTROL_F14,
    TUI_CONTROL_F15,
    TUI_CONTROL_F16,
    TUI_CONTROL_F17,
    TUI_CONTROL_F18,
    TUI_CONTROL_F19,
    TUI_CONTROL_F20,
    TUI_CONTROL_INSERT,
};
TYPEDEF_ENUM(TuiControlKind, uint32_t);
typedef struct TuiEvent TuiEvent;
struct TuiEvent {
    union {
        struct {
            TuiEventKind kind: 8;
            uint32_t shift:    1,
                     ctrl:     1,
                     alt:      1,
                     _pad:     21;
        };
        struct {
            TuiEventKind kind: 8;
            uint32_t shift:    1,
                     ctrl:     1,
                     alt:      1,
                     _pad:     13;
            TuiNavKind nav:    8;
        } nav;
        struct {
            TuiEventKind kind: 8;
            uint32_t _pad:     24;
            uint32_t w, h; // in cells
        } resize;
        struct {
            TuiEventKind kind:  8;
            uint32_t shift:     1,
                     ctrl:      1,
                     alt:       1,
                     which:     2, // 0 = left, 1 = middle, 2 = right, 3 = none
                     _pad:      11;
            TuiMouseKind mouse: 8;
            union {
                struct {uint32_t cx, cy;}; // in cells, 0-based
                struct {uint32_t x, y;} c;
            };
        } mouse;
        struct {
            TuiEventKind kind: 8;
            uint32_t shift:    1,
                     ctrl:     1,
                     alt:      1,
                     _pad:     21;
            int32_t dx, dy; // scroll steps; positive = right/down
            union {
                struct {uint32_t cx, cy;}; // mouse position in cells, 0-based
                struct {uint32_t x, y;} c;
            };
        } scroll;
        struct {
            TuiEventKind kind: 8;
            uint32_t shift:    1,
                     ctrl:     1,
                     alt:      1,
                     keypad:   1,
                     _pad:     20;
            uint32_t key; // unicode code point
        } key;
        struct {
            TuiEventKind kind: 8;
            uint32_t shift:    1,
                     ctrl:     1,
                     alt:      1,
                     _pad:     21;
            TuiControlKind key;
        } control;
    };
};

typedef struct TuiColor TuiColor;
struct TuiColor {
    union {
        struct {
            uint8_t r, g, b,
                    is_set: 1, // if not set, this is reset
                    is_8bit: 1, // use 8 bit terminal color instead of rgb
                    is_none: 1, // leave current color unchanged
                    _padding: 5; // free real estate
        } rgb;
        struct {
            uint8_t color_index,
                    _pad1, _pad2,
                    is_set: 1, // if not set, this is reset
                    is_8bit: 1, // use 8 bit terminal color instead of rgb
                    is_none: 1, // leave current color unchanged
                    _padding: 5; // free real estate
        } _8bit;
        struct {
            uint8_t _1, _2, _3,
                    is_set: 1, // if not set, this is reset
                    is_8bit: 1, // use 8 bit terminal color instead of rgb
                    is_none: 1, // leave current color unchanged
                    _padding: 5; // free real estate
        };
        uint32_t bits;
    };
};


typedef struct TuiStyle TuiStyle;
struct TuiStyle {
    union {
        uint8_t bits;
        struct {
            uint8_t bold:           1,
                    italic:         1,
                    underline:      1,
                    strike_through: 1,
                    _padding: 4; // free real estate
        };
    };
};
typedef struct TuiPaintState TuiPaintState;
struct TuiPaintState {
    TuiColor color;
    TuiColor bg_color;
    TuiStyle style;
    uint8_t bg_alpha; // 255 = opaque, 0 = transparent
    _Bool scissor_enabled;
    uint8_t _pad; // free real estate
    struct {
        int x, y;
        uint32_t w, h;
    } scissor;
};

typedef struct TuiCell TuiCell;
struct TuiCell {
    TuiColor color;
    TuiColor bg_color;
    uint8_t style:        4,
            rend_width:   3,
            continuation: 1;
    char txt[7];
};

typedef struct TuiCanvas TuiCanvas;
struct TuiCanvas {
    TuiPaintState state_stack[100];
    size_t state_cursor;
    uint32_t w, h; // size of the terminal
    int x, y; // current draw point
    TuiCell* _Nullable cells[2];
    uint32_t alloc_w, alloc_h; // size of the allocated cells
    _Bool active_cells;
    _Bool dirty;
    _Bool force_paint;
    _Bool color_256; // use the 16..255 colors instead of 24bit color
    _Bool cursor_visible;
    _Bool cursor_dirty;
    MStringBuilder buff;
    // where to draw the cursor
    int cursor_x,
        cursor_y;
};

typedef struct Tui Tui;
struct Tui {
    _Bool enabled;
    #ifdef _WIN32
    HANDLE hstdin, hstdout, hstderr;
    DWORD input_mode, output_mode;
    KEY_EVENT_RECORD repeat;
    WORD repeats, high_surrogate;
    uint32_t repeat_codepoint;
    int repeat_is_character;
    MOUSE_EVENT_RECORD pending_mouse;
    int mouse_pending;
    int32_t wheel_x, wheel_y;
    #else
    int hstdin, hstdout, hstderr;
    struct termios raw;
    struct termios orig;
    unsigned char bytes[256];
    uint32_t count;
    int eof, discard_sequence, mouse_reporting;
    int event_fd;
    _Bool events_active;
    #ifdef __linux__
    _Bool winch_was_blocked;
    #endif
    #endif
    uint32_t columns, rows, mouse_buttons;
    TuiCanvas canvas;
};

typedef struct TuiOptions TuiOptions;
struct TuiOptions {
    // handles for input/output/stderr
    // If all 3 are zero, then the standard handles are retrieved.
    // On Linux, other threads must block `SIGWINCH` while the TUI owns it.
    OsFileHandle input,
                 output,
                 error;
};


// Start-up etc.
// =============
static int tui_open(Tui*, TuiOptions*);
// tui_open()
// ----------
// Starts the alternate screen and puts the terminal in the proper mode.
//
// On POSIX, owns `SIGWINCH` notifications until paused or closed.
//
// Returns:
// --------
// 0 or an error.
//

static void tui_close(Tui*);
// tui_close()
// -----------
// Shuts down the TUI, stops the alternate screen and puts the terminal
// back into its original mode.
//

static int tui_pause(Tui*);
// tui_pause()
// -----------
// Stops the alternate screen and restores the terminal back into
// its orignal mode.
// This is for implementing ctrl-z or when you want to run a subprocess
// that wants to use the terminal.
//
// Call `tui_resume()` afterwards.
//
// Returns:
// --------
// 0 or an IO error.
//

static int tui_resume(Tui*);
// tui_resume()
// ------------
// Restores the alternate screen and puts the terminal back into raw mode or
// whatever.
//
// Call after `tui_pause()`
//
// Returns:
// --------
// 0 or an IO error.
//

static void tui_invalidate(Tui*);
// tui_invalidate()
// ----------------
// Call this if the terminal has been changed in a way that this
// library doesn't detect. The next paint won't assume any cells
// are already in the right state.
//
// Input polling checks the terminal size and invalidates the canvas on resize.
//

// Input API
// =========
static int tui_poll_event(Tui*, TuiEvent*);
// tui_poll_event()
// ----------------
// Fetches the next input event, without blocking.
//
// Returns:
// --------
// 0 on success or non-zero on error.
// Lack of input is success, but the TuiEvent will be `TUI_NO_INPUT`.
//

static int tui_wait_event(Tui*, TuiEvent*);
// tui_wait_event()
// ----------------
// Fetches the next input event, blocking.
//
// This can output `TUI_NO_INPUT` due to things like signals.
//
// Returns:
// --------
// 0 on success or non-zero on error.
//

static int tui_wait_event_timeout(Tui*, TuiEvent*, int timeout_ms);
// tui_wait_event()
// ----------------
// Fetches the next input event, waiting for up to `timeout_ms`.
// Expiration returns success with TUI_NO_INPUT.
//

// Canvas API
// ==========
static void tui_move(Tui*, int x, int y);
// tui_move()
// ----------
// Set the drawing position. This does not move the terminal cursor.
//

static void tui_cursor(Tui*, int* x, int* y);
// tui_cursor()
// ------------
// Read the drawing position.
//

static void tui_move_cursor(Tui*, int x, int y);
// tui_move_cursor()
// -----------------
// Set the terminal cursor position and mark it for the next paint.
//

static void tui_set_style(Tui*, TuiStyle);
// tui_set_style()
// ---------------
// Replace the current drawing style.
//

static void tui_clear_scissor(Tui*);
// tui_clear_scissor()
// -------------------
// Disable clipping to the current scissor rectangle.
//

static void tui_clear_screen(Tui*);
// tui_clear_screen()
// ------------------
// Clear the canvas, respecting the current scissor rectangle.
//

static void tui_set_256_color(Tui*, _Bool);
// tui_set_256_color()
// -------------------
// Select indexed-color output for RGB colors and invalidate the screen.
//

static void tui_pop_all_states(Tui*);
// tui_pop_all_states()
// ---------------------
// Return to the bottom of the state stack and reset it to defaults.
//

static int tui_paint(Tui*);
// tui_paint()
// -----------
// Call when you're done drawing. Emits the needed draw commands to synchronize
// the state of the terminal with what you've drawn.
//
// Returns:
// --------
// 0 on success or non-zero on IO error.
//

static void tui_push_state(Tui*);
// tui_push_state()
// ----------------
// Pushes the current drawing state onto a stack.
// This let's you save the current state than restore
// it with `tui_pop_state()`.
//

static void tui_pop_state(Tui*);
// tui_pop_state()
// ---------------
// Pops the drawing stack, setting the current drawing state
// to whatever is on top.
//
// Use after `tui_push_state()`
//

static void tui_clear_state(Tui*);
// tui_clear_state()
// -----------------
// Sets the current drawing state to the default.
// Does not touch the drawing stack.
//

static void tui_scissor(Tui*, int x, int y, uint32_t w, uint32_t h);
// tui_scissor()
// -------------
// Sets the current scissor area.
// Drawing commands that go outside the scissor area are ignored.
//

static void tui_set_color(Tui*, TuiColor fg, TuiColor bg);

static void tui_putc(Tui*, uint32_t cp, unsigned cp_width);
// tui_putc()
// ----------
// Set the current cell to have this codepoint and advance by width cells.
// Note that width is the drawing with of the codepoint. If you are wrong then
// who knows, but terminals are dumb and you can't know how wide a character is
// without querying it directly, so who knows maybe we'll use the kitty
// protocol or something.
//

static void tui_fill_rect(Tui*, int x, int y, unsigned w, unsigned h, uint32_t cp, unsigned cp_width);
// tui_fill_rect()
// ---------------
// Fills the given region of the screen with the given character.
// if cp_width > 1, it will stride by that much.
// Negative origins are clipped; only whole glyphs within the rectangle draw.
//

static void tui_puts(Tui*, StringView);
// tui_puts()
// ----------
// Like calling tui_putc() in a loop, with cp_width=1, for each unicode
// codepoint decoded from the string.
// Does not wrap, intead truncates upon hitting the edge of the terminal.
//

static void tui_set_cursor_visibility(Tui*, _Bool);
// tui_set_cursor_visibility()
// ---------------------------
// Controls whether the cursor can be seen.
// Note that the terminal cursor is a different position than the implicit
// drawing cursor.
//

static void tui_clear_rect(Tui*, int x, int y, unsigned w, unsigned h);
// tui_clear_rect()
// ----------------
// Clears out the given rect, which means setting all characters to 0 and
// colors back to reset.

#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
static void tui_printf(Tui*, const char* fmt, ...);
// tui_printf()
// ------------
// Formats the string thingy and basically calls tui_puts().
//

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
