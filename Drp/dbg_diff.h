//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#ifndef DRP_DBG_DIFF_H
#define DRP_DBG_DIFF_H
#include <stddef.h>
#include <string.h>
#include "stringview.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#endif

typedef struct DbgDiffPrinter DbgDiffPrinter;
struct DbgDiffPrinter {
    void *ctx;
    void (*printer)(void*, const char*, ...);
    const char *_Nullable escape_on,    // escaped characters styling
               *_Nullable escape_reset, // reset
               *_Nullable deleted,      // deleted line
               *_Nullable matched,       // matched line
               *_Nullable added,        // added line
               *_Nullable reset;        // reset deleted/add
};
enum {
    DBG_DIFF_DELETED,
    DBG_DIFF_ADDED,
    DBG_DIFF_MATCHED,
};

static inline StringView dbg_diff_line_slice(StringView*);
static inline void dbg_diff_print_line(size_t, DbgDiffPrinter*, int, StringView);

// Outputs a line-oriented diff of the two strings.
static inline
void
dbg_diff(DbgDiffPrinter* printer, StringView expected, StringView actual) {
    StringView orig = expected;
    size_t lineno = 0;
    size_t last_line_match = 0;
    StringView last_exp_match = {0}, last_act_match = {0};
    while(expected.length || actual.length){
        lineno++;
        StringView exp = dbg_diff_line_slice(&expected);
        StringView act = dbg_diff_line_slice(&actual);
        if(sv_equals(exp, act)){
            if(last_line_match != lineno-1)
                dbg_diff_print_line(lineno, printer, DBG_DIFF_MATCHED, exp);
            last_line_match = lineno;
            last_exp_match = exp;
            last_act_match = act;
            continue;
        }
        if(last_line_match == lineno-1)
            dbg_diff_print_line(last_line_match, printer, DBG_DIFF_MATCHED, last_exp_match);
        if(exp.length)
            dbg_diff_print_line(lineno, printer, DBG_DIFF_DELETED, exp);
        if(act.length)
            dbg_diff_print_line(lineno, printer, DBG_DIFF_ADDED, act);
    }
}

static inline
StringView
dbg_diff_line_slice(StringView* sv){
    const char *nl = memchr(sv->text, '\n', sv->length);
    StringView line = *sv;
    if(!nl){
        sv->length = 0;
        return line;
    }
    size_t len = (size_t)(nl - sv->text + 1);
    sv->text += len;
    sv->length -= len;
    line.length = len;
    return line;
}

static
inline
void
dbg_diff_print_line(size_t lineno, DbgDiffPrinter* printer, int mode, StringView line){
    const char* prefix = NULL;
    switch(mode){
        case DBG_DIFF_ADDED: prefix = printer->added; break;
        case DBG_DIFF_DELETED: prefix = printer->deleted; break;
        case DBG_DIFF_MATCHED: prefix = printer->matched; break;
    }
    if(!prefix) prefix = "";
    const char *end = printer->reset;
    if(!end) end = "";
    printer->printer(printer->ctx, "%s%zu │ ", prefix, lineno);
    const char *on = printer->escape_on;
    if(!on) on = "";
    const char *off = printer->escape_reset;
    if(!off) off = "";
    for(size_t i = 0; i < line.length; i++){
        unsigned char c = (unsigned char)line.text[i];
        const char* esc = NULL;
        switch(c){
            case ' ':  esc = "\xc2\xb7"; break;  // ·
            case '\t': esc = "\\t"; break;
            case '\r': esc = "\\r"; break;
            case '\n': esc = "\\n"; break;
            case '\0': esc = "\\0"; break;
            case '\a': esc = "\\a"; break;
            case '\b': esc = "\\b"; break;
            case '\f': esc = "\\f"; break;
            case '\v': esc = "\\v"; break;
            case 0x1B: esc = "\\e"; break;
            default:
                if(c < 0x20 || c >= 0x7F)
                    printer->printer(printer->ctx, "%s\\x%02X%s", on, c, off);
                else
                    printer->printer(printer->ctx, "%c", c);
                break;
        }
        if(esc)
            printer->printer(printer->ctx, "%s%s%s", on, esc, off);
    }
    printer->printer(printer->ctx, "%s\n", end);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
