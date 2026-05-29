// Runtime JSON parser using _Type introspection
#pragma once
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int json_parse_(_Type T, const char** p, void* out);
int json_write_(_Type T, FILE* f, const void* data, int indent);

int json_parse(_Type T, const char* p, void* out){
    return json_parse_(T, &p, out);
}
int json_write(_Type T, FILE* f, const void* data){
    return json_write_(T, f, data, 0);
}

int json_parse_(_Type T, const char** p, void* out){
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
        char* buf = malloc(len+1);
        if(!buf) return NULL;
        memcpy(buf, start, len);
        buf[len] = 0;
        if(**p == '"') ++*p;
        const char* result = __builtin_intern(buf);
        free(buf);
        return result;
    }

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

    json_ws(p);
    if(T == const char* || T == char *){
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
            return 0;
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
        _Bool first = 1;
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
                    int err = json_parse_(f.type, p, (char*)out + f.offset);
                    if(err) return err;
                    goto found;
                    break;
                }
            }
            json_skip(p);
            found:;
        }
    }
    if(T.is_array){
        if(**p != '[') return -1;
        ++*p;
        memset(out, 0, T.sizeof_);
        for(size_t i = 0;i<T.count; i++){
            json_ws(p);
            if(**p == ']') { ++*p; return 0; }
            if(i){
                if(**p != ',') return -1;
                ++*p;
            }
            json_ws(p);
            int err = json_parse_(T.element_type, p, (char*)out+T.element_type.sizeof_*i);
            if(err) return err;
        }
        if(**p == ']') { ++*p; return 0; }
        return -1;
    }
    fprintf(stderr, "json: unsupported type '%s'\n", T.name);
    return -1;
}


int json_write_(_Type T, FILE* f, const void* data, int indent){
    if(T == const char* || T == char *){
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
    void json_indent(FILE* f, int indent){ fprintf(f, "%*s", indent*2, ""); }
    if(T.is_struct){
        fprintf(f, "{\n");
        size_t n = T.fields;
        for(size_t i = 0; i < n; i++){
            auto field = T.field(i);
            json_indent(f, indent + 1);
            fprintf(f, "\"%s\": ", field.name);
            json_write_(field.type, f, (const char*)data + field.offset, indent + 1);
            if(i + 1 < n) fputc(',', f);
            fputc('\n', f);
        }
        json_indent(f, indent);
        fputc('}', f);
        return 0;
    }
    if(T.is_array){
        const et = T.element_type;
        fputc('[', f);
        if(et.is_struct)
            fputc('\n', f);
        for(size_t i = 0; i < T.count; i++){
            if(et.is_struct)
                json_indent(f, indent + 1);
            json_write_(et, f, (const char*)data + i*et.sizeof_, indent + 1);
            if(i + 1 < T.count) fputc(',', f);
            if(et.is_struct)
                putc('\n', f);
            else if(i +1 < T.count)
                putc(' ', f);
        }
        if(et.is_struct)
            json_indent(f, indent);
        fprintf(f, "]");
    }
    return 1;
}
