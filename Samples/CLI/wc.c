#include <stdio.h>
// Word, line, and character count on stdin/input (like wc)
long lines = 0, words = 0, chars = 0;
bool in_word = 0;
const char* input = __argc > 1 ? __argv[1] : NULL;
FILE* fp = input?fopen(input, "rb"):stdin;
if(!fp) return (perror(input), 1);
for(int c; (c = fgetc(fp)) != EOF;){
    chars++;
    if(c == '\n') lines++;
    if(c == ' ' || c == '\t' || c == '\n' || c == '\r'){
        in_word = 0;
    } 
    else if(!in_word){
        in_word = 1;
        words++;
    }
}
printf("  %ld  %ld %ld\n", lines, words, chars);
