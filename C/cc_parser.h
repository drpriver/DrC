#ifndef C_CC_PARSER_H
#define C_CC_PARSER_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/atom_map.h"
#include "../Drp/free_list.h"
#include "cc_lexer.h"
#include "c_tok.h"
#include "cc_type.h"
#include "cc_type_cache.h"
#include "cc_expr.h"
#include "cc_stmt.h"
#include "cc_func.h"
#include "cc_var.h"
#include "cc_scope.h"
#ifndef MARRAY_CCTOKEN
#define MARRAY_CCTOKEN
#define MARRAY_T CCToken
#include "../Drp/Marray.h"
#endif
#ifndef MARRAY_CCSTATMENT
#define MARRAY_CCSTATMENT
#define MARRAY_T CcStatement
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
    CcLexer lexer;
    CcTypeCache type_cache;
    // for lookahead/pushback, LIFO
    Marray(CCToken) pending;
    CcAttributes attributes;
    CcScope global;
    CcScope* current;
    FreeList(CcScope) scratch_scopes;
    FreeList(Marray(CCToken)) scratch_tokens;
};

static int cc_parse_top_level(CcParser*, _Bool* finished);
static void cc_parser_discard_input(CcParser*);
static int cc_push_scope(CcParser*);
static void cc_pop_scope(CcParser*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
