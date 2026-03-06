#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Minigrep: grep with a tiny Thompson NFA regex engine.
// Supports: . * + ? | () [] [^] ^ $
// Usage: set PATTERN env var, reads stdin.
//   PATTERN="foo|bar" ./minigrep < file.txt

const char* pattern = __ENV__("PATTERN", ".*");

// --- NFA compiler ---

enum { MATCH = 256, SPLIT = 257, ANY = 258, BOL = 259, EOL = 260, CCLASS = 261 };
enum { MAXSTATE = 512 };

typedef struct State State;
struct State {
    int op;           // char literal, MATCH, SPLIT, ANY, BOL, EOL, CCLASS
    State* out;
    State* out1;      // only for SPLIT
    unsigned char cc[32]; // bit set for CCLASS (256 bits)
    int cc_neg;       // negated class
    int lastlist;
};

State states[MAXSTATE];
int nstates;

State* newstate(int op, State* out, State* out1){
    if(nstates >= MAXSTATE){
        fprintf(stderr, "too many NFA states\n");
        exit(1);
    }
    State* s = &states[nstates++];
    s->op = op;
    s->out = out;
    s->out1 = out1;
    s->lastlist = 0;
    s->cc_neg = 0;
    memset(s->cc, 0, 32);
    return s;
}

// Parser produces NFA fragments
typedef struct {
    State* start;
    // Dangling out pointers to patch
    State*** ptrs;
    int nptrs;
    int cap;
} Frag;

State** ptrsbuf[4096];
int ptrsused;

Frag mkfrag(State* start){
    Frag f;
    f.start = start;
    f.ptrs = &ptrsbuf[ptrsused];
    f.nptrs = 0;
    f.cap = 0;
    return f;
}

void frag_addptr(Frag* f, State** p){
    ptrsbuf[ptrsused++] = p;
    f->nptrs++;
}

void frag_patch(Frag* f, State* s){
    for(int i = 0; i < f->nptrs; i++)
        *f->ptrs[i] = s;
}

Frag frag_cat(Frag a, Frag b){
    frag_patch(&a, b.start);
    Frag f;
    f.start = a.start;
    f.ptrs = b.ptrs;
    f.nptrs = b.nptrs;
    return f;
}

Frag frag_alt(Frag a, Frag b){
    State* s = newstate(SPLIT, a.start, b.start);
    Frag f = mkfrag(s);
    // Collect dangling pointers from both
    for(int i = 0; i < a.nptrs; i++)
        frag_addptr(&f, a.ptrs[i]);
    for(int i = 0; i < b.nptrs; i++)
        frag_addptr(&f, b.ptrs[i]);
    return f;
}

// Recursive descent parser for regex
const char* p; // current position in pattern

Frag parse_alt(void);
Frag parse_cat(void);
Frag parse_rep(void);
Frag parse_atom(void);

Frag parse_alt(void){
    Frag f = parse_cat();
    while(*p == '|'){
        p++;
        Frag g = parse_cat();
        f = frag_alt(f, g);
    }
    return f;
}

Frag parse_cat(void){
    // Empty concat = epsilon
    if(*p == '\0' || *p == '|' || *p == ')'){
        State* s = newstate(SPLIT, NULL, NULL);
        Frag f = mkfrag(s);
        // Both outs dangle
        frag_addptr(&f, &s->out);
        frag_addptr(&f, &s->out1);
        return f;
    }
    Frag f = parse_rep();
    while(*p != '\0' && *p != '|' && *p != ')'){
        Frag g = parse_rep();
        f = frag_cat(f, g);
    }
    return f;
}

Frag parse_rep(void){
    Frag f = parse_atom();
    if(*p == '*'){
        p++;
        State* s = newstate(SPLIT, f.start, NULL);
        frag_patch(&f, s);
        Frag r = mkfrag(s);
        frag_addptr(&r, &s->out1);
        return r;
    }
    if(*p == '+'){
        p++;
        State* s = newstate(SPLIT, f.start, NULL);
        frag_patch(&f, s);
        Frag r = mkfrag(f.start);
        frag_addptr(&r, &s->out1);
        return r;
    }
    if(*p == '?'){
        p++;
        State* s = newstate(SPLIT, f.start, NULL);
        Frag r = mkfrag(s);
        for(int i = 0; i < f.nptrs; i++)
            frag_addptr(&r, f.ptrs[i]);
        frag_addptr(&r, &s->out1);
        return r;
    }
    return f;
}

void cc_set(unsigned char* cc, int ch){
    cc[ch >> 3] |= (unsigned char)(1 << (ch & 7));
}

Frag parse_atom(void){
    if(*p == '('){
        p++;
        Frag f = parse_alt();
        if(*p == ')') p++;
        return f;
    }
    if(*p == '['){
        p++;
        State* s = newstate(CCLASS, NULL, NULL);
        if(*p == '^'){
            s->cc_neg = 1;
            p++;
        }
        int first = 1;
        while(*p && (*p != ']' || first)){
            first = 0;
            int lo = (unsigned char)*p++;
            if(*p == '-' && p[1] && p[1] != ']'){
                p++;
                int hi = (unsigned char)*p++;
                for(int c = lo; c <= hi; c++)
                    cc_set(s->cc, c);
            } else {
                cc_set(s->cc, lo);
            }
        }
        if(*p == ']') p++;
        Frag f = mkfrag(s);
        frag_addptr(&f, &s->out);
        return f;
    }
    if(*p == '^'){
        p++;
        State* s = newstate(BOL, NULL, NULL);
        Frag f = mkfrag(s);
        frag_addptr(&f, &s->out);
        return f;
    }
    if(*p == '$'){
        p++;
        State* s = newstate(EOL, NULL, NULL);
        Frag f = mkfrag(s);
        frag_addptr(&f, &s->out);
        return f;
    }
    if(*p == '.'){
        p++;
        State* s = newstate(ANY, NULL, NULL);
        Frag f = mkfrag(s);
        frag_addptr(&f, &s->out);
        return f;
    }
    if(*p == '\\' && p[1]){
        p++;
    }
    int ch = (unsigned char)*p++;
    State* s = newstate(ch, NULL, NULL);
    Frag f = mkfrag(s);
    frag_addptr(&f, &s->out);
    return f;
}

// --- NFA simulation ---

State* matchstate;
State* listbuf1[MAXSTATE];
State* listbuf2[MAXSTATE];
int listid;

typedef struct {
    State** s;
    int n;
} List;

void addstate(List* l, State* s){
    if(!s || s->lastlist == listid) return;
    s->lastlist = listid;
    if(s->op == SPLIT){
        addstate(l, s->out);
        addstate(l, s->out1);
        return;
    }
    l->s[l->n++] = s;
}

int cc_test(unsigned char* cc, int ch){
    return (cc[ch >> 3] >> (ch & 7)) & 1;
}

int nfa_match(State* start, const char* str, int len){
    List cur, nxt;
    cur.s = listbuf1;
    nxt.s = listbuf2;

    // Try match starting at each position (unanchored)
    for(int pos = 0; pos <= len; pos++){
        listid++;
        cur.n = 0;
        addstate(&cur, start);

        for(int i = pos; i <= len; i++){
            int ch = (i < len) ? (unsigned char)str[i] : -1;
            int at_bol = (i == 0);
            int at_eol = (i == len);

            // Check for match
            for(int j = 0; j < cur.n; j++)
                if(cur.s[j] == matchstate) return 1;

            if(ch == -1 && !at_eol) break;

            listid++;
            nxt.n = 0;

            for(int j = 0; j < cur.n; j++){
                State* s = cur.s[j];
                switch(s->op){
                case BOL:
                    if(at_bol) addstate(&nxt, s->out);
                    // re-add to current for further processing without consuming
                    break;
                case EOL:
                    if(at_eol) addstate(&nxt, s->out);
                    break;
                default:
                    if(ch < 0) break;
                    if(s->op == ANY){
                        if(ch != '\n') addstate(&nxt, s->out);
                    } else if(s->op == CCLASS){
                        int hit = cc_test(s->cc, ch);
                        if(s->cc_neg) hit = !hit;
                        if(hit) addstate(&nxt, s->out);
                    } else if(s->op == ch){
                        addstate(&nxt, s->out);
                    }
                    break;
                }
            }

            // Swap
            State** tmp = cur.s;
            cur.s = nxt.s;
            cur.n = nxt.n;
            nxt.s = tmp;
        }

        // Final match check
        for(int j = 0; j < cur.n; j++)
            if(cur.s[j] == matchstate) return 1;
    }
    return 0;
}

// --- Compile and run ---

// Compile pattern
p = pattern;
ptrsused = 0;
nstates = 0;
matchstate = newstate(MATCH, NULL, NULL);
Frag compiled = parse_alt();
frag_patch(&compiled, matchstate);
State* start = compiled.start;

// Read stdin line by line, print matching lines
char line[8192];
while(fgets(line, sizeof line, stdin)){
    int len = (int)strlen(line);
    if(len > 0 && line[len - 1] == '\n'){
        line[len - 1] = '\0';
        len--;
    }
    if(nfa_match(start, line, len))
        printf("%s\n", line);
}
