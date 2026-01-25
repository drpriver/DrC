//
// Copyright © 2022-2024, David Priver <david@davidpriver.com>
//
#ifndef DRJSON_H
#define DRJSON_H
#include <stddef.h> // size_t
#include <stdint.h>
#include <string.h>
#include "Allocators/allocator.h"
#include "atom.h"
#include "atom_table.h"

#define DRJSON_VERSION_MAJOR 4
#define DRJSON_VERSION_MINOR 0
#define DRJSON_VERSION_MICRO 0
#define DRJSON_VERSION "4.0.0"

#ifndef drj_memcpy
#if !defined(__GNUC__) || defined(__IMPORTC__)
#define drj_memset memset
#define drj_memcpy memcpy
#define drj_memmove memmove
#else
#define drj_memset __builtin_memset
#define drj_memcpy __builtin_memcpy
#define drj_memmove __builtin_memmove
#endif
#endif

// Define DRJSON_NO_STDIO to avoid import stdio
#ifndef DRJSON_NO_STDIO
#include <stdio.h>
#endif

#ifndef DRJSON_API

#ifndef DRJSON_STATIC_LIB

#ifdef _WIN32
#define DRJSON_API extern __declspec(dllimport)
#else
#define DRJSON_API extern __attribute__((visibility("default")))
#endif

#else
#define DRJSON_API extern __attribute__((visibility("hidden")))
#endif

#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#endif

////////////////
// DrJsonContext
//

// Opaque type
typedef struct DrJsonContext DrJsonContext;

DRJSON_API
DrJsonContext*_Nullable
drjson_create_ctx(Allocator allocator, AtomTable* table);

// Frees the ctx and all data allocated by the ctx
DRJSON_API
void
drjson_ctx_free_all(DrJsonContext* ctx);

//------------------------------------------------------------


//////////////
// Error Codes
//
enum DrJsonErrorCode {
    DRJSON_ERROR_NONE           = 0,
    DRJSON_ERROR_UNEXPECTED_EOF = 1,
    DRJSON_ERROR_ALLOC_FAILURE  = 2,
    DRJSON_ERROR_MISSING_KEY    = 3,
    DRJSON_ERROR_INDEX_ERROR    = 4,
    DRJSON_ERROR_INVALID_CHAR   = 5,
    DRJSON_ERROR_INVALID_VALUE  = 6,
    DRJSON_ERROR_TOO_DEEP       = 7,
    DRJSON_ERROR_TYPE_ERROR     = 8,
    DRJSON_ERROR_INVALID_ERROR  = 9,
};

typedef enum DrJsonErrorCode DrJsonErrorCode;

// Returns a nul-terminated string of static storage.
// length is optional.
DRJSON_API
const char*
drjson_error_name(DrJsonErrorCode code, size_t*_Nullable length);

//------------------------------------------------------------

//////////////
// DrJsonValue
//

enum DrJsonKind {
    DRJSON_ERROR         = 0x0, // error code + message
    DRJSON_NUMBER        = 0x1, // double
    DRJSON_INTEGER       = 0x2, // int64_t
    DRJSON_UINTEGER      = 0x3, // uint64_t
    DRJSON_STRING        = 0x4, // atom
    DRJSON_ARRAY         = 0x5, // index
    DRJSON_OBJECT        = 0x6, // index
    DRJSON_NULL          = 0x7,
    DRJSON_BOOL          = 0x8, //
    DRJSON_ARRAY_VIEW    = 0x9,
    DRJSON_OBJECT_KEYS   = 0xa,
    DRJSON_OBJECT_VALUES = 0xb,
    DRJSON_OBJECT_ITEMS  = 0xc,
};
typedef enum DrJsonKind DrJsonKind;

// Returns a nul-terminated string of static storage.
// length is optional.
DRJSON_API
const char*
drjson_kind_name(DrJsonKind kind, size_t*_Nullable length);


typedef struct DrJsonValue DrJsonValue;
struct DrJsonValue {
    union {
        uint64_t bits;
        struct {
            uint64_t kind: 16;
            uint64_t _pad: 48;
        };
        struct {
            uint64_t kind: 16;
            uint64_t code: 16;
            uint64_t len: 32;
        } error;
        struct {
            uint64_t kind: 16;
            uint64_t needs_escape: 1;
            uint64_t _pad: 47;
        } string;
    };
    union {
        // DRJSON_NUMBER
        double number;

        // DRJSON_INTEGER
        int64_t integer;

        // DRJSON_UINTEGER
        uint64_t uinteger;

        // DRJSON_STRING
        Atom atom;

        // DRJSON_ERROR
        const char* err_mess; // pointer to a c-string literal, nul terminated.
                              // Length is also given.  Be aware of shared
                              // object lifetimes if you dynamically link.

        // DRJSON_BOOL
        _Bool boolean;

        // DRJSON_ARRAY
        // DRJSON_ARRAY_VIEW
        size_t array_idx;

        // DRJSON_OBJECT
        // DRJSON_OBJECT_KEYS
        // DRJSON_OBJECT_VALUES
        // DRJSON_OBJECT_ITEMS
        size_t object_idx;

        // null is implicit
    };
};

_Static_assert(sizeof(DrJsonValue) == 2*sizeof(uint64_t), "");

//------------------------------------------------------------

////////////////////////
// DrJsonValue Utilities
//
static inline
int
drjson_is_numeric(DrJsonValue v){
    return v.kind == DRJSON_NUMBER || v.kind == DRJSON_INTEGER || v.kind == DRJSON_UINTEGER;
}

// shallow equality (although it does compare strings)
static inline
int
drjson_eq(DrJsonValue a, DrJsonValue b){
    if((a.kind == DRJSON_INTEGER  || a.kind == DRJSON_UINTEGER) && (b.kind == DRJSON_INTEGER || b.kind == DRJSON_UINTEGER)){
        return a.uinteger == b.uinteger;
    }
    if(a.kind != b.kind) return 0;
    switch(a.kind){
        case DRJSON_NUMBER:
            return a.number == b.number;
        case DRJSON_INTEGER:
            return a.integer == b.integer;
        case DRJSON_UINTEGER:
            return a.uinteger == b.uinteger;
        case DRJSON_STRING:
            return a.atom == b.atom;
        case DRJSON_ARRAY:
            return a.array_idx == b.array_idx;
        case DRJSON_OBJECT:
            return a.object_idx == b.object_idx;
        case DRJSON_NULL:
            return 1;
        case DRJSON_BOOL:
            return a.boolean == b.boolean;
        case DRJSON_ERROR:
            return a.error.code == b.error.code;
        case DRJSON_ARRAY_VIEW:
            return a.array_idx == b.array_idx;
        case DRJSON_OBJECT_KEYS:
        case DRJSON_OBJECT_VALUES:
        case DRJSON_OBJECT_ITEMS:
            return a.object_idx == b.object_idx;
        default:
            return 0;
    }
}

//------------------------------------------------------------

//////////////////////////////
// Helpers to make DrJsonValue
//

static inline
DrJsonValue
drjson_make_error(DrJsonErrorCode error, const char* mess){
    return (DrJsonValue){.error.kind=DRJSON_ERROR, .error.code=(uint16_t)error, .err_mess=mess, .error.len=(uint32_t)strlen(mess)};
}

static inline
DrJsonValue
drjson_make_null(void){
    return (DrJsonValue){.kind=DRJSON_NULL};
}

static inline
DrJsonValue
drjson_make_bool(_Bool b){
    return (DrJsonValue){.kind=DRJSON_BOOL, .boolean=b};
}

static inline
DrJsonValue
drjson_make_number(double d){
    return (DrJsonValue){.kind=DRJSON_NUMBER, .number=d};
}

static inline
DrJsonValue
drjson_make_int(int64_t i){
    return (DrJsonValue){.kind=DRJSON_INTEGER, .integer=i};
}

static inline
DrJsonValue
drjson_make_uint(int64_t u){
    return (DrJsonValue){.kind=DRJSON_UINTEGER, .uinteger=u};
}

DRJSON_API
DrJsonValue
drjson_make_object(DrJsonContext* ctx);

DRJSON_API
DrJsonValue
drjson_make_array(DrJsonContext* ctx);

//
// drjson_intern_value
// -------------------
// Returns a read-only copy of the value or an error if it fails to intern it.
// Note that this is only needed for objects and arrays.
// All values referenced from the value must also be read-only.
//
// For types other than objects and arrays, the original value is instead
// returned.
//
// Args:
//  ctx:
//  val:
//  consume: If true, the given value is either frozen or freed on success.
//
// Returns:
//  The new interned value.
//  If consume is true and the returned value is not an error, don't refer to
//  the given value anymore. Only refer to the returned value.
//  This is somewhat like realloc.
//
DRJSON_API
DrJsonValue
drjson_intern_value(DrJsonContext* ctx, DrJsonValue val, _Bool consume);

DRJSON_API
DrJsonValue
drjson_make_string(DrJsonContext* ctx, const char* s, size_t length);

DRJSON_API
DrJsonValue
drjson_atom_to_value(Atom atom);

// returns an object_keys (which is array-like) or error
static inline
DrJsonValue
drjson_object_keys(DrJsonValue o){
    if(o.kind != DRJSON_OBJECT)
        return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "call to keys for non-object");
    o.kind = DRJSON_OBJECT_KEYS;
    return o;
}

// returns an object_values (which is array-like) or error
static inline
DrJsonValue
drjson_object_values(DrJsonValue o){
    if(o.kind != DRJSON_OBJECT)
        return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "call to values for non-object");
    o.kind = DRJSON_OBJECT_VALUES;
    return o;
}

// returns an object_items (which is array-like) or error.
// The "array" is a flat array of alternating key value.
static inline
DrJsonValue
drjson_object_items(DrJsonValue o){
    if(o.kind != DRJSON_OBJECT)
        return drjson_make_error(DRJSON_ERROR_TYPE_ERROR, "call to items for non-object");
    o.kind = DRJSON_OBJECT_ITEMS;
    return o;
}

//------------------------------------------------------------

//////////
// Parsing
//

typedef struct DrJsonParseContext DrJsonParseContext;
struct DrJsonParseContext {
    const char* cursor; // initialize to string
    const char* end; // initialize to string + length
    const char* begin; // initialize to string
    int depth; // initialize to 0
    DrJsonContext* ctx; //
    _Bool _read_only_objects;
};

enum {
    DRJSON_PARSE_FLAG_NONE = 0x0,
    DRJSON_PARSE_FLAG_BRACELESS_OBJECT = 0x1,
    DRJSON_PARSE_FLAG_INTERN_OBJECTS = 0x4,
};

DRJSON_API
DrJsonValue
drjson_parse(DrJsonParseContext* ctx, unsigned flags);

// If an error is returned, use this to get the line and column.
DRJSON_API
void
drjson_get_line_column(const DrJsonParseContext* ctx, size_t* line, size_t* column);

// Convenience function that setups a parse context for you.
DRJSON_API
DrJsonValue
drjson_parse_string(DrJsonContext* ctx, const char* text, size_t length, unsigned flags);

//------------------------------------------------------------

/////////////
// Object ops
//

DRJSON_API
DrJsonValue
drjson_object_get_item2(const DrJsonContext* ctx, DrJsonValue object, const char* key, size_t keylen);

DRJSON_API
DrJsonValue
drjson_object_get_item(const DrJsonContext* ctx, DrJsonValue object, Atom key);

DRJSON_API
int // 0 on success
drjson_object_set_item2(DrJsonContext* ctx, DrJsonValue object, const char* key, size_t keylen, DrJsonValue item);

DRJSON_API
int // 0 on success
drjson_object_set_item(DrJsonContext* ctx, DrJsonValue object, Atom key, DrJsonValue item);

//------------------------------------------------------------

////////////
// Array ops
//

DRJSON_API
int // 0 on success
drjson_array_push_item(const DrJsonContext* ctx, DrJsonValue array, DrJsonValue item);

DRJSON_API
DrJsonValue // returns DRJSON_ERROR if array is empty
drjson_array_pop_item(const DrJsonContext* ctx, DrJsonValue array);

DRJSON_API
DrJsonValue // returns DRJSON_ERROR if array is empty
drjson_array_del_item(const DrJsonContext* ctx, DrJsonValue array, size_t idx);

DRJSON_API
int // 0 on success
drjson_array_insert_item(const DrJsonContext* ctx, DrJsonValue array, size_t idx, DrJsonValue item);

// overwrites an existing item. Errors on out of bounds.
DRJSON_API
int // 0 on success
drjson_array_set_by_index(const DrJsonContext* ctx, DrJsonValue array, int64_t idx, DrJsonValue value);

//------------------------------------------------------------

///////////////////////
// Array and Object ops
//

// Works on both arrays and array-likes (array view, object keys view, object
// values view, etc.)
// A negative idx means to idx from the back (last item is at -1).
DRJSON_API
DrJsonValue
drjson_get_by_index(const DrJsonContext* ctx, DrJsonValue v, int64_t idx);

// -1 if invalid error
DRJSON_API
int64_t
drjson_len(const DrJsonContext* ctx, DrJsonValue v);

// empties an array or object
DRJSON_API
int
drjson_clear(const DrJsonContext* ctx, DrJsonValue v);

//------------------------------------------------------------

//////////
// Queries
//

DRJSON_API
DrJsonValue
drjson_query(const DrJsonContext* ctx, DrJsonValue v, const char* query, size_t query_length);

DRJSON_API
DrJsonValue
drjson_checked_query(const DrJsonContext* ctx, DrJsonValue v, int type, const char* query, size_t query_length);

//------------------------------------------------------------

/////////////////////////
// Printing/serialization
//

// Writer abstraction for serializing json.
typedef struct DrJsonTextWriter DrJsonTextWriter;
struct DrJsonTextWriter {
    void*_Null_unspecified up; //user pointer
    // return non-zero on error.
    int (*write)(void*_Null_unspecified user_data, const void*, size_t);
};

enum {
    DRJSON_PRINT_PRETTY         = 0x1,
    DRJSON_PRINT_APPEND_NEWLINE = 0x2,
    DRJSON_PRINT_APPEND_ZERO    = 0x4,
};

// Returns 0 on success, 1 on error.
DRJSON_API
int
drjson_print_value(const DrJsonContext* ctx, const DrJsonTextWriter* writer, DrJsonValue v, int indent, unsigned flags);

DRJSON_API
int
drjson_print_value_mem(const DrJsonContext* ctx, void* buff, size_t bufflen, DrJsonValue v, int indent, unsigned flags, size_t*_Nullable printed);

// Returns 0 on success, 1 on error.
DRJSON_API
int
drjson_print_error(const DrJsonTextWriter* restrict writer, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v);

DRJSON_API
int
drjson_print_error_mem(void* buff, size_t bufflen, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v);

#if !defined(DRJSON_NO_STDIO) && !defined(DRJSON_NO_IO)
// Like above, but for FILE*
DRJSON_API
int
drjson_print_value_fp(const DrJsonContext* ctx, FILE* fp, DrJsonValue v, int indent, unsigned flags);

DRJSON_API
int
drjson_print_error_fp(FILE* fp, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v);
#endif

#if !defined(DRJSON_NO_IO)
DRJSON_API
int
drjson_print_value_path(const DrJsonContext* ctx, const char* path, DrJsonValue v, int indent, unsigned flags);
#endif

#if  !defined(DRJSON_NO_IO) && !defined(_WIN32) // posix
DRJSON_API
int
drjson_print_value_fd(const DrJsonContext* ctx, int fd, DrJsonValue v, int indent, unsigned flags);

DRJSON_API
int
drjson_print_error_fd(int fd, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v);
#elif !defined(DRJSON_NO_IO) // windows
// To avoid needing to include <Windows.h>, we just use a `void*` for `HANDLE`,
// but it is like what you would get from CreateFileW.
DRJSON_API
int
drjson_print_value_HANDLE(const DrJsonContext* ctx, void* hnd, DrJsonValue v, int indent, unsigned flags);
DRJSON_API

int
drjson_print_error_HANDLE(void* hnd, const char* filename, size_t filename_len, size_t line, size_t column, DrJsonValue v);
#endif

//------------------------------------------------------------

////////
// misc
//

// If mutating the json tree, use this to escape strings.
DRJSON_API
int
drjson_escape_string(DrJsonContext* ctx, const char* restrict unescaped, size_t length, Atom _Nullable*_Nonnull outatom);

#if 0 // to be implemented
DRJSON_API
int
drjson_unescape_string(const DrJsonAllocator* restrict allocator, const char* restrict unescaped, size_t length, char*_Nullable restrict *_Nonnull restrict outstring, size_t* restrict outlength);
#endif

//
// Implements mark+sweep gc for objects and arrays in the ctx.
// Note that strings are not gc'd.
//
DRJSON_API
int
drjson_gc(DrJsonContext* ctx, const DrJsonValue* roots, size_t rootcount);



#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
