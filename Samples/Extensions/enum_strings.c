// Enum-to-string and string-to-enum via _Type.enumerator
//
// Uses enum introspection to auto-generate:
//   - to_string: enum value -> const char*
//   - from_string: case-insensitive const char* -> enum value
//
// Works for any enum without writing boilerplate per type.

#include <stdio.h>
#include <string.h>
#include <ctype.h>

int strieq(const char* a, const char* b){
    for(; *a && *b; a++, b++)
        if(tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

const char* gen_enum_strings(_Type T){
    if(!T.is_enum) return "";
    int count = (int)T.enumerators;
    char buf[8192];
    int off = 0;

    // const char* <Name>_to_string(<Name> v)
    off += snprintf(buf+off, sizeof buf-off,
        "const char* %s_to_string(%s v){\n"
        "    switch(v){\n",
        T.tag, T.name);
    for(int i = 0; i < count; i++){
        auto e = T.enumerator(i);
        off += snprintf(buf+off, sizeof buf-off,
            "        case %lld: return \"%s\";\n",
            e.value, e.name);
    }
    off += snprintf(buf+off, sizeof buf-off,
        "    }\n"
        "    return \"(unknown)\";\n"
        "}\n");

    // _Bool <Name>_from_string(const char* s, <Name>* out)
    off += snprintf(buf+off, sizeof buf-off,
        "_Bool %s_from_string(const char* s, %s* out){\n",
        T.tag, T.name);
    for(int i = 0; i < count; i++){
        auto e = T.enumerator(i);
        off += snprintf(buf+off, sizeof buf-off,
            "    if(strieq(s, \"%s\")){ *out = %lld; return 1; }\n",
            e.name, e.value);
    }
    off += snprintf(buf+off, sizeof buf-off,
        "    return 0;\n"
        "}\n");

    return __builtin_intern(buf);
}
#pragma procmacro gen_enum_strings

// --- Usage ---

typedef enum Color Color;
enum Color { RED, GREEN, BLUE };
__mixin(gen_enum_strings(enum Color));

typedef enum Direction Direction;
enum Direction { NORTH = 1, SOUTH, EAST, WEST };
__mixin(gen_enum_strings(enum Direction));

// to_string
printf("--- to_string ---\n");
printf("RED   = %s\n", Color_to_string(RED));
printf("GREEN = %s\n", Color_to_string(GREEN));
printf("BLUE  = %s\n", Color_to_string(BLUE));
printf("WEST  = %s\n", Direction_to_string(WEST));
printf("99    = %s\n", Color_to_string(99));

// from_string (case-insensitive)
printf("\n--- from_string ---\n");
Color c;
if(Color_from_string("red", &c))
    printf("\"red\"   -> %s (%d)\n", Color_to_string(c), c);
if(Color_from_string("Blue", &c))
    printf("\"Blue\"  -> %s (%d)\n", Color_to_string(c), c);
if(Color_from_string("GREEN", &c))
    printf("\"GREEN\" -> %s (%d)\n", Color_to_string(c), c);
if(!Color_from_string("yellow", &c))
    printf("\"yellow\" -> not found\n");

Direction d;
if(Direction_from_string("north", &d))
    printf("\"north\" -> %s (%d)\n", Direction_to_string(d), d);
if(Direction_from_string("EAST", &d))
    printf("\"EAST\"  -> %s (%d)\n", Direction_to_string(d), d);
