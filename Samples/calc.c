#include <std.h>

printf("=== RPN Calculator ===\n");
printf("Enter numbers and operators (+, -, *, /)\n");
printf("Commands: p (print top), d (dump stack), c (clear), q (quit)\n\n");

double stack[64];
int sp = 0;
char line[256];
int quit = 0;
for(;!quit;){
    printf("> ");
    if(!fgets(line, 256, stdin)) break;
    // strip newline
    unsigned long len = strlen(line);
    if(len > 0 && line[len-1] == '\n') line[len-1] = '\0';
    // tokenize by spaces
    char* pos = line;
    while(*pos){
        // skip whitespace
        while(*pos == ' ' || *pos == '\t') pos++;
        if(!*pos) break;
        // find end of token
        char* start = pos;
        while(*pos && *pos != ' ' && *pos != '\t') pos++;
        char saved = *pos;
        *pos = '\0';
        // skip empty
        if(start[0] == '\0') continue;
        // quit
        if(start[0] == 'q' && start[1] == '\0'){ quit = 1; break; }
        // print top
        if(start[0] == 'p' && start[1] == '\0'){
            if(sp == 0)
                printf("  (empty)\n");
            else
                printf("  top: %g\n", stack[sp-1]);
            goto next;
        }
        // dump stack
        if(start[0] == 'd' && start[1] == '\0'){
            if(sp == 0){
                printf("  (empty)\n");
                goto next;
            }
            for(int i = sp - 1; i >= 0; i--)
                printf("  [%d] %g\n", i, stack[i]);
            goto next;
        }
        // clear
        if(start[0] == 'c' && start[1] == '\0'){
            sp = 0;
            printf("  cleared\n");
            goto next;
        }
        // operator?
        if((start[0] == '+' || start[0] == '-' || start[0] == '*' || start[0] == '/') && start[1] == '\0'){
            if(sp < 2){
                printf("  error: need 2 operands\n");
                goto next;
            }
            double b = stack[--sp];
            double a = stack[--sp];
            double r;
            switch(start[0]){
                case '+': r = a + b; break;
                case '-': r = a - b; break;
                case '*': r = a * b; break;
                case '/':
                    if(b == 0.0){ printf("  error: division by zero\n"); sp += 2; goto next; }
                    r = a / b;
                    break;
                default: r = 0; break;
            }
            stack[sp++] = r;
            printf("  = %g\n", r);
            goto next;
        }
        // try to parse as number
        double val;
        if(sscanf(start, "%lf", &val) == 1){
            if(sp >= 64){
                printf("  error: stack full\n");
                goto next;
            }
            stack[sp++] = val;
            goto next;
        }
        printf("  unknown: %s\n", start);
        next:
        *pos = saved;
    }
}
printf("bye\n");
