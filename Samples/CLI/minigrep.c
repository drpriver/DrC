#!/usr/bin/env drc
// Minigrep: grep with a tiny Thompson NFA regex engine.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <stdint.h>
#pragma typedef on

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

enum { MATCH = 256, SPLIT = 257, ANY = 258, BOL = 259, EOL = 260, CCLASS = 261 };

struct State {
    int op;         // char literal, MATCH, SPLIT, ANY, BOL, EOL, CCLASS
    State *out, 
          *out1;    // only for SPLIT
    uint8_t cc[32]; // bit set for CCLASS
    _Bool cc_neg;       // negated class
    size_t lastlist;
};

State states[512];
size_t nstates;

State* newstate(int op, State* out, State* out1){
    if(nstates >= _Countof states)
        die("Too many NFA states");
    State* s = &states[nstates++];
    *s = {
        .op = op,
        .out = out,
        .out1 = out1,
    };
    return s;
}

// Parser produces NFA fragments
struct Frag{
    State* start;
    // Dangling out pointers to patch
    State*** ptrs;
    size_t nptrs;
    size_t cap;

    void patch(_Self* self, State* s){
        for(size_t i = 0; i < self.nptrs; i++) *self.ptrs[i] = s;
    }
    _Self cat(_Self a, _Self b){
        a.patch(b.start);
        return {
            .start = a.start,
            .ptrs = b.ptrs,
            .nptrs = b.nptrs,
        };
    }
    void addptr(_Self* self, State** p){
        if(ptrsused >= _Countof ptrsbuf) die("Too many dangling NFA pointers");
        ptrsbuf[ptrsused++] = p;
        self.nptrs++;
    }

    _Self alt(_Self a, _Self b){
        State* s = newstate(SPLIT, a.start, b.start);
        Frag f = mkfrag(s);
        // Collect dangling pointers from both
        for(size_t i = 0; i < a.nptrs; i++)
            f.addptr(a.ptrs[i]);
        for(size_t i = 0; i < b.nptrs; i++)
            f.addptr(b.ptrs[i]);
        return f;
    }
};

State** ptrsbuf[4096];
size_t ptrsused;

Frag mkfrag(State* start){
    return {
        .start = start,
        .ptrs = &ptrsbuf[ptrsused],
    };
}

// Recursive descent parser for regex
struct Parser {
    const char* p; // current position in pattern
    Frag parse_alt(_Self* self){
        Frag f = self.parse_cat();
        while(*self.p == '|'){
            self.p++;
            Frag g = self.parse_cat();
            f = f.alt(g);
        }
        return f;
    }

    Frag parse_cat(_Self* self){
        // Empty concat = epsilon
        if(*self.p == '\0' || *self.p == '|' || *self.p == ')'){
            State* s = newstate(SPLIT, NULL, NULL);
            Frag f = mkfrag(s);
            // Both outs dangle
            f.addptr(&s->out);
            f.addptr(&s->out1);
            return f;
        }
        Frag f = self.parse_rep();
        while(*self.p != '\0' && *self.p != '|' && *self.p != ')'){
            Frag g = self.parse_rep();
            f = f.cat(g);
        }
        return f;
    }

    Frag parse_rep(_Self* self){
        Frag f = self.parse_atom();
        if(*self.p == '*'){
            self.p++;
            State* s = newstate(SPLIT, f.start, NULL);
            f.patch(s);
            Frag r = mkfrag(s);
            r.addptr(&s->out1);
            return r;
        }
        if(*self.p == '+'){
            self.p++;
            State* s = newstate(SPLIT, f.start, NULL);
            f.patch(s);
            Frag r = mkfrag(f.start);
            r.addptr(&s->out1);
            return r;
        }
        if(*self.p == '?'){
            self.p++;
            State* s = newstate(SPLIT, f.start, NULL);
            Frag r = mkfrag(s);
            for(size_t i = 0; i < f.nptrs; i++)
                r.addptr(f.ptrs[i]);
            r.addptr(&s->out1);
            return r;
        }
        return f;
    }

    Frag parse_atom(_Self* self){
        if(*self.p == '('){
            self.p++;
            Frag f = self.parse_alt();
            if(*self.p == ')') self.p++;
            return f;
        }
        if(*self.p == '['){
            self.p++;
            State* s = newstate(CCLASS, NULL, NULL);
            if(*self.p == '^'){
                s->cc_neg = true;
                self.p++;
            }
            int first = 1;
            while(*self.p && (*self.p != ']' || first)){
                first = 0;
                int lo = (unsigned char)*self.p++;
                if(*self.p == '-' && self.p[1] && self.p[1] != ']'){
                    self.p++;
                    int hi = (unsigned char)*self.p++;
                    for(int c = lo; c <= hi; c++)
                        s->cc[c>>3] |= (uint8_t)(1 << (c & 7));
                } 
                else
                    s->cc[lo>>3] |= (uint8_t)(1 << (lo & 7));
            }
            if(*self.p == ']') self.p++;
            Frag f = mkfrag(s);
            f.addptr(&s->out);
            return f;
        }
        if(*self.p == '^'){
            self.p++;
            State* s = newstate(BOL, NULL, NULL);
            Frag f = mkfrag(s);
            f.addptr(&s->out);
            return f;
        }
        if(*self.p == '$'){
            self.p++;
            State* s = newstate(EOL, NULL, NULL);
            Frag f = mkfrag(s);
            f.addptr(&s->out);
            return f;
        }
        if(*self.p == '.'){
            self.p++;
            State* s = newstate(ANY, NULL, NULL);
            Frag f = mkfrag(s);
            f.addptr(&s->out);
            return f;
        }
        if(*self.p == '\\' && self.p[1]){
            self.p++;
        }
        int ch = (unsigned char)*self.p++;
        State* s = newstate(ch, NULL, NULL);
        Frag f = mkfrag(s);
        f.addptr(&s->out);
        return f;
    }
};

// --- NFA simulation ---

State* matchstate;
State* listbuf1[_Countof states];
State* listbuf2[_Countof states];
size_t listid;

struct List {
    State** s;
    size_t n;
};

void addstate(List* l, State* s, _Bool at_bol, _Bool at_eol){
    if(!s || s->lastlist == listid) return;
    s->lastlist = listid;
    if(s->op == SPLIT){
        addstate(l, s->out, at_bol, at_eol);
        addstate(l, s->out1, at_bol, at_eol);
        return;
    }
    if(s->op == BOL){
        if(at_bol) addstate(l, s->out, at_bol, at_eol);
        return;
    }
    if(s->op == EOL){
        if(at_eol) addstate(l, s->out, at_bol, at_eol);
        return;
    }
    l->s[l->n++] = s;
}


bool nfa_match(State* start, const char* str, size_t len){
    List cur, nxt;
    cur.s = listbuf1; cur.n = 0;
    nxt.s = listbuf2; nxt.n = 0;
    listid++;
    addstate(&cur, start, .at_bol=1, .at_eol=len == 0);
    for(size_t i = 0; i < len; i++){
        int ch = (unsigned char)str[i];
        int next_eol = (i + 1 == len);
        for(size_t j = 0; j < cur.n; j++)
            if(cur.s[j] == matchstate) return true;
        listid++;
        nxt.n = 0;
        for(size_t j = 0; j < cur.n; j++){
            State* s = cur.s[j];
            bool matched = false;
            if(s->op == ANY)         matched = (ch != '\n');
            else if(s->op == CCLASS) { int h = (s->cc[ch>>3] >> (ch&7)) & 1; matched = s->cc_neg ? !h : h; }
            else if(s->op == ch)     matched = true;
            if(matched) addstate(&nxt, s->out, .at_bol=0, .at_eol=next_eol);
        }
        // Unanchored: also try starting fresh at this position
        addstate(&nxt, start, .at_bol=0, .at_eol=next_eol);
        State** tmp = cur.s; cur.s = nxt.s; cur.n = nxt.n; nxt.s = tmp;
    }
    for(size_t j = 0; j < cur.n; j++)
        if(cur.s[j] == matchstate) return true;
    return false;
}

void compile_and_run(const char* pattern, const char* input){
    Parser p = {pattern};
    ptrsused = 0;
    nstates = 0;
    matchstate = newstate(MATCH, NULL, NULL);
    Frag compiled = p.parse_alt();
    compiled.patch(matchstate);
    State* start = compiled.start;

    // Read stdin line by line, print matching lines
    char line[8192];
    FILE* fp = input?fopen(input, "rb"):stdin;
    if(!fp) die("Unable to open '%s': %s", input, strerror(errno));
    int lineno = 0;
    while(fgets(line, sizeof line, fp)){
        lineno++;
        size_t len = strlen(line);
        if(len > 0 && line[len - 1] == '\n'){
            line[len - 1] = '\0';
            len--;
        }
        if(nfa_match(start, line, len))
            printf("%s:%d: %s\n", input?input:"(stdin)", lineno, line);
    }
    if(ferror(fp)) die("Error reading '%s': %s", input?input:"stdin", strerror(errno));
    if(input) fclose(fp);
}


int main(int argc, char** argv){
    const char* pattern = argc > 1 ? argv[1] : ".*";
    if(argc <= 2){
        compile_and_run(pattern, NULL);
    }
    else {
        for(int i = 2; i < argc; i++)
            compile_and_run(pattern, argv[i]);
    }
    return 0;
}
