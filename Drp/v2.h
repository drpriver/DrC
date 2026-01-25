//
// Copyright © 2024-2025, David Priver <david@davidpriver.com>
//
#ifndef V2_H
#define V2_H
#ifndef SER
#define SER(...)
#endif
#if defined(PLATFORM_SDL) && __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#define HAVE_SDL_FRECT
#elif defined(PLATFORM_SDL) && __has_include(<SDL.h>)
#include <SDL.h>
#define HAVE_SDL_FRECT
#else
#endif
#if defined(__clang__)
#define V2ATTRS __attribute__((nodebug, always_inline))
#elif defined(__GNUC__)
#define V2ATTRS __attribute__((always_inline))
#elif defined(_MSC_VER)
#define V2ATTRS __forceinline
#else
#define V2ATTRS
#endif

static inline
V2ATTRS
float
lerp(float v0, float v1, float t) {
    return v0 + t * (v1 - v0);
}
typedef struct v2i v2i;
struct v2i {
    SER(tuple, null=v2i_NONE, is_null=v2i_is_null)
    // Custom serialization in types.c for v2i_NONE handling
    SER() _Alignas(unsigned long long) int x;
    SER() int y;
};
_Static_assert(sizeof(v2i) == sizeof(unsigned long long), "");
_Static_assert(sizeof(v2i) == 2*sizeof(int), "");
#define v2i_NONE ((v2i){INT32_MIN, INT32_MIN})

static inline
V2ATTRS
_Bool
v2i_eq(v2i v0, v2i v1){
    return v0.x == v1.x && v0.y == v1.y;
}

static inline
V2ATTRS
_Bool
v2i_is_null(v2i v){
    return v2i_eq(v, v2i_NONE);
}


static inline
V2ATTRS
v2i
v2i_lerp(v2i v0, v2i v1, float t) {
    return (v2i){v0.x + t * (v1.x - v0.x), v0.y + t * (v1.y - v0.y)};
}

static inline int abs_(int a){ return a < 0?-a:a; }

static inline
V2ATTRS
int
v2i_l1norm(v2i v){
    return abs_(v.x) + abs_(v.y);
}

static inline
V2ATTRS
int
vmax(int a, int b);

static inline
V2ATTRS
int
v2i_4enorm(v2i v){
    return vmax(abs_(v.x),abs_(v.y));
}


static inline
V2ATTRS
v2i
v2i_add(v2i a, v2i b){
    return (v2i){a.x+b.x, a.y+b.y};
}
static inline
V2ATTRS
v2i
v2i_add2(v2i a, int x, int y){
    return (v2i){a.x+x, a.y+y};
}
static inline
V2ATTRS
v2i
v2i_add_i(v2i a, int b){
    return (v2i){a.x+b, a.y+b};
}

static inline
V2ATTRS
v2i
v2i_sub(v2i a, v2i b){
    return (v2i){a.x-b.x, a.y-b.y};
}

static inline
V2ATTRS
int
v2i_l1dist(v2i v0, v2i v1){
    return v2i_l1norm(v2i_sub(v0, v1));
}

static inline
V2ATTRS
int
v2i_4edist(v2i v0, v2i v1){
    return v2i_4enorm(v2i_sub(v0, v1));
}

static inline
V2ATTRS
v2i
v2i_sub_i(v2i a, int b){
    return (v2i){a.x-b, a.y-b};
}

static inline
V2ATTRS
v2i
v2i_mul(v2i a, v2i b){
    return (v2i){a.x*b.x, a.y*b.y};
}
static inline
V2ATTRS
v2i
v2i_mul_i(v2i a, int b){
    return (v2i){a.x*b, a.y*b};
}

static inline
V2ATTRS
v2i
v2i_div(v2i a, v2i b){
    return (v2i){a.x/b.x, a.y/b.y};
}

static inline
V2ATTRS
v2i
v2i_div_i(v2i a, int b){
    return (v2i){a.x/b, a.y/b};
}

static inline
V2ATTRS
int
vmin(int a, int b){
    return a < b? a : b;
}

static inline
V2ATTRS
int
vmax(int a, int b){
    return a < b? b : a;
}

static inline
V2ATTRS
v2i
v2i_vmin(v2i a, v2i b){
    return (v2i){vmin(a.x,b.x), vmin(a.y, b.y)};
}

static inline
V2ATTRS
v2i
v2i_imin(v2i a, int i){
    return (v2i){vmin(a.x,i), vmin(a.y, i)};
}

static inline
V2ATTRS
v2i
v2i_vmax(v2i a, v2i b){
    return (v2i){vmax(a.x, b.x), vmax(a.y, b.y)};
}

static inline
V2ATTRS
v2i
v2i_imax(v2i a, int i){
    return (v2i){vmax(a.x, i), vmax(a.y, i)};
}

typedef struct v2f v2f;
struct v2f {
    // NOTE: _Alignas only on first one so they don't both get aligned
    SER(tuple) 
    SER() _Alignas(unsigned long long) float x;
    SER() float y;

};
_Static_assert(sizeof(v2f)==sizeof(unsigned long long), "");

typedef struct rectf rectf;
struct rectf { 
    SER(tuple) 
    SER() float x, y, w, h;
};
_Static_assert(sizeof(rectf)==sizeof(v2f)*2, "");
#ifdef HAVE_SDL_FRECT
_Static_assert(sizeof(rectf)==sizeof(SDL_FRect), "");
#endif

static inline
V2ATTRS
_Bool
v2f_eq(v2f v0, v2f v1){
    return v0.x == v1.x && v0.y == v1.y;
}

static inline
V2ATTRS
v2f
v2f_lerp(v2f v0, v2f v1, float t) {
    return (v2f){v0.x + t * (v1.x - v0.x), v0.y + t * (v1.y - v0.y)};
}
static inline
V2ATTRS
v2f
v2f_add(v2f a, v2f b){
    return (v2f){a.x+b.x, a.y+b.y};
}

static inline
V2ATTRS
v2f
v2f_add_f(v2f a, float b){
    return (v2f){a.x+b, a.y+b};
}

static inline
V2ATTRS
v2f
v2f_sub(v2f a, v2f b){
    return (v2f){a.x-b.x, a.y-b.y};
}

static inline
V2ATTRS
v2f
v2f_sub_f(v2f a, float b){
    return (v2f){a.x-b, a.y-b};
}

static inline
V2ATTRS
v2f
v2f_mul(v2f a, v2f b){
    return (v2f){a.x*b.x, a.y*b.y};
}
static inline
V2ATTRS
v2f
v2f_mul_f(v2f a, float b){
    return (v2f){a.x*b, a.y*b};
}

static inline
V2ATTRS
v2f
v2f_div(v2f a, v2f b){
    return (v2f){a.x/b.x, a.y/b.y};
}

static inline
V2ATTRS
v2f
v2f_div_f(v2f a, float b){
    return (v2f){a.x/b, a.y/b};
}

static inline
V2ATTRS
float
vminf(float a, float b){
    return a < b? a : b;
}

static inline
V2ATTRS
float
vmaxf(float a, float b){
    return a < b? b : a;
}

static inline
V2ATTRS
v2f
v2f_vmin(v2f a, v2f b){
    return (v2f){vminf(a.x,b.x), vminf(a.y, b.y)};
}

static inline
V2ATTRS
v2f
v2f_vmax(v2f a, v2f b){
    return (v2f){vmaxf(a.x, b.x), vmaxf(a.y, b.y)};
}

static inline
V2ATTRS
v2f
v2f_clamp(v2f val, v2f lo, v2f hi){
    return (v2f){vmaxf(lo.x, vminf(hi.x, val.x)), vmaxf(lo.y, vminf(hi.y, val.y))};
}

typedef struct v2d v2d;
struct v2d {
    double x, y;
};

static inline
V2ATTRS
_Bool
v2d_eq(v2d v0, v2d v1){
    return v0.x == v1.x && v0.y == v1.y;
}

static inline
V2ATTRS
v2d
v2d_lerp(v2d v0, v2d v1, double t) {
    return (v2d){v0.x + t * (v1.x - v0.x), v0.y + t * (v1.y - v0.y)};
}
static inline
V2ATTRS
v2d
v2d_add(v2d a, v2d b){
    return (v2d){a.x+b.x, a.y+b.y};
}
static inline
V2ATTRS
v2d
v2d_add_d(v2d a, double b){
    return (v2d){a.x+b, a.y+b};
}

static inline
V2ATTRS
v2d
v2d_sub(v2d a, v2d b){
    return (v2d){a.x-b.x, a.y-b.y};
}

static inline
V2ATTRS
v2d
v2d_sub_d(v2d a, double b){
    return (v2d){a.x-b, a.y-b};
}

static inline
V2ATTRS
v2d
v2d_mul(v2d a, v2d b){
    return (v2d){a.x*b.x, a.y*b.y};
}
static inline
V2ATTRS
v2d
v2d_mul_d(v2d a, double b){
    return (v2d){a.x*b, a.y*b};
}

static inline
V2ATTRS
v2d
v2d_div(v2d a, v2d b){
    return (v2d){a.x/b.x, a.y/b.y};
}

static inline
V2ATTRS
v2d
v2d_div_d(v2d a, double b){
    return (v2d){a.x/b, a.y/b};
}

static inline
V2ATTRS
double
vmind(double a, double b){
    return a < b? a : b;
}

static inline
V2ATTRS
double
vmaxd(double a, double b){
    return a < b? b : a;
}

static inline
V2ATTRS
v2d
v2d_vmin(v2d a, v2d b){
    return (v2d){vmind(a.x,b.x), vmind(a.y, b.y)};
}

static inline
V2ATTRS
v2d
v2d_vmax(v2d a, v2d b){
    return (v2d){vmaxd(a.x, b.x), vmaxd(a.y, b.y)};
}

static inline
V2ATTRS
rectf
r_add_v2(rectf a, v2f b){
    return (rectf){a.x+b.x, a.y+b.y, a.w, a.h};
}

static inline
V2ATTRS
rectf
r_add_d(rectf a, float b){
    return (rectf){a.x+b, a.y+b, a.w, a.h};
}

static inline
V2ATTRS
rectf
r_sub_v2(rectf a, v2f b){
    return (rectf){a.x-b.x, a.y-b.y, a.w, a.h};
}

static inline
V2ATTRS
rectf
r_sub_f(rectf a, float b){
    return (rectf){a.x-b, a.y-b, a.w, a.h};
}

static inline
V2ATTRS
rectf
r_mul_v2(rectf a, v2f b){
    return (rectf){a.x*b.x, a.y*b.y, a.w*b.x, a.h*b.y};
}
static inline
V2ATTRS
rectf
r_mul_f(rectf a, float b){
    return (rectf){a.x*b, a.y*b, a.w*b, a.h*b};
}

static inline
V2ATTRS
rectf
r_div_v2(rectf a, v2f b){
    return (rectf){a.x/b.x, a.y/b.y, a.w/b.x, a.h/b.y};
}

static inline
V2ATTRS
rectf
r_div_f(rectf a, float b){
    return (rectf){a.x/b, a.y/b, a.w/b, a.h/b};
}

static inline
V2ATTRS
rectf
r_expand_f(rectf a, float b){
    v2f center = {a.x+a.w/2.f, a.y+a.h/2.f};
    return (rectf){center.x-a.w*b/2, center.y-a.h*b/2, a.w*b, a.h*b};
}
static inline
V2ATTRS
rectf
r_expand_v2(rectf a, v2f b){
    v2f center = {a.x+a.w/2.f, a.y+a.h/2.f};
    return (rectf){center.x-a.w*b.x/2, center.y-a.h*b.y/2, a.w*b.x, a.h*b.y};
}

static inline
V2ATTRS
rectf
r_center(rectf a){
    return (rectf){a.x-a.w/2.f, a.y-a.h/2.f, a.w, a.h};
}
static inline
V2ATTRS
rectf
r_centerx(rectf a){
    return (rectf){a.x-a.w/2.f, a.y, a.w, a.h};
}
static inline
V2ATTRS
rectf
r_centery(rectf a){
    return (rectf){a.x, a.y-a.h/2.f, a.w, a.h};
}

static inline
V2ATTRS
_Bool
point_in_rect(rectf r, v2f point){
    return (point.x >= r.x)
        & (point.y >= r.y)
        & (point.x < r.x+r.w)
        & (point.y < r.y+r.h);
}

typedef struct recti recti;
struct recti {
    SER(tuple)
    union {
        v2i tl;
        struct {
            SER() int x, y;
        };
    };
    union {
        v2i wh;
        struct {
            SER() int w, h;
        };
    };
};

static inline
V2ATTRS
_Bool
point_in_recti(recti r, v2i point){
    return (point.x >= r.x)
        & (point.y >= r.y)
        & (point.x < r.x+r.w)
        & (point.y < r.y+r.h);
}

static inline
V2ATTRS
_Bool
recti_overlaps(recti rect1, recti rect2){
    int x = vmax(rect1.x, rect2.x);
    int y = vmax(rect1.y, rect2.y);
    int w = vmax(vmin(rect1.x + rect1.w, rect2.x + rect2.w) - x, 0);
    int h = vmax(vmin(rect1.y + rect1.h, rect2.y + rect2.h) - y, 0);
    return w && h;
}

static inline
V2ATTRS
v2i
recti_br(recti r){
    return v2i_add(r.tl, r.wh);
}


static inline
V2ATTRS
int
recti_recti_l1dist(recti A, recti B){
    // https://stackoverflow.com/questions/65107289/minimum-distance-between-two-axis-aligned-boxes-in-n-dimensions
    // Adapted to use l1 distance instead of l2 distance
    v2i A0 = A.tl;
    v2i A1 = v2i_add(A.tl, (v2i){A.w, A.h});
    v2i B0 = B.tl;
    v2i B1 = v2i_add(B.tl, (v2i){B.w, B.h});
    v2i u = v2i_imax(v2i_sub(A0, B1), 0);
    v2i v = v2i_imax(v2i_sub(B0, A1), 0);
    return v2i_l1norm(u) + v2i_l1norm(v);
}

static inline
V2ATTRS
int
recti_recti_4edist(recti A, recti B){
    // Adapted to use l1 distance instead of l2 distance
    v2i A0 = A.tl;
    v2i A1 = v2i_add(A.tl, (v2i){A.w, A.h});
    v2i B0 = B.tl;
    v2i B1 = v2i_add(B.tl, (v2i){B.w, B.h});
    v2i u = v2i_imax(v2i_sub(A0, B1), 0);
    v2i v = v2i_imax(v2i_sub(B0, A1), 0);
    return vmax(v2i_4enorm(u), v2i_4enorm(v));
}

static inline
V2ATTRS
int
recti_v2i_l1dist(recti r, v2i v){
    return recti_recti_l1dist(r, (recti){.tl=v, .w=1, .h=1});
}

static inline
V2ATTRS
int
recti_v2i_4edist(recti r, v2i v){
    return recti_recti_4edist(r, (recti){.tl=v, .w=1, .h=1});
}

static inline
V2ATTRS
_Bool
rectf_eq(rectf a, rectf b){
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

// Manhattan distance between rects for cardinal movement.
// Returns 0 if edge-adjacent, 1 if corner-adjacent,
// otherwise the number of cardinal moves to become edge-adjacent.
static
int
recti_manhattan_dist(recti a, recti b){
    int ax1 = a.tl.x + a.w, ay1 = a.tl.y + a.h;
    int bx1 = b.tl.x + b.w, by1 = b.tl.y + b.h;
    // Gap: positive = separated, zero = touching, negative = overlapping
    int gap_x = vmax(a.tl.x, b.tl.x) - vmin(ax1, bx1);
    int gap_y = vmax(a.tl.y, b.tl.y) - vmin(ay1, by1);
    int dist = vmax(0, gap_x) + vmax(0, gap_y);
    // If no overlap on either axis, need +1 to become edge-adjacent
    if(gap_x >= 0 && gap_y >= 0)
        return dist + 1;
    return dist;
}



#endif
