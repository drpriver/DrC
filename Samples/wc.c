#include <stdio.h>
// Word, line, and character count on stdin (like wc)
long lines = 0, words = 0, chars = 0;
bool in_word = 0;
for(int c; (c = getchar()) != EOF;){
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
