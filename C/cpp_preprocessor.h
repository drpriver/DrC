#ifndef C_CPP_PREPROCESSOR_H
#define C_CPP_PREPROCESSOR_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//

#include "../Drp/atom_map.h"
#include "../Drp/Allocators/allocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/long_string.h"
#include "../Drp/file_cache.h"
#include "../Drp/atom_table.h"
#include "../Drp/logger.h"
#include "../Drp/env.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/free_list.h"
#include "../Drp/rng.h"
#include "cpp_tok.h"

#ifndef MARRAY_STRING_VIEW
#define MARRAY_STRING_VIEW
#define MARRAY_T StringView
#include "../Drp/Marray.h"
#endif

#ifndef MARRAY_CPPTOKEN
#define MARRAY_CPPTOKEN
#define MARRAY_T CPPToken
#include "../Drp/Marray.h"
#endif

typedef Marray(CPPToken) CPPTokens;

#ifndef MARRAY_ATOM
#define MARRAY_ATOM
#define MARRAY_T Atom
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#include "../Drp/Marray.h"
#ifdef __clang__
#pragma clang diagnostic pop
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
            uint64_t no_expand_args:   1;
            uint64_t _reserved:        4;
            uint64_t is_disabled:      1;
            uint64_t _padding:         7;
            uint64_t nparams:          16;
            uint64_t nreplace:         32;
        };
    };
    SrcLoc def_loc;
    uint64_t data[];
};
static inline
CPPToken*
cpp_cmacro_replacement(CMacro* macro){
    return (CPPToken*)&macro->data[macro->nparams];
}
static inline
Atom _Nonnull*_Nonnull
cpp_cmacro_params(CMacro* macro){
    return (Atom*)macro->data;
}
_Static_assert(sizeof(CMacro) == 2*sizeof(uint64_t), "");

typedef struct CPPFrame CPPFrame;
struct CPPFrame {
    uint32_t file_id; // index into fc->map
    union {
        struct {
            uint32_t line;
            uint32_t column;
            size_t cursor;
        };
        struct CPPFrameLoc {
            uint32_t line;
            uint32_t column;
            size_t cursor;
        } loc;
    };
    StringView txt;
    IncludePosition include_position; // where we are in the include lookup, for include_next and related.
};

typedef struct CppPoundIf CppPoundIf;
struct CppPoundIf {
    SrcLoc start;
    _Bool true_taken: 1;
    _Bool seen_else: 1;
    _Bool is_active: 1;
    _Bool is_dummy: 1;
    // like 60 padding bits, wowee
};

typedef struct CPragma CPragma;

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#ifndef MARRAY_CPPFRAME
#define MARRAY_CPPFRAME
#define MARRAY_T CPPFrame
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_SIZE_T
#define MARRAY_SIZE_T
#define MARRAY_T size_t
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CPPPOUNDIF
#define MARRAY_CPPPOUNDIF
#define MARRAY_T CppPoundIf
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_UINT32_T
#define MARRAY_UINT32_T
#define MARRAY_T uint32_t
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CPreprocessor CPreprocessor;
struct CPreprocessor {
    Allocator allocator;
    ArenaAllocator synth_arena; // For things that need to be synthesized
    AtomMap(CMacro) macros;
    AtomMap(CPragma) pragmas;
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
    Marray(CppPoundIf) if_stack;
    CPPTokens pending; // push in reverse order so you can pop in LIFO order
    _Bool at_line_start;
    FreeList(CPPTokens) scratch_list; // reusable scratch space for collecting tokens
    FreeList(Marray(size_t)) scratch_idxes;
    uint64_t counter;
    Atom date, time;
    RngState rng;
    AtomMap(CPPTokens) kv_store; // for __set/__get
    Marray(uint32_t) pragma_once_files; // sorted list of file_ids with #pragma once
};

static
int
cpp_define_macro(CPreprocessor* cpp, StringView name, size_t ntoks, size_t nparams, CMacro*_Nullable*_Nonnull outmacro);

static
int
cpp_undef_macro(CPreprocessor* cpp, StringView name);

static
int
cpp_define_obj_macro(CPreprocessor* cpp, StringView name, CPPToken*_Null_unspecified toks, size_t ntoks);

static
int
cpp_push_tok(CPreprocessor* cpp, CPPTokens* dst, CPPToken tok);

static
_Bool
cpp_has_include(CPreprocessor* cpp, _Bool quote, StringView header_name);

static
int
cpp_next_token(CPreprocessor* cpp, CPPToken* tok);

// Includes a file without going through the include path machinery.
static
int
cpp_include_file_via_file_cache(CPreprocessor* cpp, StringView path);

// Implementation of a builtin object-like macro (that isn't just a predefined constant tokens)
typedef int CppObjMacroFn(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc, CPPTokens* outtoks);

static
int
cpp_define_builtin_obj_macro(CPreprocessor* cpp, StringView name, CppObjMacroFn* fn, void*_Null_unspecified ctx);

// Implementation of a builtin function-like macro (that can't be expressed normally)
typedef int CppFuncMacroFn(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc, CPPTokens* outtoks, const CPPTokens* args, const Marray(size_t)* arg_seps);
static
int
cpp_define_builtin_func_macro(CPreprocessor* cpp, StringView name, CppFuncMacroFn* fn, void*_Null_unspecified ctx, size_t nparams, _Bool variadic, _Bool no_expand);

static int cpp_define_builtin_macros(CPreprocessor* cpp);


typedef int CppPragmaFn(void* _Null_unspecified ctx, CPreprocessor* cpp, SrcLoc loc, const CPPToken*_Null_unspecified toks, size_t ntoks);
static int cpp_register_pragma(CPreprocessor* cpp, StringView name, CppPragmaFn* fn, void* _Null_unspecified ctx);
struct CPragma {
    void* ctx;
    CppPragmaFn* fn;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
