#include "json_parse.h"

#pragma typedef on
enum Role { WARRIOR, MAGE, ROGUE, HEALER };

typedef double Vec2[2];

struct Player {
    const char* name;
    int hp;
    int level;
    _Bool alive;
    Role role;
    Vec2 pos;
};

#define print(p) json_write(typeof(*p), stdout, p);
#define parse(p, json) json_parse(typeof(*p), json, p)

printf("--- Parsing Player from JSON ---\n");
Player p1;
parse(&p1,
    "{"
    "  \"name\": \"Alice\","
    "  \"hp\": 100,"
    "  \"level\": 42,"
    "  \"alive\": true,"
    "  \"role\": \"MAGE\","
    "  \"pos\": [ 3.5,  -7.2 ]"
    "}");
print(&p1);

printf("\n--- Parsing Player with unknown field ---\n");
Player p2;
parse(&p2,
    "{"
    "  \"name\": \"Bob\","
    "  \"hp\": 55,"
    "  \"level\": 7,"
    "  \"alive\": false,"
    "  \"role\": \"ROGUE\","
    "  \"guild\": \"Shadows\","
    "  \"pos\": [ 0.0, 100.0 ]"
    "}");
print(&p2);

printf("\n--- Parsing Vec2 directly ---\n");
Vec2 v;
parse(&v, "[ 42.0, -1.5 ]");
print(&v);

printf("\n--- Inline type printing ---\n");
json_write(struct Foo, stdout, &(struct Foo{int x, y;}){1, 2});
printf("\n");
