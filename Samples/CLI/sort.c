#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Sort lines from fp or file (like sort)
// Usage: Bin/cc Samples/sort.c [file]

const char* input = __argc > 1 ? __argv[1] : NULL;
FILE* fp = input?fopen(input, "rb"):stdin;
if(!fp) return (perror(input), 1);

void* xmalloc(size_t sz){
    void *p = malloc(sz);
    if(!p) exit(1);
    return p;
}
void* xrealloc(void* p, size_t sz){
    p = realloc(p, sz);
    if(!p) exit(1);
    return p;
}

// Read all of fp into a buffer
size_t cap = 4096;
size_t len = 0;
char* buf = xmalloc(cap);
for(;;){
    size_t n = fread(buf+len, 1, cap-len, fp);
    len += n;
    if(n < cap-len) break;
    cap *= 2;
    buf = xrealloc(buf, cap);
}
// Count lines
size_t nlines = 0;
for(size_t i = 0; i < len; i++)
    if(buf[i] == '\n') nlines++;
if(len && buf[len-1] != '\n') nlines++; // last line without newline
// Build array of line pointers
char** lines = xmalloc(nlines * sizeof *lines);
size_t* lens = xmalloc(nlines * sizeof *lens);
size_t li = 0;
size_t start = 0;
for(size_t i = 0; i < len; i++){
    if(buf[i] == '\n'){
        lines[li] = buf + start;
        lens[li] = i - start;
        li++;
        start = i + 1;
    }
}
if(start < len){
    lines[li] = buf + start;
    lens[li] = len - start;
}

// Simple quicksort
void qsort_lines(size_t lo, size_t hi){

    // captureless nested functions
    void swap(size_t a, size_t b){
        char* t = lines[a]; lines[a] = lines[b]; lines[b] = t;
        size_t tl = lens[a]; lens[a] = lens[b]; lens[b] = tl;
    }

    int cmpline(size_t a, size_t b){
        size_t m = lens[a] < lens[b] ? lens[a] : lens[b];
        int r = memcmp(lines[a], lines[b], m);
        if(r) return r;
        if(lens[a] < lens[b]) return -1;
        if(lens[a] > lens[b]) return 1;
        return 0;
    }

    if(hi <= lo + 1) return;
    // median-of-three pivot
    size_t mid = lo + (hi - lo) / 2;
    if(cmpline(lo, mid) > 0) swap(lo, mid);
    if(cmpline(lo, hi-1) > 0) swap(lo, hi-1);
    if(cmpline(mid, hi-1) > 0) swap(mid, hi-1);
    swap(mid, hi-1);
    size_t pivot = hi - 1;
    size_t i = lo, j = hi - 1;
    for(;;){
        while(cmpline(++i, pivot) < 0);
        while(j > lo && cmpline(--j, pivot) > 0);
        if(i >= j) break;
        swap(i, j);
    }
    swap(i, hi-1);
    qsort_lines(lo, i);
    qsort_lines(i+1, hi);
}

if(nlines) qsort_lines(0, nlines);

// Print sorted lines
for(size_t i = 0; i < nlines; i++){
    fwrite(lines[i], 1, lens[i], stdout);
    putchar('\n');
}
free(lines);
free(lens);
free(buf);
