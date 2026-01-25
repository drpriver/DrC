//
// Copyright © 2021-2025, David Priver <david@davidpriver.com>
//
#ifndef GET_INPUT_C
#define GET_INPUT_C
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "windowsheader.h"
#include "posixheader.h"

#include "get_input.h"
#include "mem_util.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Nonnull
#define _Nonnull
#endif
#endif


#if 0
// Debug code that logs to a file so you can understand wtf is going on
// Do a tail -f on it.

FILE* loop_fp;
void vdbg(const char* fmt, va_list args){
    if(!loop_fp){
        loop_fp = fopen("debug.txt", "a");
        setbuf(loop_fp, NULL);
    }
    vfprintf(loop_fp, fmt, args);
}

__attribute__((format(printf,1, 2)))
void dbg(const char*fmt, ...){
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vdbg(fmt, args);
    __builtin_va_end(args);
}
#define DBG(fmt, ...) dbg("%s:%3d | " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define DBG(...) (void)0
#endif

// Whether we have done any global initialization code.
// Currently this is just whether or not we have put the terminal in VT
// Processing mode on Windows.
static int get_line_is_init;
static void get_line_init(void);
static ssize_t get_line_internal(GetInputCtx*);
static ssize_t get_line_internal_loop(GetInputCtx*);

struct TermState;
static void enable_raw(struct TermState*);
static void disable_raw(struct TermState*);

static void change_history(GetInputCtx*, int magnitude);
static void redisplay(GetInputCtx*);
static void delete_right(GetInputCtx*);
static void insert_char_into_line(GetInputCtx*, char);
static inline void free_const_char_pointer(const char* p);

static inline
ssize_t
read_one(char* buff){
#ifdef _WIN32
    static const char* remaining;
    if(remaining){
        DBG("*remaining: '%c'\n", *remaining);
        *buff = *remaining++;
        if(!*remaining)
            remaining = NULL;
        return 1;
    }
    for(;;){
        extern int _getch(void);
        int c = _getch();
        DBG("c = %d\n", c);
        switch(c){
            case 0:
            case 224:{
                int next = _getch();
                DBG("next = %d\n", next);
                switch(next){
                    // left cursor
                    case 'K':
                        *buff = '\033';
                        remaining = "[D";
                        return 1;
                        // up
                    case 'H':
                        *buff = '\033';
                        remaining = "[A";
                        return 1;
                        // down
                    case 'P':
                        *buff = '\033';
                        remaining = "[B";
                        return 1;
                        // right
                    case 'M':
                        *buff = '\033';
                        remaining = "[C";
                        return 1;
                        // home
                    case 'G':
                        *buff = '\x01';
                        return 1;
                        // end
                    case 'O':
                        *buff = '\x05';
                        return 1;
                    case 'S': // del
                        // *buff = '\x7f';
                        *buff = '\033';
                        remaining = "3~";
                        return 1;

                        // insert
                        // case 'R':

                        // pgdown
                        // case 'Q':

                        // pgup
                        // case 'I':

                    default:
                        continue;
                }
            }
            default:
                *buff = (char)(unsigned char)(unsigned)c;
                return 1;
        }
    }
    return 1;
#else
    return read(STDIN_FILENO, buff, 1);
#endif
}

static inline
ssize_t
write_data(const char*buff, size_t len){
#ifdef _WIN32
    fwrite(buff, len, 1, stdout);
    fflush(stdout);
    return len;
#else
    return write(STDOUT_FILENO, buff, len);
#endif
}

static inline
void*_Nullable
memdup(const void* src, size_t size){
    if(!size) return NULL;
    void* p = malloc(size);
    if(p) memcpy(p, src, size);
    return p;
}

GET_INPUT_API
ssize_t
gi_get_input(GetInputCtx*ctx){
    ctx->_hst_cursor = ctx->_hst_count;
    ctx->_cols = gi_get_cols();
    ctx->buff_count = 0;
    ctx->buff_cursor = 0;
    ctx->tab_completion_cookie = 0;
    memset(ctx->buff, 0, GI_BUFF_SIZE);
    ssize_t length = get_line_internal(ctx);
    return length;
}

GET_INPUT_API
ssize_t
gi_get_input2(GetInputCtx*ctx, size_t preserved){
    ctx->_hst_cursor = ctx->_hst_count;
    ctx->_cols = gi_get_cols();
    ctx->buff_count = preserved;
    ctx->buff_cursor = preserved;
    ctx->tab_completion_cookie = 0;
    memset(ctx->buff+preserved, 0, GI_BUFF_SIZE-preserved);
    ssize_t length = get_line_internal(ctx);
    return length;
}

#ifdef _WIN32
// On windows you can get an unbuffered, non-echoing,
// etc. read_one by just calling _getch().
// So, no need to do anything special here.
struct TermState {
    char c; // Avoid UB.
};
static void enable_raw(struct TermState*ts){
    (void)ts;
}
static void disable_raw(struct TermState*ts){
    (void)ts;
}
#else
struct TermState {
    struct termios raw;
    struct termios orig;
};
static
void
enable_raw(struct TermState*ts){
    if(tcgetattr(STDIN_FILENO, &ts->orig) == -1)
        return;
    ts->raw = ts->orig;
    ts->raw.c_iflag &= ~(0lu
            | BRKINT // no break
            | ICRNL  // don't map CR to NL
            | INPCK  // skip parity check
            | ISTRIP // don't strip 8th bit off char
            | IXON   // don't allow start/stop input control
            );
    ts->raw.c_oflag &= ~(0lu
        | OPOST // disable post processing
        );
    ts->raw.c_cflag |= CS8; // 8 bit chars
    ts->raw.c_lflag &= ~(0lu
            | ECHO    // disable echo
            | ICANON  // disable canonical processing
            | IEXTEN  // no extended functions
            // Currently allowing these so ^Z works, could disable them
            | ISIG    // disable signals
            );
    ts->raw.c_cc[VMIN] = 1; // read every single byte
    ts->raw.c_cc[VTIME] = 0; // no timeout

    // set and flush
    // Change will ocurr after all output has been transmitted.
    // Unread input will be discarded.
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts->raw) < 0)
        return;
}
static
void
disable_raw(struct TermState*ts){
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ts->orig);
}
#endif

static
ssize_t
get_line_internal(GetInputCtx* ctx){
    if(!get_line_is_init)
        get_line_init();
    struct TermState termstate;
    enable_raw(&termstate);
    ssize_t result_length = get_line_internal_loop(ctx);
    disable_raw(&termstate);
    return result_length;
}

GET_INPUT_API
int
gi_get_one_char(void){
    if(!get_line_is_init)
        get_line_init();
    struct TermState termstate;
    enable_raw(&termstate);
    char c;
    ssize_t nread = read_one(&c);
    if(nread < 0) return -1;
    disable_raw(&termstate);
    return c;
}


static
ssize_t
get_line_internal_loop(GetInputCtx* ctx){
    DBG("Enter Loop\n");
    DBG("----------\n");
    _Bool in_tab = 0;
    int n_tabs = 0;
    write_data(ctx->prompt.text, ctx->prompt.length);
    redisplay(ctx);
    size_t original_curr_pos=0, original_used_len=0;
    enum {
        CTRL_A = 1,         // Ctrl-a
        CTRL_B = 2,         // Ctrl-b
        CTRL_C = 3,         // Ctrl-c
        CTRL_D = 4,         // Ctrl-d
        CTRL_E = 5,         // Ctrl-e
        CTRL_F = 6,         // Ctrl-f
        // CTRL_G = 7,      // unused
        CTRL_H = 8,         // Ctrl-h
        TAB    = 9,         // Tab / Ctrl-i
        CTRL_J = 10,        // Accept
        CTRL_K = 11,        // Ctrl-k
        CTRL_L = 12,        // Ctrl-l
        ENTER = 13,         // Enter / Ctrl-m
        CTRL_N = 14,        // Ctrl-n
        CTRL_O = 15,        // Ctrl-o
        CTRL_P = 16,        // Ctrl-p
        // CTRL_Q = 17,
        CTRL_R = 18,
        // CTRL_S = 19,
        CTRL_T = 20,        // Ctrl-t
        CTRL_U = 21,        // Ctrl-u
        CTRL_V = 22,        // Ctrl-v
        CTRL_W = 23,        // Ctrl-w
        // CTRL_X = 24,
        // CTRL_Y = 25,
        CTRL_Z = 26,        // Ctrl-z
        ESC = 27,           // Escape
        BACKSPACE =  127,   // Backspace
        // fake key codes
        #undef DELETE
        DELETE    = -1,
        UP        = -2,
        DOWN      = -3,
        LEFT      = -4,
        RIGHT     = -5,
        HOME      = -6,
        END       = -7,
        SHIFT_TAB = -8,
    };
    for(;;){
        char _c;
        char sequence[8];
        ssize_t nread = read_one(&_c);
        int c = (int)(unsigned char)_c;
        if(nread <= 0)
            return ctx->buff_count;
        if(c == ESC){
            DBG("ESC\n");
            if(read_one(sequence) == -1) return -1;
            if(read_one(sequence+1) == -1) return -1;
            DBG("sequence[0] = %d\n", sequence[0]);
            DBG("sequence[1] = %d\n", sequence[1]);
            if(sequence[0] == '['){
                if (sequence[1] >= '0' && sequence[1] <= '9'){
                    // Extended escape, read additional byte.
                    if (read_one(sequence+2) == -1) return -1;
                    if (sequence[2] == '~') {
                        switch(sequence[1]) {
                        case '3': /* Delete key. */
                            c = DELETE;
                            break;
                        }
                    }
                }
                else {
                    switch(sequence[1]) {
                    case 'A': // Up
                        c = UP;
                        break;
                    case 'B': // Down
                        c = DOWN;
                        break;
                    case 'C': // Right
                        c = RIGHT;
                        break;
                    case 'D': // Left
                        c = LEFT;
                        break;
                    case 'H': // Home
                        c = HOME;
                        break;
                    case 'F': // End
                        c = END;
                        break;
                    case 'Z': // Shift-tab
                        c = SHIFT_TAB;
                        break;
                    }
                }
            }
            else if(sequence[0] == 'O'){
                switch(sequence[1]){
                    case 'H': // Home
                        c = HOME;
                        break;
                    case 'F': // End
                        c = END;
                        break;
                }
            }
        }
        if(c != TAB && c != SHIFT_TAB){
            in_tab = 0;
            ctx->tab_completion_cookie = 0;
            n_tabs = 0;
        }
        if(c == TAB || (c == SHIFT_TAB && n_tabs > 0)){
            if(c == SHIFT_TAB)
                n_tabs--;
            else
                n_tabs++;
            // ignore tabs if no completion function
            if(!ctx->tab_completion_func)
                continue;
            if(!in_tab){
                original_curr_pos = ctx->buff_cursor;
                original_used_len = ctx->buff_count;
                memcpy(ctx->altbuff, ctx->buff, GI_BUFF_SIZE);
                ctx->altbuff[original_used_len] = 0;
            }
            in_tab = 1;
            int err = ctx->tab_completion_func(ctx, original_curr_pos, original_used_len, n_tabs);
            if(err){
                if(err > 0){
                    // user fucked up and gave us a positive error code.
                    return -err;
                }
                return err;
            }
            redisplay(ctx);
            continue;
        }
        switch(c){
            case CTRL_J:
            case ENTER:
                DBG("ENTER\n");
                write_data("\n", 1);
                return ctx->buff_count;
            case BACKSPACE: case CTRL_H:
                DBG("BACKSPACE\n");
                if(ctx->buff_cursor > 0 && ctx->buff_count > 0){
                    size_t n = 1;
                    if(ctx->buff_cursor >= 2 && !(ctx->buff_cursor & 1)){
                        if(ctx->buff[ctx->buff_cursor-1] == ' ' && ctx->buff[ctx->buff_cursor-2] == ' '){
                            n = 2;
                        }
                    }
                    memremove(ctx->buff_cursor-n, ctx->buff, ctx->buff_count+n, n);
                    ctx->buff_cursor-=n;
                    ctx->buff_count-=n;
                    redisplay(ctx);
                }
                break;
            case DELETE:
            case CTRL_D:
                DBG("CTRL_D\n");
                if(ctx->buff_count > 0){
                    delete_right(ctx);
                    redisplay(ctx);
                }
                else if(c != DELETE) {
                    write_data("^D\r\n", 4);
                    return -1;
                }
                break;
            case CTRL_T:
                DBG("CTRL_T\n");
                if(ctx->buff_cursor > 0 && ctx->buff_cursor < ctx->buff_count){
                    // swap with previous
                    char temp = ctx->buff[ctx->buff_cursor-1];
                    ctx->buff[ctx->buff_cursor-1] = ctx->buff[ctx->buff_cursor];
                    ctx->buff[ctx->buff_cursor] = temp;
                    if (ctx->buff_cursor != ctx->buff_count-1)
                        ctx->buff_cursor++;
                    redisplay(ctx);
                }
                break;
            case LEFT:
            case CTRL_B:
                DBG("CTRL_B\n");
                if(ctx->buff_cursor > 0){
                    ctx->buff_cursor--;
                    redisplay(ctx);
                }
                break;
            case RIGHT:
            case CTRL_F:
                DBG("CTRL_F\n");
                if(ctx->buff_cursor != ctx->buff_count){
                    ctx->buff_cursor++;
                    redisplay(ctx);
                }
                break;
            case UP:
            case CTRL_P:
                DBG("CTRL_P\n");
                change_history(ctx, -1);
                redisplay(ctx);
                break;
            case DOWN:
            case CTRL_N:
                DBG("CTRL_N\n");
                change_history(ctx, +1);
                redisplay(ctx);
                break;
            case ESC:
                break;
            default:
                if(c > 0){
                    DBG("default ('%d')\n", c);
                    DBG("default ('%c')\n", c);
                    if((unsigned char)c < 27)
                        continue;
                    insert_char_into_line(ctx, (char)c);
                    redisplay(ctx);
                }
                break;
            case CTRL_C:
                DBG("CTRL_C\n");
                ctx->buff[0] = '\0';
                ctx->buff_cursor = 0;
                ctx->buff_count = 0;
                redisplay(ctx);
                break;
            case CTRL_U: // Delete entire line
                DBG("CTRL_U\n");
                ctx->buff[0] = '\0';
                ctx->buff_cursor = 0;
                ctx->buff_count = 0;
                redisplay(ctx);
                break;
            case CTRL_K: // Delete to end of line
                DBG("CTRL_K\n");
                ctx->buff[ctx->buff_cursor] = 0;
                ctx->buff_count = ctx->buff_cursor;
                redisplay(ctx);
                break;
            case HOME:
            case CTRL_A: // Home
                DBG("CTRL_A\n");
                ctx->buff_cursor = 0;
                redisplay(ctx);
                break;
            case END:
            case CTRL_E: // End
                DBG("CTRL_E\n");
                ctx->buff_cursor = ctx->buff_count;
                redisplay(ctx);
                break;
            case CTRL_L: // Clear entire screen
                DBG("CTRL_L\n");
                #define CLEARSCREEN "\x1b[H\x1b[2J"
                write_data(CLEARSCREEN, sizeof(CLEARSCREEN)-1);
                #undef CLEARSCREEN
                redisplay(ctx);
                break;
            case CTRL_W:{ // Delete previous word
                DBG("CTRL_W\n");
                size_t old_pos = ctx->buff_cursor;
                size_t diff;
                size_t pos = ctx->buff_cursor;
                // Backup until we hit a nonspace.
                while(pos > 0 && ctx->buff[pos-1] == ' ')
                    pos--;
                // Backup until we hit a space.
                while(pos > 0 && ctx->buff[pos-1] != ' ')
                    pos--;
                diff = old_pos - pos;
                memmove(ctx->buff+pos, ctx->buff+old_pos, ctx->buff_count-old_pos+1);
                ctx->buff_cursor = pos;
                ctx->buff_count -= diff;
                redisplay(ctx);
            }break;
            case CTRL_R:{
                change_history(ctx, -1);
                redisplay(ctx);
            }break;
            case CTRL_Z:{
                DBG("CTRL_Z\n");
                write_data("^Z\r\n", 4);
                #if !defined(_WIN32)
                raise(SIGTSTP);
                #endif
                DBG("resume\n");
                redisplay(ctx);
            }break;
            #if 0
            case CTRL_V:{
                DBG("CTRL_V\n");
                id pb = [NSPasteboard generalPasteboard];
                id data = [pb dataForType:NSPasteboardTypeString];
                id s = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
                const char* c = [s cString];
                DBG("c = %s\n", c);
                size_t len = strlen(c);
                if(len)len--;
                memmove(ls.buff+ls.curr_pos+len, ls.buff+ls.curr_pos, ls.length-ls.curr_pos);
                memcpy(ls.buff+ls.curr_pos, c, len);
                ls.curr_pos += len;
                ls.length += len;
                for(;;){
                    char* newline = memchr(ls.buff, '\n', ls.length);
                    if(!newline) break;
                    *newline = ' ';
                }
                redisplay(&ls);
                [s release];
                [data release];
                [pb release];
            }break;
            #endif
        }
    }
    return ctx->buff_count;
}

typedef struct GiSimpleWriter GiSimpleWriter;
struct GiSimpleWriter {
    char buff[GI_BUFF_SIZE];
    size_t cursor;
    int overflowed;
};

static inline
void
gis_write(GiSimpleWriter*buff, const char* data, size_t length){
    int err = memappend(buff->buff, GI_BUFF_SIZE, buff->cursor, data, length);
    if(err){
        buff->overflowed = 1;
        return;
    }
    buff->cursor += length;
}

static inline
void
gis_put(GiSimpleWriter*buff, char c){
    size_t remainder = GI_BUFF_SIZE - buff->cursor;
    if(1 > remainder){
        buff->overflowed = 1;
        return;
    }
    buff->buff[buff->cursor++] = c;
}

static
void
redisplay(GetInputCtx*ctx){
    GiSimpleWriter writer;
    writer.cursor = 0;
    writer.overflowed = 0;
    // WARNING: Do not confuse display length and buffer length for the prompt.
    size_t plen = ctx->prompt_display_length?ctx->prompt_display_length:ctx->prompt.length;
    char* buff = ctx->buff;
    size_t len = ctx->buff_count;
    size_t pos = ctx->buff_cursor;
    size_t cols = ctx->_cols;

    // Scroll the text right until the current cursor position
    // fits on screen.
    while((plen+pos) >= cols){
        buff++;
        len--;
        pos--;
    }
    // Truncate the string so it fits on screen.
    while(plen+len > cols){
        len--;
    }
    // Move to left.
    gis_put(&writer, '\r');

    // Copy the prompt.
    // Do not confuse the prompt display length for the actual length in bytes.
    gis_write(&writer, ctx->prompt.text, ctx->prompt.length);

    // Copy the visible section of the buffer.
    gis_write(&writer, buff, len);
    // Erase anything remaining on this line to the right.
    #define ERASERIGHT "\x1b[0K"
    gis_write(&writer, ERASERIGHT, sizeof(ERASERIGHT)-1);
    #undef ERASERIGHT
    // Move cursor back to original position.
    char tmp[128];
    int printsize = (snprintf)(tmp, sizeof tmp, "\r\x1b[%zuC", pos+plen);
    gis_write(&writer, tmp, printsize);

    // Actually write to the terminal.
    if(writer.overflowed) return;
    write_data(writer.buff, writer.cursor);
}

static
void
delete_right(GetInputCtx* ctx){
    if(!ctx->buff_count)
        return;
    if(ctx->buff_cursor >= ctx->buff_count)
        return;
    memremove(ctx->buff_cursor, ctx->buff, ctx->buff_count, 1);
    ctx->buff_count--;
}

static
void
insert_char_into_line(GetInputCtx* ctx, char c){
    if(ctx->buff_count >= GI_BUFF_SIZE)
        return;
    char* buff = ctx->buff;
    // At the end of the line anyway
    if(ctx->buff_count == ctx->buff_cursor){
        buff[ctx->buff_cursor++] = c;
        buff[++ctx->buff_count] = '\0';
        return;
    }
    // Write into the middle of the buffer
    memmove(buff+ctx->buff_cursor+1, buff+ctx->buff_cursor, ctx->buff_count-ctx->buff_cursor);
    buff[ctx->buff_cursor++] = c;
    buff[++ctx->buff_count] = '\0';
}

GET_INPUT_API
void
gi_add_line_to_history_len(GetInputCtx* ctx, const char* text, size_t length){
    if(!length)
        return; // no empties
    if(ctx->_hst_count){
        LongString* last = &ctx->_history[ctx->_hst_count-1];
        if(length == last->length && memcmp(text, last->text, length) == 0)
            return; // Don't allow duplicates
    }
    char* copy = malloc(length+1);
    memcpy(copy, text, length);
    copy[length] = 0;
    if(ctx->_hst_count == GI_LINE_HISTORY_MAX){
        free_const_char_pointer(ctx->_history[0].text);
        memmove(ctx->_history, ctx->_history+1, (GI_LINE_HISTORY_MAX-1)*sizeof(ctx->_history[0]));
        ctx->_history[GI_LINE_HISTORY_MAX-1] = (LongString){
            .length = length,
            .text = copy,
        };
    }
    else {
        ctx->_history[ctx->_hst_count++] = (LongString){.length=length, .text=copy};
    }
}

GET_INPUT_API
void
gi_remove_last_line_from_history(GetInputCtx* ctx){
    if(!ctx->_hst_count)
        return;
    LongString* last = &ctx->_history[--ctx->_hst_count];
    free_const_char_pointer(last->text);
    return;
}

GET_INPUT_API
void
gi_add_line_to_history(GetInputCtx* ctx, StringView sv){
    gi_add_line_to_history_len(ctx, sv.text, sv.length);
}


static
void
change_history(GetInputCtx*ctx, int magnitude){
    // DBG("magnitude: %d\n", magnitude);
    // DBG("input_line_history_cursor: %d\n", history->cursor);
    // DBG("input_line_history_count: %d\n", history->count);
    ctx->_hst_cursor += magnitude;
    if(ctx->_hst_cursor < 0)
        ctx->_hst_cursor = 0;
    if(ctx->_hst_cursor >= ctx->_hst_count){
        ctx->_hst_cursor = ctx->_hst_count;
        ctx->buff_count = 0;
        ctx->buff_cursor = 0;
        ctx->buff[ctx->buff_count] = '\0';
        return;
    }
    if(ctx->_hst_cursor < 0)
        return;
    LongString old = ctx->_history[ctx->_hst_cursor];
    size_t length = old.length < GI_BUFF_SIZE? old.length : GI_BUFF_SIZE;
    if(length)
        memcpy(ctx->buff, old.text, length);
    ctx->buff[length] = '\0';
    ctx->buff_count = length;
    ctx->buff_cursor = length;
}

static void get_line_init(void){
    get_line_is_init = 1;
#ifdef _WIN32
    // In theory we should open "CONOUT$" instead, but idk.
    // TODO: report errors.
    HANDLE hnd = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    BOOL success = GetConsoleMode(hnd, &mode);
    if(!success)
        return;
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    success = SetConsoleMode(hnd, mode);
    if(!success)
        return;
    hnd = GetStdHandle(STD_INPUT_HANDLE);
    success = GetConsoleMode(hnd, &mode);
    if(!success)
        return;
    mode |= ENABLE_VIRTUAL_TERMINAL_INPUT;
    success = SetConsoleMode(hnd, mode);
    if(!success)
        return;
#endif
    // Someone might have hidden the cursor, which is annoying.
#define SHOW_CURSOR "\033[?25h"
    DBG("SHOW_CURSOR\n");
    fputs(SHOW_CURSOR, stdout);
    fflush(stdout);
#undef SHOW_CURSOR
}

GET_INPUT_API
int
gi_get_cols(void){
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    BOOL success = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    if(!success)
        goto failed;
    int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    return columns;
#else
    struct winsize ws;
    if(ioctl(1, TIOCGWINSZ, &ws) == -1)
        goto failed;
    if(ws.ws_col == 0)
        goto failed;
    return ws.ws_col;
#endif

failed:
    return 80;
}

static inline
int
gi_combine_path(char* buff, size_t bufflen, const char* prefix, const char* filename){
    size_t pref_len = prefix?strlen(prefix):0;
    size_t filename_len = strlen(filename);
    if(pref_len + filename_len + 1+1 >= bufflen)
        return 1;
    if(pref_len){
        memcpy(buff, prefix, pref_len);
        buff += pref_len;
        buff[0] = '/';
        buff++;
    }
    memcpy(buff, filename, filename_len+1);
    return 0;
}

GET_INPUT_API
int
gi_dump_history(GetInputCtx* ctx, const char* prefix, const char* filename){
    char buff[512];
    int err = gi_combine_path(buff, sizeof buff, prefix, filename);
    if(err) return 1;
    FILE* fp = fopen(buff, "w");
    if(!fp)
        return 1;
    for(int i = 0; i < ctx->_hst_count; i++){
        fwrite(ctx->_history[i].text, ctx->_history[i].length, 1, fp);
        fputc('\n', fp);
    }
    fflush(fp);
    fclose(fp);
    return 0;
}

static inline
void
free_const_char_pointer(const char* p){
    #ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcast-qual"
        free((char*)p);
    #pragma clang diagnostic pop
    #elif defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    #pragma GCC diagnostic ignored "-Wcast-qual"
        free((char*)p);
    #pragma GCC diagnostic pop
    #else
        free((char*)p);
    #endif
}


GET_INPUT_API
int
gi_load_history(GetInputCtx* ctx, const char* prefix, const char *filename){
    char buff[1024];
    int err = gi_combine_path(buff, sizeof buff, prefix, filename);
    if(err) return 1;
    FILE* fp = fopen(buff, "r");
    if(!fp) return 1;
    for(int i = 0; i < ctx->_hst_count; i++){
        free_const_char_pointer(ctx->_history[i].text);
    }
    ctx->_hst_count = 0;
    while(fgets(buff, sizeof(buff), fp)){
        size_t length = strlen(buff);
        buff[--length] = '\0';
        if(!length)
            continue;
        char* copy = memdup(buff, length+1);
        LongString* h = &ctx->_history[ctx->_hst_count++];
        h->text = copy;
        h->length = length;
    }
    return 0;
}

GET_INPUT_API
void
gi_destroy_ctx(GetInputCtx* ctx){
    for(int i = 0; i < ctx->_hst_count; i++){
        free_const_char_pointer(ctx->_history[i].text);
    }
    ctx->_hst_count = 0;
    ctx->_hst_cursor = 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
