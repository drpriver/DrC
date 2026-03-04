#include <std.h>

enum {W=80};
enum {STEPS=60};

unsigned char cells[W];
// seed: single cell on the right
cells[W-1] = 1;

for(int step = 0; step < STEPS; step++){
    for(int i = 0; i < W; i++)
        putchar(cells[i] ? '#' : ' ');
    putchar('\n');
    // in-place update, saving the previous left neighbor
    unsigned char prev = 0;
    for(int i = 0; i < W; i++){
        int L = prev;
        int C = cells[i];
        int R = (i < W - 1) ? cells[i+1] : 0;
        prev = (unsigned char)C;
        cells[i] = (unsigned char)((110 >> (L * 4 + C * 2 + R)) & 1);
    }
}
