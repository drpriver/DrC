#include <std.h>

// RPN calculator: Bin/drc Samples/Simple/calc.c "3 4 + 2 * 10 3 - /"

double stack[64];
int sp = 0;
const char* expr = __argc > 1 ? __argv[1] : "3 4 + 2 * 10 3 - /";
char tok[64];
for(int n; sscanf(expr, "%63s%n", tok, &n) == 1; expr += n){
    if(sp >= 2 && tok[1] == '\0' && (*tok=='+' || *tok=='-' || *tok=='*' || *tok=='/')){
        double b = stack[--sp], a = stack[--sp];
        switch(*tok){
            case '+': stack[sp++] = a + b; break;
            case '-': stack[sp++] = a - b; break;
            case '*': stack[sp++] = a * b; break;
            case '/': stack[sp++] = a / b; break;
        }
    }
    else stack[sp++] = atof(tok);
}
printf("%g\n", stack[0]);
