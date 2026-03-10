#include <std.h>

// ASCII Mandelbrot set renderer

enum { W = 80, H = 40 };
const char* chars = " .:-=+*#%@";
int nchars = 10;

double xmin = -2.0, xmax = 1.0;
double ymin = -1.2, ymax = 1.2;
int max_iter = 100;

for(int row = 0; row < H; row++){
    double ci = ymin + (ymax - ymin) * row / H;
    for(int col = 0; col < W; col++){
        double cr = xmin + (xmax - xmin) * col / W;
        double zr = 0.0, zi = 0.0;
        int iter = 0;
        while(zr * zr + zi * zi < 4.0 && iter < max_iter){
            double tmp = zr * zr - zi * zi + cr;
            zi = 2.0 * zr * zi + ci;
            zr = tmp;
            iter++;
        }
        int idx = iter == max_iter ? nchars - 1 : (iter * nchars / max_iter);
        putchar(chars[idx]);
    }
    putchar('\n');
}
