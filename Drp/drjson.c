//
// Copyright © 2022-2024, David Priver <david@davidpriver.com>
//
#ifndef DRJSON_C
#define DRJSON_C
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#ifndef DRJSON_NO_STDIO
#include <stdio.h>
#endif

#include "windowsheader.h"
#include "posixheader.h"

// #include "../compiler_warnings.h"
#ifndef DRJSON_API

#ifndef DRJSON_STATIC_LIB
#ifdef _WIN32
#define DRJSON_API extern __declspec(dllexport)
#else
#define DRJSON_API extern __attribute__((visibility("default")))
#endif
#else
#define DRJSON_API extern __attribute__((visibility("hidden")))
#endif
#endif

#include "drjson.h"
#include "drjson_itoa.h"
#include "hash_func.h"
#define PARSE_NUMBER_PARSE_FLOATS 1
#include "parse_numbers.h"
#define FPCONV_API static inline
#include "fpconv/src/fpconv.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

#ifndef force_inline
#if defined(__GNUC__) || defined(__clang__)
#define force_inline static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define force_inline static inline __forceinline
#else
#define force_inline static inline
#endif
#endif

#if defined(__GNUC__) || defined(__clang__)
#define ALLOCATOR_SIZE(N) __attribute__((alloc_size(N)))
#define MALLOC_FUNC __attribute__((malloc))
#else
#define ALLOCATOR_SIZE(...)
#define MALLOC_FUNC
#endif

#if !defined(likely) && !defined(unlikely)
#if defined(__GNUC__) || defined(__clang__)
#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif
#endif

#ifndef CASE_a_z
#define CASE_a_z 'a': case 'b': case 'c': case 'd': case 'e': case 'f': \
    case 'g': case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': \
    case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't': \
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z'
#endif

#ifndef CASE_A_Z
#define CASE_A_Z 'A': case 'B': case 'C': case 'D': case 'E': case 'F': \
    case 'G': case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': \
    case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': \
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z'
#endif

#ifndef CASE_A_F
#define CASE_A_F 'A': case 'B': case 'C': case 'D': case 'E': case 'F'
#endif

#ifndef CASE_a_f
#define CASE_a_f 'a': case 'b': case 'c': case 'd': case 'e': case 'f'
#endif

#ifndef CASE_0_9
#define CASE_0_9 '0': case '1': case '2': case '3': case '4': case '5': \
    case '6': case '7': case '8': case '9'
#endif
static
_Bool
drj_is_ro(DrJsonContext* ctx, DrJsonValue val);

static
DrJsonValue
drj_intern_object(DrJsonContext* ctx, DrJsonValue val, _Bool);

static
DrJsonValue
drj_intern_array(DrJsonContext* ctx, DrJsonValue val, _Bool);

typedef struct DrJsonObjectPair DrJsonObjectPair;
struct DrJsonObjectPair {
    Atom atom;
    DrJsonValue value;
};

typedef struct DrJsonHashIndex DrJsonHashIndex;
struct DrJsonHashIndex {
    uint32_t index;
};

// #define DRJ_DEBUG


typedef struct DrJsonObject DrJsonObject;
struct DrJsonObject {
    void*_Null_unspecified object_items;
    uint32_t count:31;
    uint32_t marked:1;
    uint32_t capacity:31;
    uint32_t read_only:1;
#ifdef DRJ_DEBUG
    _Bool freed:1;
#endif
};

typedef struct DrJsonArray DrJsonArray;
struct DrJsonArray {
    DrJsonValue*_Nullable array_items;
    uint32_t count:31;
    uint32_t marked:1;
    uint32_t capacity:31;
    uint32_t read_only:1;
#ifdef DRJ_DEBUG
    _Bool freed:1;
#endif
};


typedef struct DrjHashIdx DrjHashIdx;
struct DrjHashIdx {
    uint32_t hash;
    uint32_t idx;
};

struct DrJsonContext {
    Allocator allocator;
    AtomTable* atom_table;
    struct {
        DrJsonObject* data;
        size_t count;
        size_t capacity;
        size_t free_object;
    } objects;
    struct {
        DrjHashIdx* data;
        size_t count;
        size_t capacity;
    } interned_objects;
    struct {
        DrJsonArray* data;
        size_t count;
        size_t capacity;
        size_t free_array;
    } arrays;
    struct {
        DrjHashIdx* data;
        size_t count;
        size_t capacity;
    } interned_arrays;
};

static inline
void*_Nullable
drj_alloc(const DrJsonContext* ctx, size_t size){
    return Allocator_alloc(ctx->allocator, size);
}

static inline
void
drj_free(const DrJsonContext* ctx, const void*_Nullable p, size_t size){
    Allocator_free(ctx->allocator, p, size);
}

static inline
void*_Nullable
drj_realloc(const DrJsonContext* ctx, void*_Nullable p, size_t old_size, size_t new_size){
    if(!old_size || !p) return drj_alloc(ctx, new_size);
    if(!new_size) { drj_free(ctx, p, old_size); return NULL;}
    return Allocator_realloc(ctx->allocator, p, old_size, new_size);
}


DRJSON_API
DrJsonContext*_Nullable
drjson_create_ctx(Allocator allocator, AtomTable* atom_table){
    DrJsonContext* ctx = Allocator_zalloc(allocator, sizeof *ctx);
    if(!ctx) return NULL;
    ctx->allocator = allocator;
    ctx->atom_table = atom_table;
    return ctx;
}

static inline
size_t
drjson_size_for_object_of_length(size_t len){
    return len * sizeof(DrJsonObjectPair) + 2*len*sizeof(DrJsonHashIndex);
}

force_inline
void
drj_get_obj_ptrs(void* p, size_t cap, DrJsonHashIndex*_Nullable*_Nonnull hi, DrJsonObjectPair*_Nullable*_Nonnull pa){
    *pa = p;
    *hi = (DrJsonHashIndex*)(((char*)p)+cap*sizeof(DrJsonObjectPair));
}

force_inline
DrJsonObjectPair*
drj_obj_get_pairs(void* p, size_t cap){
    (void)cap;
    return p;
}

force_inline
DrJsonHashIndex*
drj_obj_get_idxes(void* p, size_t cap){
    return (DrJsonHashIndex*)(((char*)p)+cap*sizeof(DrJsonObjectPair));
}




force_inline
int
drjson_object_set_item_internal(DrJsonContext* ctx, DrJsonValue o, Atom atom, DrJsonValue item);

static inline
ssize_t
alloc_obj(DrJsonContext* ctx){
    if(ctx->objects.free_object){
        ssize_t result = ctx->objects.free_object;
        DrJsonObject* p = &ctx->objects.data[result];
        ctx->objects.free_object = p->count;
        *p = (DrJsonObject){0};
        #ifdef DRJ_DEBUG
        fprintf(stderr, "Recycling object %p (%zu)\n", (void*)p, (size_t)result);
        #endif
        return result;
    }
    if(ctx->objects.capacity <= ctx->objects.count){
        size_t new_cap = ctx->objects.capacity? ctx->objects.capacity*2:8;
        DrJsonObject* p = drj_realloc(ctx, ctx->objects.data, sizeof(DrJsonObject)*ctx->objects.capacity, sizeof(DrJsonObject)*new_cap);
        if(!p)
            return -1;
        ctx->objects.data = p;
        ctx->objects.capacity = new_cap;
        #ifdef DRJ_DEBUG
        fprintf(stderr, "reallocing object array\n");
        #endif
    }
    ssize_t result = ctx->objects.count++;
    DrJsonObject* odata = ctx->objects.data;
    odata[result] = (DrJsonObject){0};
    #ifdef DRJ_DEBUG
    fprintf(stderr, "allocated object %p (%zu)\n", (void*)&odata[result], (size_t)result);
    #endif
    return result;
}

static inline
ssize_t
alloc_array(DrJsonContext* ctx){
    if(ctx->arrays.free_array){
        ssize_t result = ctx->arrays.free_array;
        DrJsonArray* p = &ctx->arrays.data[result];
        ctx->arrays.free_array = p->count;
        *p = (DrJsonArray){0};
        #ifdef DRJ_DEBUG
        fprintf(stderr, "Recycling array %p (%zu)\n", (void*)p, (size_t)result);
        #endif
        return result;
    }
    if(ctx->arrays.capacity <= ctx->arrays.count){
        size_t new_cap = ctx->arrays.capacity? ctx->arrays.capacity*2:8;
        DrJsonArray* p = ctx->arrays.data? drj_realloc(ctx, ctx->arrays.data, sizeof(DrJsonArray)*ctx->arrays.capacity, sizeof(DrJsonArray)*new_cap) : drj_alloc(ctx, sizeof(DrJsonArray)*new_cap);
        if(!p)
            return -1;
        ctx->arrays.data = p;
        ctx->arrays.capacity = new_cap;
    }
    ssize_t result = ctx->arrays.count++;
    DrJsonArray* adata = ctx->arrays.data;
    adata[result] = (DrJsonArray){0};
    return result;
}



DRJSON_API
DrJsonValue
drjson_make_object(DrJsonContext* ctx){
    ssize_t idx = alloc_obj(ctx);
    if(idx < 0) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom");
    return (DrJsonValue){.kind=DRJSON_OBJECT, .object_idx=idx};
}

static inline
DrJsonValue
drjson_make_obj_keys(DrJsonValue o){
    o.kind = DRJSON_OBJECT_KEYS;
    return o;
}

static inline
DrJsonValue
drjson_make_obj_values(DrJsonValue o){
    o.kind = DRJSON_OBJECT_VALUES;
    return o;
}

static inline
DrJsonValue
drjson_make_obj_items(DrJsonValue o){
    o.kind = DRJSON_OBJECT_ITEMS;
    return o;
}

#if 0
static inline
DrJsonValue
drjson_make_array_view(DrJsonValue o){
    o.kind = DRJSON_ARRAY_VIEW;
    return o;
}
#endif

DRJSON_API
DrJsonValue
drjson_make_array(DrJsonContext* ctx){
    ssize_t idx = alloc_array(ctx);
    if(idx < 0) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom");
    return (DrJsonValue){.kind=DRJSON_ARRAY, .array_idx=idx};
}

// NOTE: we consider commas and colons to be whitespace ;)
static inline
void
skip_whitespace(DrJsonParseContext* ctx){
    const char* cursor = ctx->cursor;
    const char* end = ctx->end;
    strip:
    for(;cursor != end; cursor++){
        if(*cursor <= 0x20) continue;
        switch(*cursor){
            case ' ': case '\r':
            case '\t': case '\n':
            case ',': case ':':
            case '=':
                continue;
            case '/':
                cursor++;
                goto comment;
            default:
                goto end;
        }
    }
    end:
    ctx->cursor = cursor;
    return;
    comment:
    if(cursor==end){
        goto end;
    }
    if(*cursor == '/'){
        cursor++;
        const char* p = memchr(cursor, '\n', end-cursor);
        if(!p) cursor = end;
        else cursor = p+1;
        goto strip;
    }
    else if(*cursor == '*'){
        cursor++;
        for(;;){
            const char* p = memchr(cursor, '*', end-cursor);
            if(p && p != end && p+1 != end && p[1] == '/'){
                cursor = p+2;
                goto strip;
            }
            if(p){
                cursor = p + 1;
            }
            else {
                cursor = end;
                goto end;
            }
        }
    }
    else{
        cursor--;
        goto end; // let parses take it as an invalid character
    }
}

force_inline
_Bool
drj_match(DrJsonParseContext* ctx, char c){
    if(ctx->cursor == ctx->end)
        return 0;
    if(*ctx->cursor != c)
        return 0;
    ctx->cursor += 1;
    return 1;
}

static inline
DrJsonValue
drj_make_atom_val(DrJsonParseContext* ctx, const char* str, size_t len){
    // TODO: simd
    const char* back = memchr(str, '\\', len);
    Atom atom;
    if(back){
        char* tmp = Allocator_alloc(ctx->ctx->allocator, len);
        char* o = tmp;
        const char* p = str;
        const char* end = str + len;
        for(;;){
            if(back > p){
                size_t amount = back-p;
                drj_memcpy(o, p, amount);
                o += amount;
            }
            p = back + 1;
            if(p == end){
                o++[0] = '\\';
                break;
            }
            switch(p[0]){
                case '"':
                case '\\':
                    o++[0] = p++[0];
                    break;
                case 'b':
                    p++;
                    o++[0] = '\b';
                    break;
                case 'f':
                    p++;
                    o++[0] = '\f';
                    break;
                case 'n':
                    p++;
                    o++[0] = '\n';
                    break;
                case 'r':
                    p++;
                    o++[0] = '\r';
                    break;
                case 't':
                    p++;
                    o++[0] = '\t';
                    break;
                case 'u':
                    // TODO:
                default:
                    o++[0] = '\\';
                    o++[0] = p++[0];
                    break;
            }
            if(end == p)
                break;
            back = memchr(p, '\\', end-p);

            if(!back){
                drj_memcpy(o, p, end-p);
                o += end-p;
                break;
            }
        }
        atom = AT_atomize(ctx->ctx->atom_table, tmp, o-tmp);
        Allocator_free(ctx->ctx->allocator, tmp, len);
        if(!atom) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "failed to make atom");
        return (DrJsonValue){.string={.kind = DRJSON_STRING,.needs_escape=1}, .atom=atom};
    }
    else{
        atom = AT_atomize(ctx->ctx->atom_table, str, len);
        if(!atom) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "failed to make atom");
        return (DrJsonValue){.kind = DRJSON_STRING, .atom=atom};
    }
}

static inline
DrJsonValue
parse_string(DrJsonParseContext* ctx){
    skip_whitespace(ctx);
    if(ctx->cursor == ctx->end)
        return drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "eof when beginning parsing string");
    const char* string_start;
    const char* string_end;
    const char* cursor = ctx->cursor;
    const char* end = ctx->end;
    if(likely(drj_match(ctx, '"'))){
        cursor = ctx->cursor;
        string_start = cursor;
        for(;;){
            const char* close = memchr(cursor, '"', end-cursor);
            if(unlikely(!close)) return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "No closing '\"' for a string");
            cursor = close+1;
            int nbackslashes = 0;
            int negidx = -1;
            while(close[negidx--] == '\\')
                nbackslashes++;
            if(nbackslashes & 1) continue;
            string_end = close;
            break;
        }
        ctx->cursor = cursor;
        return drj_make_atom_val(ctx, string_start, string_end-string_start);
    }
    else if(drj_match(ctx, '\'')){
        cursor = ctx->cursor;
        string_start = cursor;
        for(;;){
            const char* close = memchr(cursor, '\'', end-cursor);
            if(unlikely(!close)) return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "No closing \"'\" for a string");
            cursor = close+1;
            int nbackslashes = 0;
            int negidx = -1;
            while(close[negidx--] == '\\')
                nbackslashes++;
            if(nbackslashes & 1) continue;
            string_end = close;
            break;
        }
        ctx->cursor = cursor;
        return drj_make_atom_val(ctx, string_start, string_end-string_start);
    }
    else {
        string_start = cursor;
        // allow bare identifiers
        for(;cursor != end; cursor++){
            switch(*cursor){
                default:
                    if(cursor == string_start) return drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "zero length when expecting a string");
                    else goto after2;
                case CASE_a_z:
                case CASE_A_Z:
                case CASE_0_9:
                case '_':
                case '-':
                case '.':
                case '/':
                case '+':
                case '*':
                    continue;
            }
        }
        after2:
        ctx->cursor = cursor;
        string_end = cursor;
        return drj_make_atom_val(ctx, string_start, string_end-string_start);
    }
}


static
DrJsonValue
drj_parse(DrJsonParseContext* ctx);

static inline
DrJsonValue
parse_object(DrJsonParseContext* ctx){
    if(unlikely(!drj_match(ctx, '{'))) {
        return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "Expected a '{' to begin an object");
    }
    DrJsonValue result = drjson_make_object(ctx->ctx);
    DrJsonValue error = {0};
    skip_whitespace(ctx);
    while(!drj_match(ctx, '}')){
        if(unlikely(ctx->cursor == ctx->end)){
            error = drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "Eof before closing '}'");
            goto cleanup;
        }
        skip_whitespace(ctx);
        DrJsonValue key = parse_string(ctx);
        if(unlikely(key.kind == DRJSON_ERROR)){
            error = key;
            goto cleanup;
        }
        DrJsonValue item = drj_parse(ctx);
        if(unlikely(item.kind == DRJSON_ERROR)){
            error = item;
            goto cleanup;
        }
        int err = drjson_object_set_item(ctx->ctx, result, key.atom, item);
        if(unlikely(err)){
            error = drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "Failed to allocate space for an item while setting member of an object");
            goto cleanup;
        }
        skip_whitespace(ctx);
    }
    if(ctx->_read_only_objects)
        result = drj_intern_object(ctx->ctx, result, 1);
    return result;
    cleanup:
    return error;
}

static inline
DrJsonValue
parse_array(DrJsonParseContext* ctx){
    if(!drj_match(ctx, '[')) return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "Expected a '[' to begin an array");
    DrJsonValue result = drjson_make_array(ctx->ctx);
    DrJsonValue error = {0};
    skip_whitespace(ctx);
    while(!drj_match(ctx, ']')){
        if(unlikely(ctx->cursor == ctx->end)){
            error = drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "Eof before closing ']'");
            goto cleanup;
        }
        DrJsonValue item = drj_parse(ctx);
        if(unlikely(item.kind == DRJSON_ERROR)){
            error = item;
            goto cleanup;
        }
        int err = drjson_array_push_item(ctx->ctx, result, item);
        if(unlikely(err)){
            error = drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "Failed to push an item onto an array");
            goto cleanup;
        }
        skip_whitespace(ctx);
    }
    if(ctx->_read_only_objects)
        result = drj_intern_array(ctx->ctx, result, 1);
    return result;
    cleanup:
    return error;
}

static inline
DrJsonValue
parse_bool_null(DrJsonParseContext* ctx){
    ptrdiff_t length = ctx->end - ctx->cursor;
    if(length >= 4 && memcmp(ctx->cursor, "true", 4) == 0){
        ctx->cursor += 4;
        return drjson_make_bool(1);
    }
    if(length >= 5 && memcmp(ctx->cursor, "false", 5) == 0){
        ctx->cursor += 5;
        return drjson_make_bool(0);
    }
    if(length >= 4 && memcmp(ctx->cursor, "null", 4) == 0){
        ctx->cursor += 4;
        return drjson_make_null();
    }
    return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "Invalid literal");
}
static inline
DrJsonValue
parse_number(DrJsonParseContext* ctx){
    const char* num_begin = ctx->cursor;
    const char* cursor = ctx->cursor;
    const char* end = ctx->end;
    _Bool has_exponent = 0;
    _Bool has_decimal = 0;
    _Bool has_minus = 0;
    for(;cursor != end; cursor++){
        switch(*cursor){
            case 'e': case 'E':
                has_exponent = 1;
                continue;
            case '-':
                has_minus = 1;
                continue;
            case CASE_0_9:
            case '+':
                continue;
            case '.':
                has_decimal = 1;
                continue;
            default:
                goto after;
        }
    }
    after:;
    ptrdiff_t length = cursor - num_begin;
    if(!length) return drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "Zero length number");
    DrJsonValue result;
    if(has_exponent || has_decimal){
        DoubleResult pr = parse_double(num_begin, length);
        if(pr.errored){
            return drjson_make_error(DRJSON_ERROR_INVALID_VALUE, "Failed to parse number");
        }
        result = drjson_make_number(pr.result);
    }
    else if(has_minus){
        Int64Result pr = parse_int64(num_begin, length);
        if(pr.errored){
            return drjson_make_error(DRJSON_ERROR_INVALID_VALUE, "Failed to parse number");
        }
        result =  drjson_make_int(pr.result);
    }
    else {
        Uint64Result pr = parse_uint64(num_begin, length);
        if(pr.errored){
            return drjson_make_error(DRJSON_ERROR_INVALID_VALUE, "Failed to parse number");
        }
        result =  drjson_make_uint(pr.result);
    }
    ctx->cursor = cursor;
    return result;
}

force_inline
unsigned
hexchar_to_value(char c){
    unsigned value = (uint8_t)c;
    value |= 0x20u;
    if(value > '9')
        return value - 'a' + 10u;
    return value - '0';
}

static inline
DrJsonValue
parse_color(DrJsonParseContext* ctx){
    const char* num_begin = ctx->cursor;
    const char* cursor = ctx->cursor;
    const char* end = ctx->end;
    for(;cursor != end; cursor++){
        switch(*cursor){
            case CASE_0_9:
            case CASE_a_f:
            case CASE_A_F:
                continue;
            default:
                goto after;
        }
    }
    after:;
    ptrdiff_t length = cursor - num_begin;
    if(!length) return drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "0 length color");
    uint32_t value = 0x00000000;
    if(length == 3){
        value |= 0xff000000u;
        for(int i = 0; i < 3; i++){
            unsigned b = hexchar_to_value(num_begin[i]);
            b |= b << 4;
            value |= b << (i * 8);
        }
    }
    else if(length == 4){
        for(int i = 0; i < 4; i++){
            unsigned b = hexchar_to_value(num_begin[i]);
            b |= b << 4;
            value |= b << (i * 8);
        }
    }
    else if(length == 6){
        value |= 0xff000000u;
        for(int i = 0; i < 3; i++){
            unsigned hi = hexchar_to_value(num_begin[i*2]);
            unsigned lo = hexchar_to_value(num_begin[i*2+1]);
            value |= lo << (8*i);
            value |= hi << (8*i + 4);
        }

    }
    else if(length == 8){
        for(int i = 0; i < 4; i++){
            unsigned hi = hexchar_to_value(num_begin[i*2]);
            unsigned lo = hexchar_to_value(num_begin[i*2+1]);
            value |= lo << (8*i);
            value |= hi << (8*i + 4);
        }
    }
    else {
        return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "colors must be 3, 4, 6 or 8 numeric digits after the '#'");
    }
    ctx->cursor = cursor;
    return drjson_make_uint(value);
}
static inline
DrJsonValue
drj_parse_hex(DrJsonParseContext* ctx){
    const char* num_begin = ctx->cursor;
    const char* cursor = ctx->cursor;
    const char* end = ctx->end;
    for(;cursor != end; cursor++){
        switch(*cursor){
            case CASE_0_9:
            case CASE_a_f:
            case CASE_A_F:
                continue;
            default:
                goto after;
        }
    }
    after:;
    ptrdiff_t length = cursor - num_begin;
    if(!length) return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "0 length hex literal");
    if(length > 16) return drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "Hex literal longer than 16 digits");
    uint64_t value = 0;
    for(ptrdiff_t i = 0; i < length; i++){
        value <<= 4;
        value |= hexchar_to_value(num_begin[i]);
    }
    ctx->cursor = cursor;
    return drjson_make_uint(value);
}

static
DrJsonValue
drjson_parse_braceless_object(DrJsonParseContext* ctx);


DRJSON_API
DrJsonValue
drjson_parse(DrJsonParseContext* ctx, unsigned flags){
    if(flags & DRJSON_PARSE_FLAG_INTERN_OBJECTS)
        ctx->_read_only_objects = 1;
    if(flags & DRJSON_PARSE_FLAG_BRACELESS_OBJECT)
        return drjson_parse_braceless_object(ctx);
    return drj_parse(ctx);
}

static
DrJsonValue
drj_parse(DrJsonParseContext* ctx){
    ctx->depth++;
    if(unlikely(ctx->depth > 100))
        return drjson_make_error(DRJSON_ERROR_TOO_DEEP, "Too many levels of nesting.");
    skip_whitespace(ctx);
    DrJsonValue result;
    if(ctx->cursor == ctx->end)
        return drjson_make_error(DRJSON_ERROR_UNEXPECTED_EOF, "Eof before any values");
    switch(ctx->cursor[0]){
        case '{':
            result = parse_object(ctx);
            break;
        case '[':
            result = parse_array(ctx);
            break;
        case '\'':
        case '"':
            result = parse_string(ctx);
            break;
        case 't':
        case 'f':
        case 'n':
            result = parse_bool_null(ctx);
            if(result.kind == DRJSON_ERROR)
                result = parse_string(ctx);
            break;
        case '#':
            ctx->cursor++;
            result = parse_color(ctx);
            break;
        case '+':
        case '.': case '-':
        case '1': case '2': case '3':
        case '4': case '5': case '6': case '7': case '8': case '9':
            result = parse_number(ctx);
            if(result.kind == DRJSON_ERROR)
                result = parse_string(ctx);
            break;
        case '0':
            if(ctx->cursor + 1 != ctx->end){
                if((ctx->cursor[1] | 0x20) == 'x'){
                    ctx->cursor += 2;
                    result = drj_parse_hex(ctx);
                    break;
                }
            }
            result = parse_number(ctx);
            if(result.kind == DRJSON_ERROR)
                result = parse_string(ctx);
            break;

        default:
            result = parse_string(ctx);
            if(result.kind != DRJSON_ERROR) break;
            result = drjson_make_error(DRJSON_ERROR_INVALID_CHAR, "Character is not a valid starting character for json");
            break;
    }
    ctx->depth--;
    return result;
}

DRJSON_API
DrJsonValue
drjson_parse_string(DrJsonContext* jctx, const char* text, size_t length, unsigned flags){
    DrJsonParseContext ctx = {
        .ctx = jctx,
        .begin = text,
        .cursor = text,
        .end = text+length,
        .depth = 0,
    };
    return drjson_parse(&ctx, flags);
}


static
DrJsonValue
drjson_parse_braceless_object(DrJsonParseContext* ctx){
    DrJsonValue result = drjson_make_object(ctx->ctx);
    DrJsonValue error = {0};
    ctx->depth++;
    skip_whitespace(ctx);
    for(skip_whitespace(ctx); ctx->cursor != ctx->end; skip_whitespace(ctx)){
        DrJsonValue key = parse_string(ctx);
        if(unlikely(key.kind == DRJSON_ERROR)){
            error = key;
            goto cleanup;
        }
        DrJsonValue item = drj_parse(ctx);
        if(unlikely(item.kind == DRJSON_ERROR)){
            error = item;
            goto cleanup;
        }
        int err = drjson_object_set_item(ctx->ctx, result, key.atom, item);
        if(unlikely(err)){
            error = drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "Failed to allocate space for an item while setting member of an object");
            goto cleanup;
        }
    }
    ctx->depth--;
    return result;
    cleanup:
    return error;
}

DRJSON_API
int // 0 on success
drjson_array_push_item(const DrJsonContext* ctx, DrJsonValue a, DrJsonValue item){
    if(a.kind != DRJSON_ARRAY) return 1;
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[a.array_idx];
    if(array->read_only) return 1;
    if(array->capacity < array->count+1u){
        size_t old_cap = array->capacity;
        enum {ARRAY_MAX = 0x1fffffff};
        size_t new_cap = old_cap?old_cap*2:4;
        if(new_cap > ARRAY_MAX) return 1;
        DrJsonValue* new_items = drj_realloc(ctx, array->array_items, old_cap*sizeof(*new_items), new_cap*sizeof(*new_items));
        if(!new_items) return 1;
        array->array_items = new_items;
        array->capacity = (uint32_t)new_cap;
    }
    array->array_items[array->count] = item;
    array->count += 1;
    return 0;
}
DRJSON_API
int // 0 on success
drjson_array_insert_item(const DrJsonContext* ctx, DrJsonValue a, size_t idx, DrJsonValue item){
    if(a.kind != DRJSON_ARRAY) return 1;
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[a.array_idx];
    if(array->read_only) return 1;
    if(idx >= array->count) return 1;
    if(array->capacity < array->count+1u){
        size_t old_cap = array->capacity;
        enum {ARRAY_MAX = 0x1fffffff};
        size_t new_cap = old_cap?old_cap*2:4;
        if(new_cap > ARRAY_MAX) return 1;
        DrJsonValue* new_items = drj_realloc(ctx, array->array_items, old_cap*sizeof(*new_items), new_cap*sizeof(*new_items));
        if(!new_items) return 1;
        array->array_items = new_items;
        array->capacity = (uint32_t)new_cap;
    }
    size_t nmove = array->count - idx;
    drj_memmove(array->array_items+idx+1, array->array_items+idx, nmove * sizeof(*array->array_items));
    array->array_items[idx] = item;
    array->count += 1;
    return 0;
}

DRJSON_API
DrJsonValue
drjson_array_pop_item(const DrJsonContext* ctx, DrJsonValue a){
    if(a.kind != DRJSON_ARRAY) return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "Argument is not an array");
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[a.array_idx];
    if(array->read_only)
        return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "Argument is read only");
    if(!array->count)
        return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "Array is empty");
    return array->array_items[--array->count];
}

DRJSON_API
int
drjson_clear(const DrJsonContext* ctx, DrJsonValue v){
    switch(v.kind){
        case DRJSON_ARRAY:{
            DrJsonArray* adata = ctx->arrays.data;
            DrJsonArray* array = &adata[v.array_idx];
            if(array->read_only) return 1;
            array->count = 0;
            return 0;
        }
        case DRJSON_OBJECT:{
            DrJsonObject* odata = ctx->objects.data;
            DrJsonObject* object = &odata[v.object_idx];
            if(object->read_only) return 1;
            drj_memset(object->object_items, 0, drjson_size_for_object_of_length(object->capacity));
            return 0;
        }
        default:
            return 1;
    }
}

DRJSON_API
DrJsonValue
drjson_array_del_item(const DrJsonContext* ctx, DrJsonValue a, size_t idx){
    if(a.kind != DRJSON_ARRAY) return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "Argument is not an array");
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[a.array_idx];
    if(array->read_only) return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "Argument is read only");
    if(!array->count)
        return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "Array is empty");
    if(idx >= array->count){
        return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "Index out of bounds.");
    }
    if(idx == array->count-1u)
        return drjson_array_pop_item(ctx, a);
    size_t nmove = array->count - idx-1;
    DrJsonValue result= array->array_items[idx];
    drj_memmove(array->array_items+idx, array->array_items+idx+1, nmove*sizeof(*array->array_items));
    array->count -= 1;
    return result;
}


force_inline
int
drjson_object_set_item_internal(DrJsonContext* ctx, DrJsonValue o, Atom atom, DrJsonValue item){
    if(o.kind != DRJSON_OBJECT) return 1;
    DrJsonObject* object = &ctx->objects.data[o.object_idx];
    if(object->read_only) return 1;
    enum {OBJECT_MAX = 0x1fffffff};
    if(unlikely(object->count >= object->capacity)){
        if(!object->capacity){
            size_t new_cap = 4;
            size_t size = drjson_size_for_object_of_length(new_cap);
            void* p = drj_alloc(ctx, size);
            if(!p) return 1;
            drj_memset(drj_obj_get_idxes(p, new_cap), 0xff, 2*new_cap*sizeof(DrJsonHashIndex));
            object->object_items = p;
            object->capacity = (uint32_t)new_cap;
        }
        else {
            size_t old_cap = object->capacity;
            size_t new_cap = old_cap * 2;
            if(new_cap > OBJECT_MAX) return 1;
            void* p = drj_realloc(ctx, object->object_items, drjson_size_for_object_of_length(old_cap), drjson_size_for_object_of_length(new_cap));
            if(!p) return 1;
            DrJsonHashIndex* idxes; DrJsonObjectPair* pairs;
            drj_get_obj_ptrs(p, new_cap, &idxes, &pairs);
            drj_memset(idxes, 0xff, 2*new_cap * sizeof *idxes);
            for(size_t i = 0; i < object->count; i++){
                const DrJsonObjectPair* pa = &pairs[i];
                uint32_t hash = pa->atom->hash;
                uint32_t idx = fast_reduce32(hash, (uint32_t)(2*new_cap));
                while(idxes[idx].index != UINT32_MAX){
                    idx++;
                    if(idx >= 2*new_cap) idx = 0;
                }
                idxes[idx].index = (uint32_t)i;
            }
            object->object_items = p;
            object->capacity = (uint32_t)new_cap;
        }
        #if 0
        if(object->capacity <= 32){
            DrJsonHashIndex* idxes = drj_obj_get_idxes(object->object_items, object->capacity);
            uint64_t seent = 0;
            for(size_t i = 0; i < object->capacity*2; i++){
                if(idxes[i].index == UINT32_MAX) continue;
                uint64_t mask = 1llu << idxes[i].index;
                if(mask & seent){
                    __builtin_debugtrap();
                }
                seent |= mask;
            }
        }
        #endif
    }

    #if 0
    if(object->capacity <= 32){
        DrJsonHashIndex* idxes = drj_obj_get_idxes(object->object_items, object->capacity);
        uint64_t seent = 0;
        for(size_t i = 0; i < object->capacity*2; i++){
            if(idxes[i].index == UINT32_MAX) continue;
            uint64_t mask = 1llu << idxes[i].index;
            if(mask & seent){
                __builtin_debugtrap();
            }
            seent |= mask;
        }
    }
    #endif
    uint32_t capacity = object->capacity;
    uint32_t hash = atom->hash;
    uint32_t idx = fast_reduce32(hash, 2*capacity);
    DrJsonHashIndex* idxes; DrJsonObjectPair* pairs;
    drj_get_obj_ptrs(object->object_items, object->capacity, &idxes, &pairs);
    for(;;){
        DrJsonHashIndex hi = idxes[idx];
        if(hi.index == UINT32_MAX){
            size_t pidx = object->count;
            object->count += 1;
            DrJsonObjectPair* op = &pairs[pidx];
            *op = (DrJsonObjectPair){
                .atom=atom,
                .value=item,
            };
            idxes[idx].index = (uint32_t)pidx;
            return 0;
        }
        DrJsonObjectPair* op = &pairs[hi.index];
        if(op->atom == atom){
            op->value = item;
            return 0;
        }
        idx++;
        if(idx >= 2*capacity)
            idx = 0;
    }
}

DRJSON_API
int // 0 on success
drjson_object_set_item2(DrJsonContext* ctx, DrJsonValue object, const char* key, size_t keylen, DrJsonValue item){
    Atom atom = AT_atomize(ctx->atom_table, key, keylen);
    if(!atom) return DRJSON_ERROR_ALLOC_FAILURE;
    return drjson_object_set_item_internal(ctx, object, atom, item);
}

DRJSON_API
int // 0 on success
drjson_object_set_item(DrJsonContext* ctx, DrJsonValue object, Atom atom, DrJsonValue item){
    return drjson_object_set_item_internal(ctx, object, atom, item);
}

DRJSON_API
DrJsonValue
drjson_object_get_item(const DrJsonContext* ctx, DrJsonValue o, Atom atom){
    if(o.kind != DRJSON_OBJECT) return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "not an object");
    uint32_t hash = atom->hash;
    const DrJsonObject* object = &ctx->objects.data[o.object_idx];
    if(!object->capacity)
        return drjson_make_error(DRJSON_ERROR_MISSING_KEY, "key is not valid for object");
    uint32_t capacity = object->capacity;
    uint32_t idx = fast_reduce32(hash, 2*capacity);
    DrJsonHashIndex* his; DrJsonObjectPair* pairs;
    drj_get_obj_ptrs(object->object_items, object->capacity, &his, &pairs);
    for(;;){
        DrJsonHashIndex hi = his[idx];
        if(hi.index == UINT32_MAX) return drjson_make_error(DRJSON_ERROR_MISSING_KEY, "key is not valid for object");
        const DrJsonObjectPair* op = &pairs[hi.index];
        if(op->atom == atom)
            return op->value;
        idx++;
        if(idx >= 2*capacity)
            idx = 0;
    }
}

DRJSON_API
DrJsonValue
drjson_object_get_item2(const DrJsonContext* ctx, DrJsonValue o, const char* key, size_t keylen){
    Atom atom = AT_atomize(ctx->atom_table, key, keylen);
    if(!atom) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom");
    return drjson_object_get_item(ctx, o, atom);
}

DRJSON_API
DrJsonValue
drjson_checked_query(const DrJsonContext* ctx, DrJsonValue v, int type, const char* query, size_t query_length){
    DrJsonValue o = drjson_query(ctx, v, query, query_length);
    if(o.kind == DRJSON_ERROR) return o;
    if(o.kind != type)
        return drjson_make_error(DRJSON_ERROR_INVALID_VALUE, "Wrong type");
    return o;
}

DRJSON_API
DrJsonValue
drjson_query(const DrJsonContext* ctx, DrJsonValue v, const char* query, size_t length){
    enum {
        GETITEM,
        SUBSCRIPT,
        QUOTED_GETITEM,
        KEYS,
        VALUES,
        // GLOB,
    };
    size_t begin = 0;
    size_t i = 0;
    DrJsonValue o = v;
    // This macro is vestigial, could just be replaced by returning the error.
    #define RETERROR(code, mess) return drjson_make_error(code, mess)
    if(i == length) RETERROR(DRJSON_ERROR_UNEXPECTED_EOF, "Query is 0 length");
    Ldispatch:
    for(;i != length; i++){
        char c = query[i];
        switch(c){
            case '.':
                i++;
                LHack:
                begin = i;
                if(i == length) RETERROR(DRJSON_ERROR_UNEXPECTED_EOF, "Empty query after a '.'");
                switch(query[i]){
                    case '"':
                        i++;
                        begin = i;
                        goto Lquoted_getitem;
                    case '#':
                    case '$':
                    case '@':
                        i++;
                        if(length - i >= sizeof("keys")-1 && memcmp(query+i, "keys", sizeof("keys")-1) == 0){
                            i += sizeof("keys")-1;
                            goto Lkeys;
                        }
                        if(length - i >= sizeof("values")-1 && memcmp(query+i, "values", sizeof("values")-1) == 0){
                            i += sizeof("values")-1;
                            goto Lvalues;
                        }
                        if(length - i >= sizeof("items")-1 && memcmp(query+i, "items", sizeof("items")-1) == 0){
                            i += sizeof("items")-1;
                            goto Litems;
                        }
                        if(length - i >= sizeof("length")-1 && memcmp(query+i, "length", sizeof("length")-1) == 0){
                            i += sizeof("length")-1;
                            goto Llength;
                        }
                        RETERROR(DRJSON_ERROR_INVALID_CHAR, "Unknown special key");
                    case CASE_0_9:
                    case CASE_a_z:
                    case CASE_A_Z:
                    case '/':
                    case '_':
                        goto Lgetitem;
                    default:
                        RETERROR(DRJSON_ERROR_INVALID_CHAR, "Invalid character identifier");
                }
            case '[':
                i++;
                begin = i;
                goto Lsubscript;
            default:
                if(i == 0)
                    goto LHack;
                RETERROR(DRJSON_ERROR_INVALID_CHAR, "Queries must continue with '.', '['");
        }
    }
    goto Ldone;
    Lgetitem:
        for(;i!=length;i++){
            switch(query[i]){
                case '.':
                case '[':
                    goto Ldo_getitem;
                case CASE_a_z:
                case CASE_A_Z:
                case CASE_0_9:
                case '/':
                case '_':
                case '-':
                case '+':
                case '*':
                    continue;
                default:
                    RETERROR(DRJSON_ERROR_INVALID_CHAR, "Invalid character in identifier query");
            }
        }
        // fall-through
    Ldo_getitem:
        if(i == begin) RETERROR(DRJSON_ERROR_INVALID_CHAR, "0 length query after '.'");
        o = drjson_object_get_item2(ctx, o, query+begin, i-begin);
        if(o.kind == DRJSON_ERROR) RETERROR(DRJSON_ERROR_MISSING_KEY, "Key not found");
        goto Ldispatch;
    Lsubscript:
        for(;i!=length;i++){
            switch(query[i]){
                case '-':
                case CASE_0_9:
                    continue;
                case ']':
                    goto Ldo_subscript;
                default:
                    RETERROR(DRJSON_ERROR_MISSING_KEY, "Invalid subscript character (must be integer)");
            }
        }
        // Need to see a ']'
        RETERROR(DRJSON_ERROR_UNEXPECTED_EOF, "No ']' found to close a subscript");
    Ldo_subscript:
        {
            // lazy
            Int64Result pr = parse_int64(query+begin, i-begin);
            if(pr.errored){
                RETERROR(DRJSON_ERROR_INVALID_VALUE, "Unable to parse number for subscript");
            }
            int64_t index = pr.result;
            o = drjson_get_by_index(ctx, o, index);
            if(o.kind == DRJSON_ERROR) return o;
        }
        i++;
        goto Ldispatch;
    Lquoted_getitem:
        for(;i!=length;i++){
            if(query[i] == '"'){
                assert(i != begin);
                assert(i);
                size_t nbackslash = 0;
                for(size_t back = i-1; back != begin; back--){
                    if(query[back] != '\\') break;
                    nbackslash++;
                }
                if(nbackslash & 1) continue;
                o = drjson_object_get_item2(ctx, o, query+begin, i-begin);
                if(o.kind == DRJSON_ERROR) RETERROR(DRJSON_ERROR_MISSING_KEY, "Key not found");
                i++;
                goto Ldispatch;
            }
        }
        RETERROR(DRJSON_ERROR_UNEXPECTED_EOF, "Unterminated quoted query");
    Llength:;
        int64_t len = drjson_len(ctx, o);
        if(len < 0) RETERROR(DRJSON_ERROR_TYPE_ERROR, "Length applied to non-object, non-array, non-string");
        o = drjson_make_uint((uint64_t)len);
        goto Ldispatch;
    Lkeys:
        if(o.kind != DRJSON_OBJECT) RETERROR(DRJSON_ERROR_TYPE_ERROR, "@keys applied to non-object");
        o = drjson_make_obj_keys(o);
        goto Ldispatch;
    Lvalues:
        if(o.kind != DRJSON_OBJECT) RETERROR(DRJSON_ERROR_TYPE_ERROR, "Querying @values of non-object type");
        o = drjson_make_obj_values(o);
        goto Ldispatch;
    Litems:
        if(o.kind != DRJSON_OBJECT) RETERROR(DRJSON_ERROR_TYPE_ERROR, "Querying @items of non-object type");
        o = drjson_make_obj_items(o);
        goto Ldispatch;
    Ldone:
        return o;
    #undef RETERROR
}

DRJSON_API
int64_t
drjson_len(const DrJsonContext* ctx, DrJsonValue v){
    switch(v.kind){
        case DRJSON_ARRAY:
        case DRJSON_ARRAY_VIEW:{
            const DrJsonArray* adata = ctx->arrays.data;
            const DrJsonArray* array = &adata[v.array_idx];
            return array->count;
        }
        case DRJSON_OBJECT:
        case DRJSON_OBJECT_KEYS:
        case DRJSON_OBJECT_VALUES:{
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            return object->count;
        }
        case DRJSON_OBJECT_ITEMS:{
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            return 2*object->count;
        }
        case DRJSON_STRING:{
            return v.atom->length;
        }
        default:
            return -1;
    }
}
DRJSON_API
DrJsonValue
drjson_get_by_index(const DrJsonContext* ctx, DrJsonValue v, int64_t idx){
    int64_t len = drjson_len(ctx, v);
    if(idx < 0) idx += len;
    size_t index  = idx < 0? (size_t)(idx + len) : (size_t)idx;
    switch(v.kind){
        case DRJSON_ARRAY:
        case DRJSON_ARRAY_VIEW:{
            const DrJsonArray* adata = ctx->arrays.data;
            const DrJsonArray* array = &adata[v.array_idx];
            if(array->count <= index)
                return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "out of bounds indexing");
            return array->array_items[index];
        }
        case DRJSON_OBJECT_KEYS:{
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            if(object->count <= index)
                return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "out of bounds indexing");
            size_t capacity = object->capacity;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, capacity);
            return drjson_atom_to_value(pairs[index].atom);
        }
        case DRJSON_OBJECT_VALUES:{
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            if(object->count <= index)
                return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "out of bounds indexing");
            size_t capacity = object->capacity;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, capacity);
            return pairs[index].value;
        }
        case DRJSON_OBJECT_ITEMS:{
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            size_t pidx = index/2;
            if(object->count <= pidx)
                return drjson_make_error(DRJSON_ERROR_INDEX_ERROR, "out of bounds indexing");
            size_t capacity = object->capacity;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, capacity);
            if(index & 1)
                return pairs[pidx].value;
            else
                return drjson_atom_to_value(pairs[pidx].atom);
        }
        default:
            return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "object does not support indexing by integer");
    }
}

DRJSON_API
int
drjson_array_set_by_index(const DrJsonContext* ctx, DrJsonValue a, int64_t idx, DrJsonValue value){
    if(a.kind != DRJSON_ARRAY) return 1;
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[a.array_idx];
    if(array->read_only) return 1;
    if(idx < 0) idx += array->count;
    if(idx < 0) return 1;
    if(idx >= array->count) return 1;
    array->array_items[idx] = value;
    return 0;
}

typedef struct DrJsonMemBuff DrJsonMemBuff;
struct DrJsonMemBuff {
    char* begin;
    char* end;
};

static
int mem_buff_write(void* p, const void* data, size_t len){
    DrJsonMemBuff* buff = p;
    if(buff->begin + len >= buff->end)
        return 1;
    drj_memcpy(buff->begin, data, len);
    buff->begin += len;
    return 0;
}

DRJSON_API
int
drjson_print_value_mem(const DrJsonContext* ctx, void* buff, size_t bufflen, DrJsonValue v, int indent, unsigned flags, size_t*_Nullable printed){
    DrJsonMemBuff membuff = {.begin = buff, .end = bufflen+(char*)buff};
    DrJsonTextWriter writer = {
        .up = &membuff,
        .write = mem_buff_write,
    };
    int result = drjson_print_value(ctx, &writer, v, indent, flags);
    if(!result && printed){
        *printed = membuff.begin - (char*)buff;
    }
    return result;
}

DRJSON_API
int
drjson_print_error_mem(void* buff, size_t bufflen, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v){
    DrJsonMemBuff membuff = {.begin = buff, .end = bufflen+(char*)buff};
    DrJsonTextWriter writer = {
        .up = &membuff,
        .write = mem_buff_write,
    };
    return drjson_print_error(&writer, filename, filename_len, line, column, v);
}

#ifndef DRJSON_NO_STDIO
static
int wrapped_fwrite(void* ud, const void* data, size_t length){
    return fwrite(data, 1, length, ud) != length;
}

DRJSON_API
#ifdef __clang__
__attribute__((__noinline__)) // clang is generating invalid code on windows without this
#endif
int
drjson_print_value_fp(const DrJsonContext* ctx, FILE* fp, DrJsonValue v, int indent, unsigned flags){
    DrJsonTextWriter writer = {
        .up = fp,
        .write = wrapped_fwrite,
    };
    return drjson_print_value(ctx, &writer, v, indent, flags);
}
DRJSON_API
#ifdef __clang__
__attribute__((__noinline__)) // probably ditto
#endif
int
drjson_print_error_fp(FILE* fp, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v){
    DrJsonTextWriter writer = {
        .up = fp,
        .write = wrapped_fwrite,
    };
    return drjson_print_error(&writer, filename, filename_len, line, column, v);
}
#endif

#ifndef DRJSON_NO_IO
#ifndef _WIN32
static
int
wrapped_write(void* restrict ud, const void* restrict data, size_t length){
    int fd = (int)(intptr_t)ud;
    return write(fd, data, length) != (ssize_t)length;
}
DRJSON_API
int
drjson_print_value_fd(const DrJsonContext* ctx, int fd, DrJsonValue v, int indent, unsigned flags){
    DrJsonTextWriter writer = {
        .up = (void*)(intptr_t)fd,
        .write = wrapped_write,
    };
    return drjson_print_value(ctx, &writer, v, indent, flags);
}

DRJSON_API
int
drjson_print_value_path(const DrJsonContext* ctx, const char* path, DrJsonValue v, int indent, unsigned flags){
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0) return fd;
    int err = drjson_print_value_fd(ctx, fd, v, indent, flags);
    close(fd);
    return err;
}
DRJSON_API
int
drjson_print_error_fd(int fd, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v){
    DrJsonTextWriter writer = {
        .up = (void*)(intptr_t)fd,
        .write = wrapped_write,
    };
    return drjson_print_error(&writer, filename, filename_len, line, column, v);
}
#else
static int
wrapped_write_file(void* restrict ud, const void* restrict data, size_t length){
    HANDLE hnd = ud;
    DWORD nwrit = 0;
    return WriteFile(hnd, data, (DWORD)length, &nwrit, NULL) != TRUE;
}
DRJSON_API
int
drjson_print_value_HANDLE(const DrJsonContext* ctx, void* hnd, DrJsonValue v, int indent, unsigned flags){
    DrJsonTextWriter writer = {
        .up = (void*)hnd,
        .write = wrapped_write_file,
    };
    return drjson_print_value(ctx, &writer, v, indent, flags);
}
DRJSON_API
int
drjson_print_value_path(const DrJsonContext* ctx, const char* path, DrJsonValue v, int indent, unsigned flags){
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if(h == INVALID_HANDLE_VALUE){
        return 1;
    }
    int err = drjson_print_value_HANDLE(ctx, h, v, indent, flags);
    CloseHandle(h);
    return err;
}
DRJSON_API
int
drjson_print_error_HANDLE(void* hnd, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v){
    DrJsonTextWriter writer = {
        .up = (void*)hnd,
        .write = wrapped_write_file,
    };
    return drjson_print_error(&writer, filename, filename_len, line, column, v);
}
#endif
#endif

#ifdef __wasm__
enum {DRJSON_BUFF_SIZE = 1024};
#else
enum {DRJSON_BUFF_SIZE = 1024*512};
#endif
typedef struct DrJsonBuffered DrJsonBuffered;
struct DrJsonBuffered {
    const DrJsonTextWriter* writer;
    size_t cursor;
    int errored;
    char buff[DRJSON_BUFF_SIZE];
};


static inline
void
drjson_print_value_inner(const DrJsonContext* ctx, DrJsonBuffered* restrict buffer, DrJsonValue v);

static inline
void
drjson_pretty_print_value_inner(const DrJsonContext*_Nullable ctx, DrJsonBuffered* restrict buffer, DrJsonValue v, int indent);

static inline
void
drjson_buff_flush(DrJsonBuffered* restrict buffer){
    if(!buffer->errored)
        buffer->errored = buffer->writer->write(buffer->writer->up, buffer->buff, buffer->cursor);
    buffer->cursor = 0;
}

static inline
void
drjson_buff_ensure_n(DrJsonBuffered* restrict buffer, size_t length){
    if(buffer->cursor + length > DRJSON_BUFF_SIZE){
        drjson_buff_flush(buffer);
    }
}

static inline
void
drjson_buff_write(DrJsonBuffered* restrict buffer, const char* restrict data, size_t length){
    drjson_buff_ensure_n(buffer, length);
    if(length >= DRJSON_BUFF_SIZE){
        if(!buffer->errored)
            buffer->errored = buffer->writer->write(buffer->writer->up, data, length);
        return;
    }
    drj_memcpy(buffer->buff+buffer->cursor, data, length);
    buffer->cursor += length;
}
static inline
void
drjson_buff_write_escaped(DrJsonBuffered* restrict buffer, const char* restrict data, size_t length){
    const char* prev = data;
    const char* end = data + length;
    const char* p = data;
    static const char ucodes[32][6] = {
        "\\u0000",
        "\\u0001",
        "\\u0002",
        "\\u0003",
        "\\u0004",
        "\\u0005",
        "\\u0006",
        "\\u0007",
        "\\u0008",
        "\\u0009",
        "\\u000a",
        "\\u000b",
        "\\u000c",
        "\\u000d",
        "\\u000e",
        "\\u000f",
        "\\u0010",
        "\\u0011",
        "\\u0012",
        "\\u0013",
        "\\u0014",
        "\\u0015",
        "\\u0016",
        "\\u0017",
        "\\u0018",
        "\\u0019",
        "\\u001a",
        "\\u001b",
        "\\u001c",
        "\\u001d",
        "\\u001e",
        "\\u001f",
    };
#define FLUSH() \
    do { \
        if(prev < p) { \
            drjson_buff_write(buffer, prev, p-prev); \
        } \
        prev = p+1; \
    }while(0)

    for(;p != end;p++){
        switch(*p){
            // 0x0 through 0x1f (0 through 31) have to all be
            // escaped with the 6 character sequence of \u00xx
            // Why on god's green earth did they force utf16 escapes?

            // gnu case ranges are nicer, but nonstandard
            // just spell them all out.
            case  0: case  1: case  2: case  3: case  4:
            // 8 is '\b', 9 is '\t'
            case  5: case  6: case  7:
            // 10 is '\n', 12 is '\f', 13 is '\r'
                     case 11:                   case 14:
            case 15: case 16: case 17: case 18: case 19:
            case 20: case 21: case 22: case 23: case 24:
            case 25: case 26: case 27: case 28: case 29:
            case 30: case 31:
                FLUSH();
                drjson_buff_write(buffer, ucodes[(unsigned char)*p], 6);
                break;
            case '"':
                FLUSH();
                drjson_buff_write(buffer, "\\\"", 2);
                break;
            case '\\':
                FLUSH();
                drjson_buff_write(buffer, "\\\\", 2);
                break;
            case '\b':
                FLUSH();
                drjson_buff_write(buffer, "\\b", 2);
                break;
            case '\f':
                FLUSH();
                drjson_buff_write(buffer, "\\f", 2);
                break;
            case '\n':
                FLUSH();
                drjson_buff_write(buffer, "\\n", 2);
                break;
            case '\r':
                FLUSH();
                drjson_buff_write(buffer, "\\r", 2);
                break;
            case '\t':
                FLUSH();
                drjson_buff_write(buffer, "\\t", 2);
                break;
            // Other characters are allowed through as is
            // (presumably utf-8).
            default:
                continue;
        }
    }
    FLUSH();
    #undef FLUSH
}

#define drjson_buff_write_lit(b, lit) drjson_buff_write(b, "" lit, sizeof(lit)-1)
static inline
void
drjson_buff_putc(DrJsonBuffered* restrict buffer, char c){
    drjson_buff_ensure_n(buffer, 1);
    buffer->buff[buffer->cursor] = c;
    buffer->cursor += 1;
}

DRJSON_API
int
drjson_print_value(const DrJsonContext* ctx, const DrJsonTextWriter* restrict writer, DrJsonValue v, int indent, unsigned flags){
    DrJsonBuffered buffer;
    buffer.cursor = 0;
    buffer.writer = writer;
    buffer.errored = 0;
    if(flags & DRJSON_PRINT_PRETTY){
        for(int i = 0; i < indent; i++)
            drjson_buff_putc(&buffer, ' ');
        drjson_pretty_print_value_inner(ctx, &buffer, v, indent);
    }
    else
        drjson_print_value_inner(ctx, &buffer, v);
    if(flags & DRJSON_PRINT_APPEND_NEWLINE)
        drjson_buff_putc(&buffer, '\n');
    if(flags & DRJSON_PRINT_APPEND_ZERO)
        drjson_buff_putc(&buffer, '\0');
    if(buffer.cursor)
        drjson_buff_flush(&buffer);
    return buffer.errored;
}

DRJSON_API
int
drjson_print_error(const DrJsonTextWriter* restrict writer, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v){
#if 0
    size_t line = 0;
    size_t column = 0;
    // just do it the slow way, whatever.
    for(const char* c = ctx->begin; c != ctx->cursor; c++){
        switch(*c){
            case '\n':
                line++;
                column = 0;
                break;
            default:
                column++;
                break;
        }
    }
#endif
    DrJsonBuffered buffer;
    buffer.cursor = 0;
    buffer.writer = writer;
    buffer.errored = 0;
    if(filename_len){
        drjson_buff_write(&buffer, filename, filename_len);
        drjson_buff_putc(&buffer, ':');
    }

    drjson_buff_ensure_n(&buffer, 20);
    buffer.cursor += drjson_uint64_to_ascii(buffer.buff+buffer.cursor, line+1);
    drjson_buff_putc(&buffer, ':');
    buffer.cursor += drjson_uint64_to_ascii(buffer.buff+buffer.cursor, column+1);
    drjson_buff_putc(&buffer, ':');
    drjson_buff_putc(&buffer, ' ');
    drjson_pretty_print_value_inner(NULL, &buffer, v, 0);
    drjson_buff_putc(&buffer, '\n');
    if(buffer.cursor)
        drjson_buff_flush(&buffer);
    return buffer.errored;
}
static inline
const char*
drjson_get_error_name(DrJsonValue);

static inline
size_t
drjson_get_error_name_length(DrJsonValue);

static inline
void
drjson_print_value_inner(const DrJsonContext* ctx, DrJsonBuffered* restrict buffer, DrJsonValue v){
    if(buffer->errored) return;
    switch(v.kind){
        case DRJSON_NUMBER:{
            drjson_buff_ensure_n(buffer, 24);
            int len = fpconv_dtoa(v.number, buffer->buff+buffer->cursor);
            buffer->cursor += len;
        }break;
        case DRJSON_INTEGER:
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_int64_to_ascii(buffer->buff+buffer->cursor, v.integer);
            break;
        case DRJSON_UINTEGER:
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_uint64_to_ascii(buffer->buff+buffer->cursor, v.uinteger);
            break;
        case DRJSON_STRING:{
            drjson_buff_putc(buffer, '"');
            if(v.string.needs_escape)
                drjson_buff_write_escaped(buffer, v.atom->data, v.atom->length);
            else
                drjson_buff_write(buffer, v.atom->data, v.atom->length);
            drjson_buff_putc(buffer, '"');
        }break;
        case DRJSON_ARRAY_VIEW:
        case DRJSON_ARRAY:{
            drjson_buff_putc(buffer, '[');
            const DrJsonArray* adata = ctx->arrays.data;
            const DrJsonArray* array = &adata[v.array_idx];
            for(size_t i = 0; i < array->count; i++){
                drjson_print_value_inner(ctx, buffer, array->array_items[i]);
                if(i != array->count-1u)
                    drjson_buff_putc(buffer, ',');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT:{
            drjson_buff_putc(buffer, '{');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                newlined = 1;
                Atom s = o->atom;
                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, s->data, s->length);
                drjson_buff_putc(buffer, '"');
                drjson_buff_putc(buffer, ':');
                drjson_print_value_inner(ctx, buffer, o->value);
            }
            drjson_buff_putc(buffer, '}');
        }break;
        case DRJSON_OBJECT_KEYS:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                newlined = 1;
                Atom s = o->atom;
                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, s->data, s->length);
                drjson_buff_putc(buffer, '"');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT_VALUES:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                newlined = 1;
                drjson_print_value_inner(ctx, buffer, o->value);
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT_ITEMS:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                newlined = 1;
                Atom s = o->atom;
                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, s->data, s->length);
                drjson_buff_putc(buffer, '"');
                drjson_buff_putc(buffer, ',');
                drjson_print_value_inner(ctx, buffer, o->value);
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_NULL:
            drjson_buff_write_lit(buffer, "null");
            break;
        case DRJSON_BOOL:
            if(v.boolean)
                drjson_buff_write_lit(buffer, "true");
            else
                drjson_buff_write_lit(buffer, "false");
            break;
        case DRJSON_ERROR:
            drjson_buff_write_lit(buffer, "Error: ");
            drjson_buff_write(buffer, drjson_get_error_name(v), drjson_get_error_name_length(v));
            drjson_buff_write_lit(buffer, "(Code ");
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_int64_to_ascii(buffer->buff+buffer->cursor, v.error.code);
            drjson_buff_write_lit(buffer, "): ");
            drjson_buff_write(buffer, v.err_mess, v.error.len);
            break;
    }
}

static inline
void
drjson_pretty_print_value_inner(const DrJsonContext*_Nullable ctx, DrJsonBuffered* restrict buffer, DrJsonValue v, int indent){
    if(buffer->errored) return;
    switch(v.kind){
        case DRJSON_NUMBER:{
            drjson_buff_ensure_n(buffer, 24);
            int len = fpconv_dtoa(v.number, buffer->buff+buffer->cursor);
            buffer->cursor += len;
        }break;
        case DRJSON_INTEGER:
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_int64_to_ascii(buffer->buff+buffer->cursor, v.integer);
            break;
        case DRJSON_UINTEGER:
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_uint64_to_ascii(buffer->buff+buffer->cursor, v.uinteger);
            break;
        case DRJSON_STRING:{
            drjson_buff_putc(buffer, '"');
            if(v.string.needs_escape)
                drjson_buff_write_escaped(buffer, v.atom->data, v.atom->length);
            else
                drjson_buff_write(buffer, v.atom->data, v.atom->length);
            drjson_buff_putc(buffer, '"');
        }break;
        case DRJSON_ARRAY_VIEW:
        case DRJSON_ARRAY:{
            drjson_buff_putc(buffer, '[');
            const DrJsonArray* adata = ctx->arrays.data;
            const DrJsonArray* array = &adata[v.array_idx];
            int newlined = 0;
            if(array->count && !drjson_is_numeric(array->array_items[0]) && array->array_items[0].kind != DRJSON_STRING){
                drjson_buff_putc(buffer, '\n');
                newlined = 1;
            }
            for(size_t i = 0; i < array->count; i++){
                if(newlined)
                    for(int ii = 0; ii < indent+2; ii++)
                        drjson_buff_putc(buffer, ' ');
                drjson_pretty_print_value_inner(ctx, buffer, array->array_items[i], indent+2);
                if(i != array->count-1u)
                    drjson_buff_putc(buffer, ',');
                if(newlined)
                    drjson_buff_putc(buffer, '\n');
            }
            if(newlined){
                for(int i = 0; i < indent; i++)
                    drjson_buff_putc(buffer, ' ');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT:{
            drjson_buff_putc(buffer, '{');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                drjson_buff_putc(buffer, '\n');
                newlined = 1;
                for(int ind = 0; ind < indent+2; ind++)
                    drjson_buff_putc(buffer, ' ');

                Atom s = o->atom;
                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, s->data, s->length);
                drjson_buff_putc(buffer, '"');
                drjson_buff_putc(buffer, ':');
                drjson_buff_putc(buffer, ' ');
                drjson_pretty_print_value_inner(ctx, buffer, o->value, indent+2);
            }
            if(newlined) {
                drjson_buff_putc(buffer, '\n');
                for(int i = 0; i < indent; i++)
                    drjson_buff_putc(buffer, ' ');
            }
            drjson_buff_putc(buffer, '}');
        }break;
        case DRJSON_OBJECT_KEYS:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                drjson_buff_putc(buffer, '\n');
                newlined = 1;
                for(int ind = 0; ind < indent+2; ind++)
                    drjson_buff_putc(buffer, ' ');

                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, o->atom->data, o->atom->length);
                drjson_buff_putc(buffer, '"');
            }
            if(newlined) {
                drjson_buff_putc(buffer, '\n');
                for(int i = 0; i < indent; i++)
                    drjson_buff_putc(buffer, ' ');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT_VALUES:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                drjson_buff_putc(buffer, '\n');
                newlined = 1;
                for(int ind = 0; ind < indent+2; ind++)
                    drjson_buff_putc(buffer, ' ');
                drjson_pretty_print_value_inner(ctx, buffer, o->value, indent+2);
            }
            if(newlined) {
                drjson_buff_putc(buffer, '\n');
                for(int i = 0; i < indent; i++)
                    drjson_buff_putc(buffer, ' ');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_OBJECT_ITEMS:{
            drjson_buff_putc(buffer, '[');
            const DrJsonObject* odata = ctx->objects.data;
            const DrJsonObject* object = &odata[v.object_idx];
            int newlined = 0;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++){
                DrJsonObjectPair* o = &pairs[i];
                if(newlined)
                    drjson_buff_putc(buffer, ',');
                drjson_buff_putc(buffer, '\n');
                newlined = 1;
                for(int ind = 0; ind < indent+2; ind++)
                    drjson_buff_putc(buffer, ' ');

                drjson_buff_putc(buffer, '"');
                drjson_buff_write(buffer, o->atom->data, o->atom->length);
                drjson_buff_putc(buffer, '"');
                drjson_buff_putc(buffer, ',');
                drjson_buff_putc(buffer, ' ');
                drjson_pretty_print_value_inner(ctx, buffer, o->value, indent+2);
            }
            if(newlined) {
                drjson_buff_putc(buffer, '\n');
                for(int i = 0; i < indent; i++)
                    drjson_buff_putc(buffer, ' ');
            }
            drjson_buff_putc(buffer, ']');
        }break;
        case DRJSON_NULL:
            drjson_buff_write_lit(buffer, "null");
            break;
        case DRJSON_BOOL:
            if(v.boolean)
                drjson_buff_write_lit(buffer, "true");
            else
                drjson_buff_write_lit(buffer, "false");
            break;
        case DRJSON_ERROR:
            drjson_buff_write_lit(buffer, "Error: ");
            drjson_buff_write(buffer, drjson_get_error_name(v), drjson_get_error_name_length(v));
            drjson_buff_write_lit(buffer, "(Code ");
            drjson_buff_ensure_n(buffer, 20);
            buffer->cursor += drjson_int64_to_ascii(buffer->buff+buffer->cursor, v.error.code);
            drjson_buff_write_lit(buffer, "): ");
            drjson_buff_write(buffer, v.err_mess, v.error.len);
            break;
    }
}

// return 1 - error
// return 2 - no need to escape
static
int
drjson_escape_string2(Allocator allocator, const char* restrict unescaped, size_t length, char *_Nullable restrict *_Nonnull restrict outstring, size_t* restrict outlength){
    if(!length) return 1;
    const char* const hex = "0123456789abcdef";
    size_t i = 0;
    for(; i < length; i++){
        switch(unescaped[i]){
            // 0x0 through 0x1f (0 through 31) have to all be
            // escaped with the 6 character sequence of \u00xx
            // Why on god's green earth did they force utf16 escapes?

            // gnu case ranges are nicer, but nonstandard
            // just spell them all out.
            case  0: case  1: case  2: case  3: case  4:
            // 8 is '\b', 9 is '\t'
            case  5: case  6: case  7:
            // 10 is '\n', 12 is '\f', 13 is '\r'
                     case 11:                   case 14:
            case 15: case 16: case 17: case 18: case 19:
            case 20: case 21: case 22: case 23: case 24:
            case 25: case 26: case 27: case 28: case 29:
            case 30: case 31:
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                goto needs_alloc;
            // Other characters are allowed through as is
            // (presumably utf-8).
            default:
                continue;
        }
    }
    return 2; // no escape
    needs_alloc:;

    // reserve 2 characters of output for every 2 characters of input.
    // This is enough space for all common json strings (the bytes that must
    // be escaped are rare).
    // We fix that up if we actually hit them.
    size_t allocated_size = length * 2;
    char* s = Allocator_alloc(allocator, allocated_size);
    if(!s) return 1;
    if(i) drj_memcpy(s, unescaped, i);
    size_t cursor = i;
    for(; i < length; i++){
        switch(unescaped[i]){
            // 0x0 through 0x1f (0 through 31) have to all be
            // escaped with the 6 character sequence of \u00xx
            // Why on god's green earth did they force utf16 escapes?

            // gnu case ranges are nicer, but nonstandard
            // just spell them all out.
            case  0: case  1: case  2: case  3: case  4:
            // 8 is '\b', 9 is '\t'
            case  5: case  6: case  7:
            // 10 is '\n', 12 is '\f', 13 is '\r'
                     case 11:                   case 14:
            case 15: case 16: case 17: case 18: case 19:
            case 20: case 21: case 22: case 23: case 24:
            case 25: case 26: case 27: case 28: case 29:
            case 30: case 31:
                // These are rare, so only reserve more space when we actually hit them.
                {
                    size_t realloc_size = allocated_size+4;
                    char* news = Allocator_realloc(allocator, s, allocated_size, realloc_size);
                    if(!news){
                        Allocator_free(allocator, s, allocated_size);
                        return 1;
                    }
                    allocated_size = realloc_size;
                    s = news;
                }
                s[cursor++] = '\\';
                s[cursor++] = 'u';
                s[cursor++] = '0';
                s[cursor++] = '0';
                s[cursor++] = hex[(unescaped[i] & 0xf0)>>4];
                s[cursor++] = hex[(unescaped[i] & 0xf)];
                break;
            case '"':
                s[cursor++] = '\\';
                s[cursor++] = '"';
                break;
            case '\\':
                s[cursor++] = '\\';
                s[cursor++] = '\\';
                break;
            case '\b':
                s[cursor++] = '\\';
                s[cursor++] = 'b';
                break;
            case '\f':
                s[cursor++] = '\\';
                s[cursor++] = 'f';
                break;
            case '\n':
                s[cursor++] = '\\';
                s[cursor++] = 'n';
                break;
            case '\r':
                s[cursor++] = '\\';
                s[cursor++] = 'r';
                break;
            case '\t':
                s[cursor++] = '\\';
                s[cursor++] = 't';
                break;
            // Other characters are allowed through as is
            // (presumably utf-8).
            default:
                s[cursor++] = unescaped[i];
                break;
        }
    }
    // shrink to the size we actually used
    char* news = Allocator_realloc(allocator, s, allocated_size, cursor);
    if(!news){
        Allocator_free(allocator, s, allocated_size);
        return 1;
    }
    *outstring = news;
    *outlength = cursor;
    return 0;
}

#if 0
DRJSON_API
int
drjson_escape_string(DrJsonContext* ctx, const char* restrict unescaped, size_t length, DrJsonAtom* outatom){
    if(!outatom) return 1;
    if(length >= ATOM_MAX_LEN)
        return 1;
    if(!length){
        int err = drj_atomize_str(&ctx->atoms, ctx->allocator, "", 0, 0, outatom);
        return err;
    }
    char* tmp;
    size_t tmp_length;
    int err = drjson_escape_string2(ctx->allocator, unescaped, length, &tmp, &tmp_length);
    if(err == 1) return err;
    // String doesn't need to be escaped, make a copy in the atom table.
    const _Bool copy = 1;
    if(err == 2) return drj_atomize_str(&ctx->atoms, ctx->allocator, unescaped, (uint32_t)length, copy, outatom);
    if(tmp_length >= ATOM_MAX_LEN)
        err = 1;
    else {
        // Always copy as we'd otherwise have to add a function that tells if it was interned or not.
        // so we know whether to free.
        // We could do that.
        err = drj_atomize_str(&ctx->atoms, ctx->allocator, tmp, (uint32_t)tmp_length, copy, outatom);
    }
    drj_free(ctx, tmp, tmp_length);
    return err;
}
#endif

#if 0
DRJSON_API
int
drjson_unescape_string(const DrJsonAllocator* restrict allocator, const char* restrict unescaped, size_t length, char*_Nullable restrict *_Nonnull restrict outstring, size_t* restrict outlength){
    if(!length) return 1;
    // TODO
    return 1;
}
#endif

DRJSON_API
void
drjson_get_line_column(const DrJsonParseContext* ctx, size_t* line, size_t* column){
    size_t lin = 0;
    size_t col = 0;
    // just do it the slow way, whatever.
    for(const char* c = ctx->begin; c != ctx->cursor; c++){
        switch(*c){
            case '\n':
                lin++;
                col = 0;
                break;
            default:
                col++;
                break;
        }
    }
    *line = lin;
    *column = col;
}

DRJSON_API
void
drjson_ctx_free_all(DrJsonContext* ctx){
    if(0 && Allocator_supports_free_all(ctx->allocator)){
        Allocator_free_all(ctx->allocator);
        return;
    }
    // Free each object
    for(size_t i = 0; i < ctx->objects.count; i++){
        DrJsonObject* odata = ctx->objects.data;
        DrJsonObject* o = &odata[i];
        if(o->capacity)
            drj_free(ctx, o->object_items, drjson_size_for_object_of_length(o->capacity));
    }
    // Then the objects array
    if(ctx->objects.data)
        drj_free(ctx, ctx->objects.data, ctx->objects.capacity*sizeof (DrJsonObject));
    // Free interned objects
    if(ctx->interned_objects.capacity)
        drj_free(ctx, ctx->interned_objects.data, ctx->interned_objects.capacity*sizeof(uint32_t)*4);

    // Free each array
    for(size_t i = 0; i < ctx->arrays.count; i++){
        DrJsonArray* adata = ctx->arrays.data;
        DrJsonArray* a = &adata[i];
        if(a->capacity)
            drj_free(ctx, a->array_items,a->capacity*sizeof *a->array_items);
    }
    // Free arrays array
    if(ctx->arrays.data)
        drj_free(ctx, ctx->arrays.data, ctx->arrays.capacity*sizeof(DrJsonArray));

    // Free interned arrays
    if(ctx->interned_arrays.capacity)
        drj_free(ctx, ctx->interned_arrays.data, ctx->interned_arrays.capacity*sizeof(uint32_t)*4);

    #ifndef DRJ_DONT_FREE_CTX
        drj_free(ctx, ctx, sizeof *ctx);
    #endif
}


DRJSON_API
DrJsonValue
drjson_make_string(DrJsonContext* ctx, const char* s, size_t length){
    Atom atom = AT_atomize(ctx->atom_table, s, length);
    if(!atom) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom");
    return drjson_atom_to_value(atom);
}

static const char*_Nonnull const DrJsonKindNames[] = {
    [DRJSON_ERROR]         = "error",
    [DRJSON_NUMBER]        = "number",
    [DRJSON_INTEGER]       = "integer",
    [DRJSON_UINTEGER]      = "uinteger",
    [DRJSON_STRING]        = "string",
    [DRJSON_ARRAY]         = "array",
    [DRJSON_OBJECT]        = "object",
    [DRJSON_NULL]          = "null",
    [DRJSON_BOOL]          = "bool",
    [DRJSON_ARRAY_VIEW]    = "array view",
    [DRJSON_OBJECT_KEYS]   = "object keys",
    [DRJSON_OBJECT_VALUES] = "object values",
    [DRJSON_OBJECT_ITEMS]  = "object items",
};

static const size_t DrJsonKindNameLengths[] = {
    [DRJSON_ERROR]         = sizeof("error")-1,
    [DRJSON_NUMBER]        = sizeof("number")-1,
    [DRJSON_INTEGER]       = sizeof("integer")-1,
    [DRJSON_UINTEGER]      = sizeof("uinteger")-1,
    [DRJSON_STRING]        = sizeof("string")-1,
    [DRJSON_ARRAY]         = sizeof("array")-1,
    [DRJSON_OBJECT]        = sizeof("object")-1,
    [DRJSON_NULL]          = sizeof("null")-1,
    [DRJSON_BOOL]          = sizeof("bool")-1,
    [DRJSON_ARRAY_VIEW]    = sizeof("array view")-1,
    [DRJSON_OBJECT_KEYS]   = sizeof("object keys")-1,
    [DRJSON_OBJECT_VALUES] = sizeof("object values")-1,
    [DRJSON_OBJECT_ITEMS]  = sizeof("object items")-1,
};

static const char*_Nonnull const DrJsonErrorNames[] = {
    [DRJSON_ERROR_NONE]           = "No error",
    [DRJSON_ERROR_UNEXPECTED_EOF] = "Unexpected End of Input",
    [DRJSON_ERROR_ALLOC_FAILURE]  = "Allocation Failure",
    [DRJSON_ERROR_MISSING_KEY]    = "Missing Key",
    [DRJSON_ERROR_INDEX_ERROR]    = "Index Error",
    [DRJSON_ERROR_INVALID_CHAR]   = "Invalid Char",
    [DRJSON_ERROR_INVALID_VALUE]  = "Invalid Value",
    [DRJSON_ERROR_TOO_DEEP]       = "Too Many Levels of Nesting",
    [DRJSON_ERROR_TYPE_ERROR]     = "Invalid type for operation",
    [DRJSON_ERROR_INVALID_ERROR]  = "Error is Invalid",
};

static const size_t DrJsonErrorNameLengths[] = {
    [DRJSON_ERROR_NONE]           = sizeof("No error")-1,
    [DRJSON_ERROR_UNEXPECTED_EOF] = sizeof("Unexpected End of Input")-1,
    [DRJSON_ERROR_ALLOC_FAILURE]  = sizeof("Allocation Failure")-1,
    [DRJSON_ERROR_MISSING_KEY]    = sizeof("Missing Key")-1,
    [DRJSON_ERROR_INDEX_ERROR]    = sizeof("Index Error")-1,
    [DRJSON_ERROR_INVALID_CHAR]   = sizeof("Invalid Char")-1,
    [DRJSON_ERROR_INVALID_VALUE]  = sizeof("Invalid Value")-1,
    [DRJSON_ERROR_TOO_DEEP]       = sizeof("Too Many Levels of Nesting")-1,
    [DRJSON_ERROR_TYPE_ERROR]     = sizeof("Invalid type for operation")-1,
    [DRJSON_ERROR_INVALID_ERROR]  = sizeof("Error is Invalid")-1,
};

static inline
const char*
drjson_get_error_name(DrJsonValue v){
    return DrJsonErrorNames[v.error.code];
}

static inline
size_t
drjson_get_error_name_length(DrJsonValue v){
    return DrJsonErrorNameLengths[v.error.code];
}

DRJSON_API
const char*
drjson_error_name(DrJsonErrorCode code, size_t*_Nullable length){
    if(code < 0  || code > DRJSON_ERROR_INVALID_ERROR)
        code = DRJSON_ERROR_INVALID_ERROR;
    if(length) *length = DrJsonErrorNameLengths[code];
    return DrJsonErrorNames[code];
}

DRJSON_API
const char*
drjson_kind_name(DrJsonKind kind, size_t*_Nullable length){
    if(kind < 0 || kind > DRJSON_OBJECT_ITEMS)
        kind = DRJSON_ERROR;
    if(length) *length = DrJsonKindNameLengths[kind];
    return DrJsonKindNames[kind];
}

static
void
drj_mark(DrJsonContext* ctx, DrJsonValue v){
#ifdef DRJ_DEBUG
    fprintf(stderr, "mark\n");
#endif
    switch(v.kind){
        case DRJSON_OBJECT:
        case DRJSON_OBJECT_KEYS:
        case DRJSON_OBJECT_ITEMS:
        case DRJSON_OBJECT_VALUES:{
            DrJsonObject* odata = ctx->objects.data;
            DrJsonObject* object = &odata[v.object_idx];
            if(!object->capacity) return;
            if(object->marked) return;
            object->marked = 1;
            DrJsonObjectPair* pairs = drj_obj_get_pairs(object->object_items, object->capacity);
            for(size_t i = 0; i < object->count; i++)
                drj_mark(ctx, pairs[i].value);
        }break;

        case DRJSON_ARRAY:
        case DRJSON_ARRAY_VIEW:{
            DrJsonArray* adata = ctx->arrays.data;
            DrJsonArray* array = &adata[v.array_idx];
            if(!array->capacity) return;
            if(array->marked) return;
            array->marked = 1;
            for(size_t i = 0; i < array->count; i++)
                drj_mark(ctx, array->array_items[i]);
        }break;
        default:
            return;
    }
}

#define DRJ_FREE_IDX (UINT32_MAX-1)
static
void
drj_free_obj(DrJsonContext* ctx, DrJsonObject* o){
    size_t o_idx = o - ctx->objects.data;
    #ifdef DRJ_DEBUG
    fprintf(stderr, "Freeing object: %p (%zu)\n", (void*)o, o_idx);
    assert(!o->freed);
    o->freed = 1;
    #endif
    if(o->read_only){
        o->read_only = 0;
        uint32_t hash = hash_align8(o->object_items, o->count*sizeof(DrJsonObjectPair));
        uint32_t idx = fast_reduce32(hash, (uint32_t)ctx->interned_objects.capacity*2);
        uint32_t* idxes = (uint32_t*)(ctx->interned_objects.data+ctx->interned_objects.capacity);
        DrjHashIdx* hi = ctx->interned_objects.data;
        #ifdef DRJ_DEBUG
        uint32_t found_idx = -1;
        for(size_t i = 0; i < 2*ctx->interned_objects.capacity; i++){
            if(hi[i].idx == (uint32_t)o_idx){
                found_idx = (uint32_t)i;
                break;
            }
        }
        assert(found_idx != (uint32_t)-1);
        #endif
        for(;;){
            uint32_t i = idxes[idx];
            assert(i != UINT32_MAX);
            if(i != UINT32_MAX && hi[i].idx == (uint32_t)o_idx){
                #ifdef DRJ_DEBUG
                assert(i == found_idx);
                #endif
                assert(hi[i].hash == hash);
                hi[i].idx = DRJ_FREE_IDX;
                break;
            }
            idx++;
            if(idx == (uint32_t)(2*ctx->interned_objects.capacity)) idx = 0;
        }
    }
    if(o->capacity){
        drj_free(ctx, o->object_items, drjson_size_for_object_of_length(o->capacity));
        o->object_items = NULL;
        o->capacity = 0;
    }
    if(o_idx == ctx->objects.count-1){
        ctx->objects.count--;
    }
    else {
        if(o_idx <= UINT32_MAX/2 && o_idx){
            o->count = (uint32_t)ctx->objects.free_object;
            ctx->objects.free_object = o_idx;
        }
    }
}

static
void
drj_free_array(DrJsonContext* ctx, DrJsonArray* a){
    size_t a_idx = a - ctx->arrays.data;
#ifdef DRJ_DEBUG
    fprintf(stderr, "Freeing array: %p (%zu)\n", (void*)a, a_idx);
    assert(!a->freed);
    a->freed = 1;
#endif
    if(a->read_only){
        a->read_only = 0;
        // fprintf(stderr, "Freeing interned array: %p (%zu)\n", a, a_idx);
        uint32_t hash = hash_align8(a->array_items, a->count*sizeof *a->array_items);
        uint32_t idx = fast_reduce32(hash, (uint32_t)ctx->interned_arrays.capacity*2);
        uint32_t* idxes = (uint32_t*)(ctx->interned_arrays.data+ctx->interned_arrays.capacity);
        DrjHashIdx* hi = ctx->interned_arrays.data;
        #ifdef DRJ_DEBUG
        uint32_t found_idx = -1;
        for(size_t i = 0; i < 2*ctx->interned_arrays.capacity; i++){
            if(hi[i].idx == (uint32_t)a_idx){
                found_idx = (uint32_t)i;
                break;
            }
        }
        assert(found_idx != (uint32_t)-1);
        #endif
        for(;;){
            uint32_t i = idxes[idx];
            assert(i != UINT32_MAX);
            if(i != UINT32_MAX && hi[i].idx == (uint32_t)a_idx){
                #ifdef DRJ_DEBUG
                assert(i == found_idx);
                #endif
                assert(hi[i].hash == hash);
                hi[i].idx = DRJ_FREE_IDX;
                break;
            }
            idx++;
            if(idx == (uint32_t)(2*ctx->interned_arrays.capacity)) idx = 0;
        }
    }
    if(a->capacity){
        drj_free(ctx, a->array_items, a->capacity*sizeof *a->array_items);
        a->array_items = NULL;
        a->capacity = 0;
    }
    if(a_idx == ctx->arrays.count-1){
        ctx->arrays.count--;
    }
    else {
        if(a_idx <= UINT32_MAX/2 && a_idx){
            a->count = (uint32_t)ctx->arrays.free_array;
            ctx->arrays.free_array = a_idx;
        }
    }
}
static
void
drj_sweep(DrJsonContext* ctx){
#ifdef DRJ_DEBUG
    fprintf(stderr, "sweep\n");
#endif
    {
        DrJsonObject* odata = ctx->objects.data;
        for(size_t i = ctx->objects.count; i--;){
            DrJsonObject* o = &odata[i];
            if(!o->capacity) continue;
            if(o->marked){
                o->marked = 0;
                continue;
            }
            drj_free_obj(ctx, o);
        }
    }
    {
        DrJsonArray* adata = ctx->arrays.data;
        for(size_t i = ctx->arrays.count; i--;){
            DrJsonArray* a = &adata[i];
            if(!a->capacity) continue;
            if(a->marked){
                a->marked = 0;
                continue;
            }
            drj_free_array(ctx, a);

        }
    }
}

DRJSON_API
int
drjson_gc(DrJsonContext* ctx, const DrJsonValue* roots, size_t rootcount){
#ifdef DRJ_DEBUG
    fprintf(stderr, "gc\n");
#endif
    for(size_t i = 0; i < rootcount; i++)
        drj_mark(ctx, roots[i]);
    drj_sweep(ctx);
    return 0;
}

static
DrJsonValue
drj_dupe_array_ronly(DrJsonContext* ctx, DrJsonValue src_val){
    ssize_t new_idx = alloc_array(ctx);
    if(new_idx < 0) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom when duping array");
    assert(src_val.kind == DRJSON_ARRAY);
    DrJsonArray* src = &ctx->arrays.data[src_val.array_idx];
    DrJsonArray* dst = &ctx->arrays.data[new_idx];
    uint32_t cap = src->count;
    DrJsonValue* items = drj_alloc(ctx, cap * sizeof *items);
    if(!items) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom when duping array");
    *dst = (DrJsonArray){
        .count = cap,
        .capacity = cap,
        .array_items = items,
        .marked = 0,
        .read_only = 1,
    };
    drj_memcpy(items, src->array_items, cap*sizeof *items);
    return (DrJsonValue){.kind=DRJSON_ARRAY, .array_idx=new_idx};
}

static
DrJsonValue
drj_intern_array(DrJsonContext* ctx, DrJsonValue val, _Bool consume){
    assert(val.kind == DRJSON_ARRAY);
    DrJsonArray* adata = ctx->arrays.data;
    DrJsonArray* array = &adata[val.array_idx];
    if(array->read_only) return val;
    size_t count = array->count;
    DrJsonValue* items = array->array_items;
    for(size_t i = 0; i < count; i++){
        if(!drj_is_ro(ctx, items[i]))
            return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "All values of array must be read only to be interned");
    }
    // assume we're going to insert for purpose of checking capacity.
    if(ctx->interned_arrays.count == ctx->interned_arrays.capacity){
        size_t old_cap = ctx->interned_arrays.capacity;
        size_t new_cap = old_cap?old_cap*2:16;
        size_t old_size = sizeof(DrjHashIdx)*old_cap + sizeof(uint32_t)*2*old_cap;
        size_t new_size = sizeof(DrjHashIdx)*new_cap + sizeof(uint32_t)*2*new_cap;
        void* data = drj_realloc(ctx, ctx->interned_arrays.data, old_size, new_size);
        if(!data) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "Failed to allocate while interning array");
        DrjHashIdx* hi = data;
        uint32_t* idxes = (uint32_t*)((char*)data+sizeof(DrjHashIdx)*new_cap);
        drj_memset(idxes, 0xff, sizeof(uint32_t)*2*new_cap);
        size_t acount = 0;
        for(size_t i = 0; i < ctx->interned_arrays.count; i++){
            if(hi[i].idx == DRJ_FREE_IDX) continue;
            uint32_t hash = hi[i].hash;
            uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap*2);
            while(idxes[idx] != UINT32_MAX){
                idx++;
                if(idx == new_cap*2) idx = 0;
            }
            if(i != acount) hi[acount] = hi[i];
            idxes[idx] = (uint32_t)acount;
            acount++;
        }
        ctx->interned_arrays.capacity = new_cap;
        ctx->interned_arrays.data = data;
        ctx->interned_arrays.count = acount;
    }
    uint32_t hash = hash_align8(items, count * sizeof *items);
    uint32_t* idxes = (uint32_t*)(ctx->interned_arrays.data+ctx->interned_arrays.capacity);
    uint32_t idx = fast_reduce32(hash, (uint32_t)ctx->interned_arrays.capacity*2);
    DrjHashIdx* hi = ctx->interned_arrays.data;
    uint32_t first_free = -1; _Bool found_free = 0;
    for(;;){
        uint32_t i = idxes[idx];
        if(i == UINT32_MAX){
            if(found_free) idx = first_free;
            DrJsonValue cpy_val = consume?val:drj_dupe_array_ronly(ctx, val);
            if(consume)array->read_only = 1;
            if(cpy_val.kind == DRJSON_ERROR) return cpy_val;
            idxes[idx] = (uint32_t)ctx->interned_arrays.count;
            hi[ctx->interned_arrays.count] = (DrjHashIdx){.hash=hash, .idx=(uint32_t)cpy_val.array_idx};
            ctx->interned_arrays.count++;
            return cpy_val;
        }
        else if((hi[i].hash == hash && hi[i].idx == DRJ_FREE_IDX)){
            found_free = 1;
            first_free = idx;
        }
        else if(hi[i].hash == hash){
            uint32_t a_idx = hi[i].idx;
            DrJsonArray* a = &ctx->arrays.data[a_idx];
            if(a->count == array->count){
                if(!a->count || memcmp(a->array_items, array->array_items, a->count*sizeof *a->array_items) == 0){
                    if(consume) drj_free_array(ctx, array);
                    return (DrJsonValue){.kind=DRJSON_ARRAY, .array_idx=a_idx};
                }
            }
        }
        idx++;
        if(idx == ctx->interned_arrays.capacity*2) idx = 0;
    }

    return val;
}

static
DrJsonValue
drj_dupe_object_ronly(DrJsonContext* ctx, DrJsonValue src_val){
    assert(src_val.kind == DRJSON_OBJECT);
    ssize_t new_idx = alloc_obj(ctx);
    if(new_idx < 0) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom when duping object");
    DrJsonObject* src = &ctx->objects.data[src_val.object_idx];
    DrJsonObject* dst = &ctx->objects.data[new_idx];
    uint32_t cap = src->count;
    void* items = drj_alloc(ctx, drjson_size_for_object_of_length(cap));
    if(!items) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "oom when duping object");
    *dst = (DrJsonObject){
        .count = cap,
        .capacity = cap,
        .object_items = items,
        .marked = 0,
        .read_only = 1,
    };
    uint32_t* idxes = (uint32_t*)((char*)items + sizeof(DrJsonObjectPair)*cap);
    drj_memcpy(items, src->object_items, cap*sizeof(DrJsonObjectPair));
    drj_memset(idxes, 0xff, cap*2*sizeof *idxes);
    DrJsonObjectPair* pairs = items;
    for(size_t i = 0; i < cap; i++){
        Atom a = pairs[i].atom;
        uint32_t hash = a->hash;
        uint32_t idx = fast_reduce32(hash, cap*2);
        while(idxes[idx] != UINT32_MAX){
            idx++;
            if(idx == cap*2) idx = 0;
        }
        idxes[idx] = (uint32_t)i;
    }
    return (DrJsonValue){.kind = DRJSON_OBJECT, .object_idx=new_idx};
}

static
DrJsonValue
drj_intern_object(DrJsonContext* ctx, DrJsonValue val, _Bool consume){
    assert(val.kind == DRJSON_OBJECT);
    DrJsonObject* odata = ctx->objects.data;
    DrJsonObject* object = &odata[val.object_idx];
    if(object->read_only) return val;
    size_t count = object->count;
    DrJsonObjectPair* pairs = object->object_items;
    for(size_t i = 0; i < count; i++){
        if(!drj_is_ro(ctx, pairs[i].value))
            return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "All values of object must be read only to be interned");
    }
    // assume we're going to insert for purpose of checking capacity.
    if(ctx->interned_objects.count == ctx->interned_objects.capacity){
        size_t old_cap = ctx->interned_objects.capacity;
        size_t new_cap = old_cap?old_cap*2:16;
        size_t old_size = sizeof(DrjHashIdx)*old_cap + sizeof(uint32_t)*2*old_cap;
        size_t new_size = sizeof(DrjHashIdx)*new_cap + sizeof(uint32_t)*2*new_cap;
        void* data = drj_realloc(ctx, ctx->interned_objects.data, old_size, new_size);
        if(!data) return drjson_make_error(DRJSON_ERROR_ALLOC_FAILURE, "Failed to allocate while interning object");
        DrjHashIdx* hi = data;
        uint32_t* idxes = (uint32_t*)((char*)data+sizeof(DrjHashIdx)*new_cap);
        drj_memset(idxes, 0xff, sizeof(uint32_t)*2*new_cap);
        size_t ocount = 0;
        for(size_t i = 0; i < ctx->interned_objects.count; i++){
            if(hi[i].idx == DRJ_FREE_IDX) continue;
            uint32_t hash = hi[i].hash;
            uint32_t idx = fast_reduce32(hash, (uint32_t)new_cap*2);
            while(idxes[idx] != UINT32_MAX){
                idx++;
                if(idx == new_cap*2) idx = 0;
            }
            if(i != ocount) hi[ocount] = hi[i];
            idxes[idx] = (uint32_t)ocount;
            ocount++;
        }
        ctx->interned_objects.capacity = new_cap;
        ctx->interned_objects.data = data;
        ctx->interned_objects.count = ocount;
    }
    uint32_t hash = hash_align8(pairs, count * sizeof *pairs);
    uint32_t* idxes = (uint32_t*)(ctx->interned_objects.data+ctx->interned_objects.capacity);
    uint32_t idx = fast_reduce32(hash, (uint32_t)ctx->interned_objects.capacity*2);
    DrjHashIdx* hi = ctx->interned_objects.data;
    uint32_t first_free = -1; _Bool found_free = 0;
    for(;;){
        uint32_t i = idxes[idx];
        if(i == UINT32_MAX){
            if(found_free) idx = first_free;
            DrJsonValue cpy = consume?val:drj_dupe_object_ronly(ctx, val);
            if(consume) object->read_only = 1;
            // DrJsonValue cpy = drj_dupe_object_ronly(ctx, val);
            if(cpy.kind == DRJSON_ERROR) return cpy;
            idxes[idx] = (uint32_t)ctx->interned_objects.count;
            hi[ctx->interned_objects.count] = (DrjHashIdx){.hash=hash, .idx=(uint32_t)cpy.object_idx};
            ctx->interned_objects.count++;
            return cpy;
        }
        else if((hi[i].hash == hash && hi[i].idx == DRJ_FREE_IDX)){
            found_free = 1;
            first_free = idx;
            // printf("Empty slot\n");
        }
        else if(hi[i].hash == hash){
            uint32_t o_idx = hi[i].idx;
            DrJsonObject* o = &ctx->objects.data[o_idx];
            if(o->count == object->count){
                if(memcmp(o->object_items, object->object_items, o->count*sizeof(DrJsonObjectPair)) == 0){
                    if(consume) drj_free_obj(ctx, object);
                    return (DrJsonValue){.kind=DRJSON_OBJECT, .object_idx=o_idx};
                }
            }
        }
        idx++;
        if(idx == ctx->interned_objects.capacity*2) idx = 0;
    }

    return val;
}


static
_Bool
drj_is_ro(DrJsonContext* ctx, DrJsonValue val){
    switch(val.kind){
        case DRJSON_ARRAY_VIEW:
        case DRJSON_OBJECT_KEYS:
        case DRJSON_OBJECT_VALUES:
        case DRJSON_OBJECT_ITEMS: return 0;
        case DRJSON_OBJECT:{
            DrJsonObject* odata = ctx->objects.data;
            DrJsonObject* object = &odata[val.object_idx];
            return object->read_only;
        }break;
        case DRJSON_ARRAY:{
            DrJsonArray* adata = ctx->arrays.data;
            DrJsonArray* array = &adata[val.array_idx];
            return array->read_only;
        }break;
        default: return 1;
    }
}

DRJSON_API
DrJsonValue
drjson_intern_value(DrJsonContext* ctx, DrJsonValue val, _Bool consume){
    switch(val.kind){
        case DRJSON_ARRAY_VIEW:
        case DRJSON_OBJECT_KEYS:
        case DRJSON_OBJECT_VALUES:
        case DRJSON_OBJECT_ITEMS: return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "Cannot intern this type");
        case DRJSON_ARRAY: return drj_intern_array(ctx, val, consume);
        case DRJSON_OBJECT: return drj_intern_object(ctx, val, consume);
        default: return val;
    }
}

DRJSON_API
DrJsonValue
drjson_atom_to_value(Atom atom){
    // TODO: simd
    const char* txt = atom->data;
    size_t len = atom->length;
    _Bool needs_escape = 0;
    for(size_t i = 0; i < len; i++){
        switch(txt[i]){
            // 0x0 through 0x1f (0 through 31) have to all be
            // escaped with the 6 character sequence of \u00xx
            // Why on god's green earth did they force utf16 escapes?

            // gnu case ranges are nicer, but nonstandard
            // just spell them all out.
            case  0: case  1: case  2: case  3: case  4:
            // 8 is '\b', 9 is '\t'
            case  5: case  6: case  7:
            // 10 is '\n', 12 is '\f', 13 is '\r'
                     case 11:                   case 14:
            case 15: case 16: case 17: case 18: case 19:
            case 20: case 21: case 22: case 23: case 24:
            case 25: case 26: case 27: case 28: case 29:
            case 30: case 31:
            case '"':
            case '\\':
            case '\b':
            case '\f':
            case '\n':
            case '\r':
            case '\t':
                needs_escape = 1;
                goto done;
            // Other characters are allowed through as is
            // (presumably utf-8).
            default:
                continue;
        }
    }
    done:;
    return (DrJsonValue){.string={.kind=DRJSON_STRING, .needs_escape=needs_escape}, .atom=atom};
}


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#include "fpconv/src/fpconv.c"

#endif
