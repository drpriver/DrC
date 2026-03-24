// Runtime JSON parser using _Type introspection
//
// A single generic json_parse() function handles arbitrary structs,
// integers, floats, strings, booleans, and enums — all dispatched
// at runtime via _Type reflection. No code generation, no macros.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ---------------------------------------------------------------------------
// Minimal JSON tokenizer
// ---------------------------------------------------------------------------

void json_ws(const char** p){
    while(isspace((unsigned char)**p)) ++*p;
}

const char* json_string(const char** p){
    json_ws(p);
    if(**p != '"') return 0;
    ++*p;
    const char* start = *p;
    while(**p && **p != '"') ++*p;  // no escape handling for brevity
    int len = (int)(*p - start);
    char buf[256];
    memcpy(buf, start, len);
    buf[len] = 0;
    if(**p == '"') ++*p;
    return __builtin_intern(buf);
}

// Skip a JSON value we don't care about (for unknown struct fields).
void json_skip(const char** p){
    json_ws(p);
    if(**p == '"'){
        ++*p;
        while(**p && **p != '"') ++*p;
        if(**p) ++*p;
    }
    else if(**p == '{'){
        ++*p;
        int depth = 1;
        while(**p && depth){
            if(**p == '{') depth++;
            else if(**p == '}') depth--;
            else if(**p == '"'){ ++*p; while(**p && **p != '"') ++*p; }
            ++*p;
        }
    }
    else if(**p == '['){
        ++*p;
        int depth = 1;
        while(**p && depth){
            if(**p == '[') depth++;
            else if(**p == ']') depth--;
            else if(**p == '"'){ ++*p; while(**p && **p != '"') ++*p; }
            ++*p;
        }
    }
    else {
        while(**p && **p != ',' && **p != '}' && **p != ']') ++*p;
    }
}

// ---------------------------------------------------------------------------
// Generic JSON parser — dispatches on _Type at runtime
// ---------------------------------------------------------------------------

int json_parse(_Type T, const char** p, void* out);

int json_parse(_Type T, const char** p, void* out){
    json_ws(p);

    // const char*
    if(T == (const char*)){
        const char* s = json_string(p);
        if(!s) return -1;
        *(const char**)out = s;
        return 0;
    }

    // _Bool — JSON true/false
    if(T == (_Bool)){
        if(strncmp(*p, "true", 4) == 0){
            *(_Bool*)out = 1;
            *p += 4;
            return 0;
        }
        if(strncmp(*p, "false", 5) == 0){
            *(_Bool*)out = 0;
            *p += 5;
            return 0;
        }
        return -1;
    }

    // Enum — parse from JSON string, match enumerator names
    if(T.is_enum){
        const char* s = json_string(p);
        if(!s) return -1;
        for(int i = 0; i < (int)T.enumerators; i++){
            auto e = T.enumerator(i);
            if(strcmp(s, e.name) == 0){
                if(T.sizeof_ <= 4) *(int*)out = (int)e.value;
                else                *(long long*)out = e.value;
                return 0;
            }
        }
        fprintf(stderr, "json: unknown enumerator '%s' for %s\n", s, T.name);
        return -1;
    }

    // Integers
    if(T.is_integer){
        char* end;
        long long v = strtoll(*p, &end, 10);
        if(end == *p) return -1;
        *p = end;
        if(T.sizeof_ == 1)      *(char*)out = (char)v;
        else if(T.sizeof_ == 2) *(short*)out = (short)v;
        else if(T.sizeof_ == 4) *(int*)out = (int)v;
        else                     *(long long*)out = v;
        return 0;
    }

    // Floats
    if(T.is_float){
        char* end;
        double v = strtod(*p, &end);
        if(end == *p) return -1;
        *p = end;
        if(T.sizeof_ == 4) *(float*)out = (float)v;
        else                *(double*)out = v;
        return 0;
    }

    // Struct — parse JSON object, match keys to field names
    if(T.is_struct){
        if(**p != '{') return -1;
        ++*p;
        memset(out, 0, T.sizeof_);
        int first = 1;
        for(;;){
            json_ws(p);
            if(**p == '}') { ++*p; return 0; }
            if(!first){
                if(**p != ',') return -1;
                ++*p;
            }
            first = 0;
            const char* key = json_string(p);
            if(!key) return -1;
            json_ws(p);
            if(**p != ':') return -1;
            ++*p;
            // Find matching field
            _Bool found = 0;
            for(int i = 0; i < (int)T.fields; i++){
                auto f = T.field(i);
                if(strcmp(key, f.name) == 0){
                    int err = json_parse(f.type, p, (char*)out + f.offset);
                    if(err) return err;
                    found = 1;
                    break;
                }
            }
            if(!found) json_skip(p); // ignore unknown fields
        }
    }

    fprintf(stderr, "json: unsupported type '%s'\n", T.name);
    return -1;
}

// ---------------------------------------------------------------------------
// Generic JSON writer — dispatches on _Type at runtime
// ---------------------------------------------------------------------------

void json_write(_Type T, FILE* f, const void* data, int indent);

void json_indent(FILE* f, int indent){
    for(int i = 0; i < indent; i++) fprintf(f, "  ");
}

void json_write(_Type T, FILE* f, const void* data, int indent){
    // const char*
    if(T == (const char*)){
        const char* s = *(const char**)data;
        if(!s){ fprintf(f, "null"); return; }
        fputc('"', f);
        for(const char* c = s; *c; c++){
            if(*c == '"') fprintf(f, "\\\"");
            else if(*c == '\\') fprintf(f, "\\\\");
            else if(*c == '\n') fprintf(f, "\\n");
            else fputc(*c, f);
        }
        fputc('"', f);
        return;
    }

    // _Bool
    if(T == (_Bool)){
        fprintf(f, *(_Bool*)data ? "true" : "false");
        return;
    }

    // Enum — write as string using enumerator name
    if(T.is_enum){
        long long v;
        if(T.sizeof_ <= 4) v = *(int*)data;
        else                v = *(long long*)data;
        for(int i = 0; i < (int)T.enumerators; i++){
            auto e = T.enumerator(i);
            if(e.value == v){
                fprintf(f, "\"%s\"", e.name);
                return;
            }
        }
        fprintf(f, "%lld", v); // fallback: unknown enumerator
        return;
    }

    // Integers
    if(T.is_integer){
        long long v;
        if(T.sizeof_ == 1)      v = *(char*)data;
        else if(T.sizeof_ == 2) v = *(short*)data;
        else if(T.sizeof_ == 4) v = *(int*)data;
        else                     v = *(long long*)data;
        fprintf(f, "%lld", v);
        return;
    }

    // Floats
    if(T.is_float){
        double v;
        if(T.sizeof_ == 4) v = *(float*)data;
        else                v = *(double*)data;
        fprintf(f, "%g", v);
        return;
    }

    // Struct — write as JSON object
    if(T.is_struct){
        fprintf(f, "{\n");
        int n = (int)T.fields;
        for(int i = 0; i < n; i++){
            auto field = T.field(i);
            json_indent(f, indent + 1);
            fprintf(f, "\"%s\": ", field.name);
            json_write(field.type, f, (const char*)data + field.offset, indent + 1);
            if(i + 1 < n) fputc(',', f);
            fputc('\n', f);
        }
        json_indent(f, indent);
        fputc('}', f);
        return;
    }

    fprintf(f, "null");
}

// ---------------------------------------------------------------------------
// Demo
// ---------------------------------------------------------------------------

typedef enum Role Role;
enum Role { WARRIOR, MAGE, ROGUE, HEALER };

typedef struct Vec2 Vec2;
struct Vec2 { double x, y; };

typedef struct Player Player;
struct Player {
    const char* name;
    int hp;
    int level;
    _Bool alive;
    Role role;
    Vec2 pos;
};

void print_player(const Player* p){
    const char* roles[] = {"WARRIOR","MAGE","ROGUE","HEALER"};
    printf("  name:  %s\n", p->name);
    printf("  hp:    %d\n", p->hp);
    printf("  level: %d\n", p->level);
    printf("  alive: %s\n", p->alive ? "true" : "false");
    printf("  role:  %s\n", roles[p->role]);
    printf("  pos:   (%.1f, %.1f)\n", p->pos.x, p->pos.y);
}

// Parse a single player
const char* json1 =
    "{"
    "  \"name\": \"Alice\","
    "  \"hp\": 100,"
    "  \"level\": 42,"
    "  \"alive\": true,"
    "  \"role\": \"MAGE\","
    "  \"pos\": { \"x\": 3.5, \"y\": -7.2 }"
    "}";

printf("--- Parsing Player from JSON ---\n");
Player p1;
const char* cursor = json1;
int err = json_parse(typeof(p1), &cursor, &p1);
if(err){
    printf("parse error!\n");
    return 1;
}
print_player(&p1);

// Parse another with different values + unknown field (ignored)
const char* json2 =
    "{"
    "  \"name\": \"Bob\","
    "  \"hp\": 55,"
    "  \"level\": 7,"
    "  \"alive\": false,"
    "  \"role\": \"ROGUE\","
    "  \"guild\": \"Shadows\","
    "  \"pos\": { \"x\": 0.0, \"y\": 100.0 }"
    "}";

printf("\n--- Parsing Player with unknown field ---\n");
Player p2;
cursor = json2;
err = json_parse(typeof(p2), &cursor, &p2);
if(err){
    printf("parse error!\n");
    return 1;
}
print_player(&p2);

// Parse just a nested struct directly
const char* json3 = "{ \"x\": 42.0, \"y\": -1.5 }";
printf("\n--- Parsing Vec2 directly ---\n");
Vec2 v;
cursor = json3;
err = json_parse(typeof(v), &cursor, &v);
if(err){
    printf("parse error!\n");
    return 1;
}
printf("  (%.1f, %.1f)\n", v.x, v.y);

// Write JSON back out
printf("\n--- Writing Player as JSON ---\n");
json_write(typeof(p1), stdout, &p1, 0);
printf("\n");

printf("\n--- Round-trip: parse then write ---\n");
json_write(typeof(p2), stdout, &p2, 0);
printf("\n");
