#include <stdio.h>
#include <string.h>
// Brainfuck interpreter
enum { MEMSZ = 30000 };
enum { MAXPROG = 65536 };
unsigned char mem[MEMSZ];
char prog[MAXPROG] = ">++++++++[<+++++++++>-]<.>++++[<+++++++>-]<+.+++++++..+++.>>++++++[<+++++++>-]<++.------------.>++++++[<+++++++++>-]<+.<.+++.------.--------.>>>++++[<++++++++>-]<+."; // prints "Hello, World!"

int proglen = 0;

// read from stdin
int c;
while((c = getchar()) != EOF && proglen < MAXPROG - 1){
    prog[proglen++] = (char)c;
}
if(!proglen) proglen = strlen(prog);
prog[proglen] = '\0';

// precompute bracket matches
int brackets[MAXPROG];
int stack[1024];
int sp = 0;
for(int i = 0; i < proglen; i++){
    if(prog[i] == '['){
        if(sp >= 1024){
            fprintf(stderr, "too many nested brackets\n");
            return 1;
        }
        stack[sp++] = i;
    }
    else if(prog[i] == ']'){
        if(sp == 0){
            fprintf(stderr, "unmatched ']' at %d\n", i);
            return 1;
        }
        int j = stack[--sp];
        brackets[i] = j;
        brackets[j] = i;
    }
}
if(sp != 0){
    fprintf(stderr, "unmatched '['\n");
    return 1;
}

// execute
int dp = 0;
for(int ip = 0; ip < proglen; ip++){
    switch(prog[ip]){
        case '>': dp++; if(dp >= MEMSZ) dp = 0; break;
        case '<': dp--; if(dp < 0) dp = MEMSZ - 1; break;
        case '+': mem[dp]++; break;
        case '-': mem[dp]--; break;
        case '.': putchar(mem[dp]); break;
        case ',': { int c = getchar(); if(c != EOF) mem[dp] = (unsigned char)c; } break;
        case '[': if(mem[dp] == 0) ip = brackets[ip]; break;
        case ']': if(mem[dp] != 0) ip = brackets[ip]; break;
        default: break;
    }
}
return 0;
