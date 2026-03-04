#include <std.h>

// Hex dump of stdin, similar to `xxd` or `hexdump -C`

unsigned char buf[16];
unsigned long offset = 0;

for(;;){
    int n = (int)fread(buf, 1, 16, stdin);
    if(n <= 0) break;

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
