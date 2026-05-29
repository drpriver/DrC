// Uses __get/__set/__append to register commands without boilerplate.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<readline/readline.h>) || __has_include(<readline.h>)
#include <readline/readline.h> <readline.h>
#include <readline/history.h> <history.h>
#pragma lib "readline"
#pragma typedef on
enum {HAS_READLINE=1};
#else
enum {HAS_READLINE=0};
#endif

unsigned long hash(const char* s){
    unsigned long h = 5381;
    while(*s) h = h * 33 + *s++;
    return h;
}
#pragma procmacro hash // turns function into a macro, so hash computed in the cpp

struct Ctx {
    char line[1024];
    char* argv[64];
    int    argc;
    _Bool  running;
};

__set(cmd_cases, )
__set(_cmd_rows, )

// Forward-declares the handler, accumulates into dispatch switch + help table,
// then opens the function
#define CMD(name, desc)                                               \
    void cmd_##name(Ctx*);                                            \
    __append(cmd_cases, case hash(#name): cmd_##name(ctx); break;)   \
    __append(_cmd_rows, { #name, desc },)                             \
    void cmd_##name(Ctx* ctx)

CMD(help, "list all commands") {
    for(size_t i = 0; i < _Countof cmd_table; i++)
        printf("  %-12s  %s\n", cmd_table[i].name, cmd_table[i].desc);
    printf("\n");
}

CMD(quit, "exit the shell") {
    printf("bye\n");
    ctx->running = false;
}

CMD(echo, "echo arguments back") {
    for(int i = 1; i < ctx->argc; i++)
        printf("%s%s", ctx->argv[i], i+1 < ctx->argc ? " " : "");
    printf("\n");
}

CMD(add, "add numbers: add 1 2 3") {
    if(ctx->argc < 2){ printf("usage: add <num...>\n"); return; }
    double sum = 0;
    for(int i = 1; i < ctx->argc; i++)
        sum += atof(ctx->argv[i]);
    printf("%g\n", sum);
}

CMD(upper, "uppercase text: upper hello") {
    if(ctx->argc < 2){ printf("usage: upper <text...>\n"); return; }
    for(int i = 1; i < ctx->argc; i++){
        for(char* p = ctx->argv[i]; *p; p++)
            putchar(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
        if(i+1 < ctx->argc) putchar(' ');
    }
    putchar('\n');
}

CMD(rev, "reverse a string: rev hello") {
    if(ctx->argc != 2){ printf("usage: rev <text>\n"); return; }
    char* s = ctx->argv[1];
    int len = (int)strlen(s);
    for(int i = len-1; i >= 0; i--) putchar(s[i]);
    putchar('\n');
}

CMD(count, "count words: count one two three") {
    printf("%d\n", ctx->argc - 1);
}

CMD(env, "print env var: env HOME") {
    if(ctx->argc != 2){ printf("usage: env <VAR>\n"); return; }
    const char* v = getenv(ctx->argv[1]);
    printf("%s\n", v ? v : "(not set)");
}

CMD(repeat, "repeat text N times: repeat 3 hello") {
    if(ctx->argc < 3){ printf("usage: repeat <n> <text...>\n"); return; }
    int n = atoi(ctx->argv[1]);
    for(int i = 0; i < n; i++){
        for(int j = 2; j < ctx->argc; j++)
            printf("%s%s", ctx->argv[j], j+1 < ctx->argc ? " " : "");
        printf("\n");
    }
}
CMD(exit, "exit"){
    exit(0);
}

CMD(big, "embiggen: big hello") {
    if(ctx->argc < 2){ printf("usage: big <text>\n"); return; }

    static const char* G[128][5] = {
        ['A']={"  #  "," # # ","#####","#   #","#   #"},
        ['B']={"#### ","#   #","#### ","#   #","#### "},
        ['C']={" ####","#    ","#    ","#    "," ####"},
        ['D']={"#### ","#   #","#   #","#   #","#### "},
        ['E']={"#####","#    ","#### ","#    ","#####"},
        ['F']={"#####","#    ","#### ","#    ","#    "},
        ['G']={" ####","#    ","#  ##","#   #"," ### "},
        ['H']={"#   #","#   #","#####","#   #","#   #"},
        ['I']={"#####","  #  ","  #  ","  #  ","#####"},
        ['J']={"  ###","   # ","   # ","#  # "," ##  "},
        ['K']={"#   #","#  # ","###  ","#  # ","#   #"},
        ['L']={"#    ","#    ","#    ","#    ","#####"},
        ['M']={"#   #","## ##","# # #","#   #","#   #"},
        ['N']={"#   #","##  #","# # #","#  ##","#   #"},
        ['O']={" ### ","#   #","#   #","#   #"," ### "},
        ['P']={"#### ","#   #","#### ","#    ","#    "},
        ['Q']={" ### ","#   #","# # #","#  # "," ## #"},
        ['R']={"#### ","#   #","#### ","#  # ","#   #"},
        ['S']={" ####","#    "," ### ","    #","#### "},
        ['T']={"#####","  #  ","  #  ","  #  ","  #  "},
        ['U']={"#   #","#   #","#   #","#   #"," ### "},
        ['V']={"#   #","#   #","#   #"," # # ","  #  "},
        ['W']={"#   #","#   #","# # #","## ##","#   #"},
        ['X']={"#   #"," # # ","  #  "," # # ","#   #"},
        ['Y']={"#   #"," # # ","  #  ","  #  ","  #  "},
        ['Z']={"#####","   # ","  #  "," #   ","#####"},
        ['0']={" ### ","#  ##","# # #","##  #"," ### "},
        ['1']={"  #  "," ##  ","  #  ","  #  ","#####"},
        ['2']={" ### ","#   #","  ## "," #   ","#####"},
        ['3']={"#### ","    #"," ### ","    #","#### "},
        ['4']={"#   #","#   #","#####","    #","    #"},
        ['5']={"#####","#    ","#### ","    #","#### "},
        ['6']={" ### ","#    ","#### ","#   #"," ### "},
        ['7']={"#####","   # ","  #  "," #   ","#    "},
        ['8']={" ### ","#   #"," ### ","#   #"," ### "},
        ['9']={" ### ","#   #"," ####","    #"," ### "},
        ['!']={"  #  ","  #  ","  #  ","     ","  #  "},
        ['?']={" ### ","#   #","  ## ","     ","  #  "},
        [' ']={"     ","     ","     ","     ","     "},
        ['.']={"     ","     ","     ","     ","  #  "},
        [',']={"     ","     ","     ","  #  "," #   "},
    };

    for(int i = 1; i < ctx->argc; i++){
        const char* word = ctx->argv[i];
        int len = (int)strlen(word);
        for(int row = 0; row < 5; row++){
            for(int j = 0; j < len; j++){
                unsigned char c = (unsigned char)word[j];
                if(c >= 'a' && c <= 'z') c -= 32;
                const char* cell = (c < 128 && G[c][row]) ? G[c][row] : "     ";
                printf("%s ", cell);
            }
            printf("\n");
        }
        if(i+1 < ctx->argc) printf("\n");
    }
}
CMD(roll, "roll a die: roll 6"){
    static unsigned rng = (unsigned)__RAND__;
    unsigned n = 6;
    if(ctx->argc == 2)
        n = atoi(ctx->argv[1]);
    if(!rng) rng = 1;
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    unsigned roll = rng;
    roll = n>0?(roll % n)+1:0;
    printf("%u / %u\n", roll, (unsigned)n);
}

void dispatch(Ctx* ctx){
    switch((hash)(ctx->argv[0])){
        __get(cmd_cases)
        default:
            printf("unknown command '%s'  (try 'help')\n", ctx->argv[0]);
    }
}

int split(char* line, char** argv, int max) {
    int n = 0;
    char* p = line;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
        if (!*p) break;
        if (n >= max) break;
        if (*p == '"') {
            p++;
            argv[n++] = p;
            while (*p && *p != '"') p++;
            if (*p == '"') *p++ = '\0';
        }
        else {
            argv[n++] = p;
            while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            if (*p) *p++ = '\0';
        }
    }
    return n;
}

struct CmdRow { const char* name; const char* desc; };
CmdRow cmd_table[] = { __get(_cmd_rows) };
Ctx ctx = { .running = true };

printf("cmd shell  —  type 'help' to list commands\n");
while(ctx.running){
    static if(HAS_READLINE){
        char* line = readline("> ");
        if(!line) break;
        if(!line[0]) {
            free(line);
            continue;
        }
        add_history(line);
    }
    else {
        char* line = ctx.line;
        printf("> "); fflush(stdout);
        if(!fgets(ctx.line, sizeof ctx.line, stdin)) break;
        if(!line[0] || line[0] == '\n') continue;
    }
    int argc = split(line, ctx.argv, _Countof ctx.argv);
    ctx.argc = argc;
    if(argc) dispatch(&ctx);
    static if(HAS_READLINE){
        free(line);
    }
}
