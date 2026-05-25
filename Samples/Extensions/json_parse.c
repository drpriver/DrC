// Runtime JSON parser using _Type introspection

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int json_parse(_Type T, const char** p, void* out);
int json_write(_Type T, FILE* f, const void* data, int indent);

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

// skip a json value
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

int json_parse(_Type T, const char** p, void* out){
    json_ws(p);
    if(T == const char*){
        const char* s = json_string(p);
        if(!s) return -1;
        *(const char**)out = s;
        return 0;
    }
    if(T == _Bool){
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
    if(T.is_enum){
        const char* s = json_string(p);
        if(!s) return -1;
        for(size_t i = 0; i < T.enumerators; i++){
            auto e = T.enumerator(i);
            if(strcmp(s, e.name) == 0){
                memcpy(out, &i, T.sizeof_);
                return 0;
            }
        }
        fprintf(stderr, "json: unknown enumerator '%s' for %s\n", s, T.name);
        return -1;
    }
    if(T.is_integer){
        char* end;
        if(T.is_unsigned){
            if(T.sizeof_ > 8) return 1;
            unsigned long long v = strtoull(*p, &end, 10);
            if(end == *p) return -1;
            *p = end;
            memcpy(out, &v, T.sizeof_);
        }
        else {
            long long v = strtoll(*p, &end, 10);
            if(end == *p) return -1;
            *p = end;
            switch(T.sizeof_){
                case 1: *(int8_t*)out = (int8_t)v; break;
                case 2: *(int16_t*)out = (int16_t)v; break;
                case 4: *(int32_t*)out = (int32_t)v; break;
                case 8: *(int64_t*)out = (int64_t)v; break;
                default: return 1;
            }
            return 0;
        }
    }
    if(T.is_float){
        char* end;
        double v = strtod(*p, &end);
        if(end == *p) return -1;
        *p = end;
        if(T == float)
            *(float*)out = (float)v;
        else
            *(double*)out = v;
        return 0;
    }
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
            for(size_t i = 0; i < T.fields; i++){
                auto f = T.field(i);
                if(strcmp(key, f.name) == 0){
                    int err = json_parse(f.type, p, (char*)out + f.offset);
                    if(err) return err;
                    goto found;
                    break;
                }
            }
            json_skip(p);
            found:;
        }
    }
    fprintf(stderr, "json: unsupported type '%s'\n", T.name);
    return -1;
}

void json_indent(FILE* f, int indent){
    for(int i = 0; i < indent; i++) fprintf(f, "  ");
}

int json_write(_Type T, FILE* f, const void* data, int indent){
    if(T == const char*){
        const char* s = *(const char**)data;
        if(!s){ fprintf(f, "null"); return 0;}
        fputc('"', f);
        for(const char* c = s; *c; c++){
            if(*c == '"') fprintf(f, "\\\"");
            else if(*c == '\\') fprintf(f, "\\\\");
            else if(*c == '\n') fprintf(f, "\\n");
            else fputc(*c, f);
        }
        fputc('"', f);
        return 0;
    }
    if(T == _Bool){
        fprintf(f, *(_Bool*)data ? "true" : "false");
        return 0;
    }
    if(T.is_enum){
        long long v;
        if(T.is_signed){
            switch(T.sizeof_){
                case 1: v = (long long)*(int8_t*)data; break;
                case 2: v = (long long)*(int16_t*)data; break;
                case 4: v = (long long)*(int32_t*)data; break;
                case 8: v = (long long)*(int64_t*)data; break;
                default: return 1;
            }
        }
        else {
            switch(T.sizeof_){
                case 1: v = (long long)*(uint8_t*)data; break;
                case 2: v = (long long)*(uint16_t*)data; break;
                case 4: v = (long long)*(uint32_t*)data; break;
                case 8: v = (long long)*(uint64_t*)data; break;
                default: return 1;
            }
        }
        for(size_t i = 0; i < T.enumerators; i++){
            auto e = T.enumerator(i);
            if(e.value == v){
                fprintf(f, "\"%s\"", e.name);
                return 0;
            }
        }
        fprintf(f, "%lld", v); // unknown enumerator
        return 0;
    }
    if(T.is_integer){
        if(T.is_signed){
            long long v;
            switch(T.sizeof_){
                case 1: v = (long long)*(int8_t*)data; break;
                case 2: v = (long long)*(int16_t*)data; break;
                case 4: v = (long long)*(int32_t*)data; break;
                case 8: v = (long long)*(int64_t*)data; break;
                default: return 1;
            }
            fprintf(f, "%lld", v);
        }
        else {
            unsigned long long v;
            switch(T.sizeof_){
                case 1: v = (unsigned long long)*(uint8_t*)data; break;
                case 2: v = (unsigned long long)*(uint16_t*)data; break;
                case 4: v = (unsigned long long)*(uint32_t*)data; break;
                case 8: v = (unsigned long long)*(uint64_t*)data; break;
                default: return 1;
            }
            fprintf(f, "%llu", v);
        }
        return 0;
    }
    if(T.is_float){
        double v;
        if(T == float)
            v = (double)*(float*)data;
        else if(T == double)
            v = *(double*)data;
        else
            return 1;
        fprintf(f, "%g", v);
        return 0;
    }
    if(T.is_struct){
        fprintf(f, "{\n");
        size_t n = T.fields;
        for(size_t i = 0; i < n; i++){
            auto field = T.field(i);
            json_indent(f, indent + 1);
            fprintf(f, "\"%s\": ", field.name);
            json_write(field.type, f, (const char*)data + field.offset, indent + 1);
            if(i + 1 < n) fputc(',', f);
            fputc('\n', f);
        }
        json_indent(f, indent);
        fputc('}', f);
        return 0;
    }
    return 1;
}


//////////////////
//
//    Demo
//

#pragma typedef on
enum Role { WARRIOR, MAGE, ROGUE, HEALER };

struct Vec2 { double x, y; };

struct Player {
    const char* name;
    int hp;
    int level;
    _Bool alive;
    Role role;
    Vec2 pos;
};

void print_player(const Player* p){
    json_write(Player, stdout, p, 0);
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

printf("\n--- Inline type printing ---\n");
json_write(struct Foo {int x; int y;}, stdout, &(struct Foo){1, 2}, 0);
printf("\n");
