//usr/bin/env drc "$0" "$@"; exit
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if __has_include(<readline/readline.h>) || __has_include(<readline.h>)
#include <readline/readline.h> <readline.h>
#include <readline/history.h> <history.h>
#pragma lib "readline"
#else
char* readline(const char* prompt){
    printf("%s", prompt);
    char buff[1024];
    char* p = fgets(buff, sizeof buff, stdin);
    if(!p) return NULL;
    size_t len = strlen(p);
    if(len && buff[len-1] == '\n') buff[--len] = 0;
    p = malloc(len + 1);
    if(!p) return NULL;
    memcpy(p, buff, len);
    p[len] = 0;
    return p;
}
void add_history(const char*){}
#endif
int math(int x, int y){ return 0; }
int add(int x, int y){return x+y;}
int sub(int x, int y){return x-y;}
int mul(int x, int y){return x*y;}
int print(int x, int y){printf("x=%d, y=%d\n", x, y); return 0;}
int x = 3;
int y = 2;
char *line;
for(;;){
    free(line);
    line = readline("> ");
    if(!line) break;
    if(!line[0]) continue;
    add_history(line);
    {
        typeof(&math) sym = __symbol(NULL, line, typeof(*sym));
        if(sym){
            __hotswap(math, sym);
            printf("math(%d, %d) = %d\n", x, y, math(x,y));
            continue;
        }
    }
    if(strcmp(line, "xor")==0){
        __hotswap(math, int(int x, int y){
            return x ^ y;
        });
            printf("math(%d, %d) = %d\n", x, y, math(x,y));
            continue;
    }
    int* p = __symbol(NULL, line, int);
    if(p) {
        ++*p;
        printf("++%s -> %d\n", line, *p);
    }
}
