#ifndef C_CPP_PREPROCESSOR_H
#define C_CPP_PREPROCESSOR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include "../Drp/atom_map.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/long_string.h"
#include "../Drp/file_cache.h"
#include "../Drp/atom_table.h"
#include "../Drp/logger.h"
#include "../Drp/env.h"
#include "cpp_tok.h"

#ifndef MARRAY_STRING_VIEW
#define MARRAY_STRING_VIEW
#define MARRAY_T StringView
#include "../Drp/Marray.h"
#endif

#ifndef MARRAY_PP_TOK
#define MARRAY_PP_TOK
#define MARRAY_T CPPToken
#include "../Drp/Marray.h"
#endif

#ifndef MARRAY_ATOM
#define MARRAY_ATOM
#define MARRAY_T Atom
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "../Drp/Marray.h"
#ifdef __clang__
#pragma clang diagnostic push
#endif
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef arrlen
#define arrlen(arr) (sizeof(arr)/sizeof((arr)[0]))
#endif

typedef struct IncludePosition IncludePosition;
struct IncludePosition {
    size_t array, // which array we are scanning through
           idx;   // actual index into the array.
};

typedef struct CMacro CMacro;
struct CMacro {
    union {
        uint64_t _bits;
        struct {
            uint64_t is_function_like: 1;
            uint64_t is_variadic:      1;
            uint64_t is_builtin:       1;
            uint64_t _reserved:        5;
            uint64_t _padding:         8;
            uint64_t nparams:          16;
            uint64_t nreplace:         32;
        };
    };
    SrcLoc def_loc;
    uint64_t data[];
};
static inline
CPPToken*
pp_cmacro_replacement(CMacro* macro){
    return (CPPToken*)&macro->data[macro->nparams];
}
static inline
Atom _Nonnull*_Nonnull
pp_cmacro_params(CMacro* macro){
    return (Atom*)macro->data;
}
_Static_assert(sizeof(CMacro) == 2*sizeof(uint64_t), "");

typedef struct CPPFrame CPPFrame;
struct CPPFrame {
    StringView path;
    StringView txt;
    size_t cursor;
    IncludePosition include_position; // where we are in the include lookup, for include_next and related.
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#ifndef MARRAY_CPPFRAME
#define MARRAY_CPPFRAME
#define MARRAY_T CPPFrame
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CPreprocessor CPreprocessor;
struct CPreprocessor {
    Allocator allocator;
    AtomMap(CMacro) macros;
    FileCache* fc;
    AtomTable* at;
    Logger* logger;
    Environment* env;
    /*
     * 1. For the quote form of the include directive, the directory of the
     *    current file is searched first.
     * 2. For the quote form of the include directive, the directories specified
     *    by -iquote options are searched in left-to-right order, as they appear
     *    on the command line.
     * 3. Directories specified with -I options are scanned in left-to-right
     *    order.
     * 4. Directories specified with -isystem options are scanned in
     *    left-to-right order.
     * 5. Standard system directories are scanned.
     * 6. Directories specified with -idirafter options are scanned in
     *    left-to-right order.
     */
    union {
        struct {
            Marray(StringView) iquote_paths,
                               Ipaths,
                               isystem_paths,
                               istandard_system_paths,
                               idirafter_paths,
                               framework_paths;
        };
        Marray(StringView) include_paths[5];
    };
    Marray(CPPFrame) frames;
    Marray(CPPToken) token_buff;
};

static
int
cpp_define_macro(CPreprocessor* cpp, StringView name, size_t ntoks, size_t nparams, CMacro*_Nullable*_Nonnull outmacro);

static
int
cpp_undef_macro(CPreprocessor* cpp, StringView name);

static
int
cpp_define_obj_macro(CPreprocessor* cpp, StringView name, CPPToken* toks, size_t ntoks);

static
_Bool
cpp_has_include(CPreprocessor* cpp, _Bool quote, StringView header_name);

static
int
cpp_next_token(CPreprocessor* cpp, CPPToken* tok);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
