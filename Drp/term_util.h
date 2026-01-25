//
// Copyright © 2022-2025, David Priver
//
#ifndef TERM_UTIL_H
#define TERM_UTIL_H
#ifndef _WIN32
#include <stdlib.h>
#endif
#include "windowsheader.h"
#include "posixheader.h"

typedef struct TermSize {
    int columns, rows;
    int xpix, ypix;
} TermSize;
//
// Returns the size of the terminal.
// On error, we return 80 columns and 24 rows.
//
static inline
TermSize
get_terminal_size(void){
    #if defined(__wasm__)
        return (TermSize){80, 24, 0, 0};
    #elif defined(_WIN32)
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        BOOL success = GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
        if(success){
            int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            return (TermSize){columns, rows, 0, 0, /* idk? */};
        }
        return (TermSize){80, 24, 0, 0};
    #else
        struct TermSize result = {80,24, 80*8, 24*16};
        struct winsize w;
        int err = ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        if(err == -1){
            char* cols_s = getenv("COLUMNS");
            if(!cols_s)
                goto err;
            char* rows_s = getenv("ROWS");
            if(!rows_s)
                goto err;
            int cols = atoi(cols_s);
            if(!cols)
                goto err;
            int rows = atoi(rows_s);
            if(!rows)
                goto err;
            result = (TermSize){cols, rows, 8*cols, 16*rows};
        }
        else {
            if(!w.ws_col || !w.ws_row)
                goto err;
            result = (TermSize){w.ws_col, w.ws_row, w.ws_xpixel, w.ws_ypixel};
        }
        err:
        return result;
    #endif
}

static inline
_Bool
stdout_is_terminal(void){
    #if defined(__wasm__)
        return 0;
    #elif defined(_WIN32)
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
        if(h == INVALID_HANDLE_VALUE) return 0;
        if(h == NULL) return 0;
        DWORD ft = GetFileType(h);
        return ft == FILE_TYPE_CHAR;
    #else
        return isatty(STDOUT_FILENO);
    #endif
}
static inline
_Bool
stdin_is_terminal(void){
    #if defined(__wasm__)
        return 0;
    #elif defined(_WIN32)
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
        if(h == INVALID_HANDLE_VALUE) return 0;
        if(h == NULL) return 0;
        DWORD ft = GetFileType(h);
        return ft == FILE_TYPE_CHAR;
    #else
        return isatty(STDIN_FILENO);
    #endif
}
#endif
