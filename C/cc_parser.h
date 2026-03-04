#ifndef C_CC_PARSER_H
#define C_CC_PARSER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/atom_map.h"
#include "../Drp/free_list.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "cc_tok.h"
#include "cc_type.h"
#include "cc_type_cache.h"
#include "cc_expr.h"
#include "cc_stmt.h"
#include "cc_func.h"
#include "cc_var.h"
#include "cc_scope.h"
#include "cc_interp.h"
#include "cpp_preprocessor.h"
#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CcToken
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCSTATMENT
#define MARRAY_CCSTATMENT
#define MARRAY_T CcStatement
#include "../Drp/Marray.h"
#endif
typedef struct CcPackRecord CcPackRecord;
struct CcPackRecord {
    StringView ident;
    uint16_t pack;
};
#ifndef MARRAY_CCPACKRECORD
#define MARRAY_CCPACKRECORD
#define MARRAY_T CcPackRecord
#include "../Drp/Marray.h"
#endif
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CcAttributes CcAttributes;
struct CcAttributes {
    union {
        uint64_t bits;
        struct {
            uint64_t packed:            1,
                     transparent_union: 1,
                     has_aligned:       1,
                     _padding:          13,
                     vector_size:       16,
                     aligned:           16,
                     _padding2:         16;
        };
    };
};

static inline
void
cc_clear_attributes(CcAttributes* attrs){
    attrs->bits = 0;
}

typedef struct CcParser CcParser;
struct CcParser {
    union {
        uint32_t flags;
        struct {
            uint32_t repl:1, // repl mode, allow top level statements
                     eager_parsing: 1, // Parse function bodies upon definition instead of upon use.
                    _padding:30;
        };
    };
    Marray(CcStatement) toplevel_statements; // only allowed in repl/script mode.
    AtomMap(uintptr_t) toplevel_labels; // label name -> statement index (1-based, like CcFunc.labels)
    CppPreprocessor cpp;
    CcTypeCache type_cache;
    // for lookahead/pushback, LIFO
    Marray(CcToken) pending;
    CcAttributes attributes;
    Marray(CcPackRecord) pack_stack;
    uint16_t pragma_pack; // 0 = default (no pack), otherwise pack(N) value
    CcScope global;
    CcScope* current;
    CcFunc*_Nullable current_func;
    FreeList(CcScope) scratch_scopes;
    FreeList(Marray(CcToken)) scratch_tokens;
    ArenaAllocator scratch_arena;
    CcInterpFrame top_frame;
    CcInterpFrame *current_frame;
};

static int cc_parse_top_level(CcParser*, _Bool* finished);
static int cc_parse_all(CcParser*);
static void cc_parser_discard_input(CcParser*);
static int cc_push_scope(CcParser*);
static void cc_pop_scope(CcParser*);
static int cc_register_pragmas(CcParser*);
static int cc_define_builtin_types(CcParser*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
