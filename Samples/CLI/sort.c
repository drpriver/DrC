#!/usr/bin/env drc
// Sort lines from stdin or file;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

// Print error with backtrace and exit
__attribute__((format(printf, 1, 2)))
_Noreturn void die(const char* fmt, ...){
    __bt();
    char fmt2[1024];
    snprintf(fmt2, sizeof fmt2, "Fatal error: %s\n", fmt);
    va_list va;
    va_start(va, fmt);
    vfprintf(stderr, fmt2, va);
    exit(1);
}

// realloc with die-on-failure for scripting
void* xrealloc(void* p, size_t sz){
    p = realloc(p, sz);
    if(!p) die("realloc failed: %zu bytes", sz);
    return p;
}

const char* input = _Argc > 1 ? _Argv[1] : NULL;
FILE* fp = input?fopen(input, "rb"):stdin;
if(!fp) die("Can't open '%s': %s", input, strerror(errno));


// Read all of fp into a buffer
size_t cap = 4096;
size_t len = 0;
char* buf = xrealloc(NULL, cap);
for(;;){
    size_t avail = cap-len;
    size_t n = fread(buf+len, 1, avail, fp);
    len += n;
    if(n < avail){
        if(ferror(fp)) die("Error reading '%s': %s", input, strerror(errno));
        break;
    }
    cap *= 2;
    buf = xrealloc(buf, cap);
}
if(fp != stdin) fclose(fp);
char (*_lines)[:] = NULL;
size_t _line_cap = 0;
size_t _line_count = 0;
void add_line(char* start, size_t len){
    if(_line_count == _line_cap){
        _line_cap = _line_cap?_line_cap*2:8;
        _lines = xrealloc(_lines, _line_cap*sizeof *_lines);
    }
    _lines[_line_count++] = start[:len];
}
char* start = buf;
char* end = buf + len;
while(start < end){
    char* p = memchr(start, '\n', end-start);
    if(!p) break;
    add_line(start, p-start);
    start = p+1;
}
if(start < end) add_line(start, end-start);
char lines[:][:] = _lines[:_line_count];
qsort(lines.data, lines.count, sizeof lines[0], int(const void* left, const void* right){
    char a[:] = *(char(*)[:])left;
    char b[:] = *(char(*)[:])right;
    size_t m = a.count < b.count? a.count: b.count;
    int r = memcmp(a.data, b.data, m);
    if(r) return r;
    return (a.count > b.count) - (a.count < b.count);
});

for(size_t i = 0; i < _Countof lines; i++)
    fprintf(stdout, "%.*s\n", (int)lines[i].count, lines[i].data);
free(lines.data);
free(buf);
