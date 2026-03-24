#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Multithreaded Mandelbrot set renderer.
// Each thread renders a horizontal band of the image.
// Outputs a PPM image to a file (default: mandelbrot.ppm).
//   Bin/cc Samples/mandelbrot_multithreaded.c [output.ppm]

enum { W = 800, H = 600, MAX_ITER = 256, NTHREADS = __mixin(__env("NTHREADS", "8")) };

unsigned char pixels[H][W][3];

typedef struct Band Band;
struct Band {
    int y_start;
    int y_end;
    int id;
};

void hsv_to_rgb(double h, double s, double v, unsigned char* r, unsigned char* g, unsigned char* b){
    int hi = (int)(h / 60.0) % 6;
    double f = h / 60.0 - hi;
    double p = v * (1.0 - s);
    double q = v * (1.0 - f * s);
    double t = v * (1.0 - (1.0 - f) * s);
    double rr, gg, bb;
    if(hi == 0)      { rr = v; gg = t; bb = p; }
    else if(hi == 1) { rr = q; gg = v; bb = p; }
    else if(hi == 2) { rr = p; gg = v; bb = t; }
    else if(hi == 3) { rr = p; gg = q; bb = v; }
    else if(hi == 4) { rr = t; gg = p; bb = v; }
    else             { rr = v; gg = p; bb = q; }
    *r = (unsigned char)(rr * 255);
    *g = (unsigned char)(gg * 255);
    *b = (unsigned char)(bb * 255);
}

void* render_band(void* arg){
    Band* band = (Band*)arg;
    for(int y = band->y_start; y < band->y_end; y++){
        for(int x = 0; x < W; x++){
            double cr = (x - W * 0.5) * (3.5 / W) - 0.5;
            double ci = (y - H * 0.5) * (3.5 / W);
            double zr = 0, zi = 0;
            int iter = 0;
            while(zr * zr + zi * zi <= 4.0 && iter < MAX_ITER){
                double tmp = zr * zr - zi * zi + cr;
                zi = 2.0 * zr * zi + ci;
                zr = tmp;
                iter++;
            }
            if(iter == MAX_ITER){
                pixels[y][x][0] = 0;
                pixels[y][x][1] = 0;
                pixels[y][x][2] = 0;
            }
            else {
                double hue = 360.0 * iter / MAX_ITER;
                hsv_to_rgb(hue, 1.0, 1.0, &pixels[y][x][0], &pixels[y][x][1], &pixels[y][x][2]);
            }
        }
    }
    fprintf(stderr, "  thread %d done (rows %d-%d)\n", band->id, band->y_start, band->y_end - 1);
    return NULL;
}

fprintf(stderr, "Rendering %dx%d Mandelbrot with %d threads...\n", W, H, NTHREADS);

pthread_t threads[NTHREADS];
Band bands[NTHREADS];
int rows_per = H / NTHREADS;

for(int i = 0; i < NTHREADS; i++){
    bands[i].id = i;
    bands[i].y_start = i * rows_per;
    bands[i].y_end = (i == NTHREADS - 1) ? H : (i + 1) * rows_per;
    pthread_create(&threads[i], NULL, render_band, &bands[i]);
}

for(int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

// Write PPM
const char* outpath = __argc > 1 ? __argv[1] : "mandelbrot.ppm";
FILE* out = fopen(outpath, "wb");
if(!out){ perror(outpath); return 1; }
fprintf(out, "P6\n%d %d\n255\n", W, H);
for(int y = 0; y < H; y++)
    fwrite(pixels[y], 3, W, out);
fclose(out);

fprintf(stderr, "Wrote %s\n", outpath);
