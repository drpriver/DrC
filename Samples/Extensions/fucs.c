// Function Uniform Call Syntax (FUCS)
//
// x.foo(args) -> foo(x, args)

#include <stdio.h>

// Vector math

typedef struct v2f v2f;
struct v2f {
    union {
        struct {float x, y;};
        float v[2];
    };
};

v2f add(v2f a, v2f b){
    return (v2f){a.x+b.x, a.y+b.y};
}
v2f scale(v2f a, float s){
    return (v2f){a.x*s, a.y*s};
}
float dot(v2f a, v2f b){
    return a.x*b.x + a.y*b.y;
}
float mag2(v2f a){
    return a.dot(a);
}

void zero(v2f* v){
    v->x = 0;
    v->y = 0;
}

printf(" basic FUCS \n");
printf(" ---------- \n");
v2f a = {3, 4}, b = {1, 2};

printf("a.add(b)    = {%.0f, %.0f}\n", a.add(b).x, a.add(b).y);
printf("a.dot(b)    = %.0f\n", a.dot(b));
printf("a.mag2()    = %.0f\n", a.mag2());
printf("a.scale(2)  = {%.0f, %.0f}\n", a.scale(2).x, a.scale(2).y);

// auto-&
a.zero();
printf("a.zero()    -> {%.0f, %.0f}\n", a.x, a.y);

// auto-*
v2f c = {5, 12};
v2f* p = &c;
printf("p.mag2()    = %.0f\n", p.mag2());
printf("p.dot(b)    = %.0f\n", p.dot(b));

printf("\n ---------- \n\n");

typedef struct Player Player;
struct Player {
    const char* name;
    int hp;
    v2f pos, velocity, acc;
};

void update(Player* p){
    p.pos = p.pos.add(p.velocity);
    p.velocity = p.velocity.add(p.acc);
    p.acc = p.acc.scale(0.9);
}

void display(Player* p){
    printf("%4s: %2dhp %7.3f,%7.3f\n", p.name, p.hp, p.pos.x, p.pos.y);
}

Player players[] = {
   {"jon",  10,  0,  0,  3,  3,  1, 1},
   {"bill", 20, 20, 20, -3, -3,  1, 1},
};

for(int i = 0; i < 5; i++){
    for(__SIZE_TYPE__ j = 0; j < _Countof(players); j++){
        players[j].update();
        players[j].display();
    }
}
