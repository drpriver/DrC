#include <stdio.h>
// Hex dump of input/stdin, similar to `xxd` or `hexdump -C`
unsigned char buf[16];
unsigned long offset = 0;
const char* input = __argc > 1 ? __argv[1] : NULL;
FILE* fp = input?fopen(input, "rb"):stdin;
if(!fp) return (perror(input), 1);
for(;;){
    size_t n = fread(buf, 1, 16, fp);
    if(!n) break;
    // offset
    printf("%08lx  ", offset);
    // hex bytes
    for(int i = 0; i < 16; i++){
        if(i == 8) putchar(' ');
        if(i < n)
            printf("%02x ", buf[i]);
        else
            printf("   ");
    }
    // ascii
    printf(" |");
    for(int i = 0; i < n; i++){
        unsigned char c = buf[i];
        putchar((c >= 0x20 && c <= 0x7e) ? c : '.');
    }
    printf("|\n");
    offset += (unsigned long)n;
    if(n < 16) break;
}
printf("%08lx\n", offset);
