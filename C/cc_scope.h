#ifndef C_CC_SCOPE_H
#define C_CC_SCOPE_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "../Drp/atom.h"
#include "../Drp/atom_map.h"
#include "../Drp/atom_map16.h"
#include "../Drp/parray.h"
#include "../Drp/Allocators/allocator.h"
#include "cc_type.h"
#include "srcloc.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
typedef struct CcScope CcScope;
typedef struct CcVariable CcVariable;
typedef struct CcFunc CcFunc;
typedef struct CcUnion CcUnion;
typedef struct CcEnum CcEnum;
typedef struct CcTypedef CcTypedef;
struct CcTypedef {
    union {
        struct {
            CcQualType type;
            SrcLoc loc; // zero for builtins and automatic aliases that inherit the tag location
        };
        uint64_t payload[2];
    };
};
_Static_assert(sizeof(CcQualType) == sizeof(uint64_t), "");
_Static_assert(sizeof(SrcLoc) == sizeof(uint64_t), "");
_Static_assert(sizeof(CcTypedef) == 2*sizeof(uint64_t), "");
struct CcScope {
    CcScope* parent;
    AtomMap16(CcTypedef) typedefs;
    AtomMap(CcVariable) variables;
    AtomMap(CcFunc) functions;
    AtomMap(CcStruct) structs;
    AtomMap(CcUnion) unions;
    AtomMap(CcEnum) enums;
    AtomMap(CcEnumerator) enumerators;
    Parray(CcFunc) deferred_methods;
};

static inline void cc_scope_clear(CcScope* scope);
static SrcLoc cc_typedef_loc(const CcTypedef*);

enum {
    CC_SCOPE_NO_WALK,
    CC_SCOPE_WALK_CHAIN,
};

enum CcSymbolKind {
    CC_SYM_VAR,
    CC_SYM_FUNC,
    CC_SYM_TYPEDEF,
    CC_SYM_ENUMERATOR,
};

typedef struct CcSymbol CcSymbol;
struct CcSymbol {
    enum CcSymbolKind kind;
    union {
        CcVariable* var;
        CcFunc* func;
        CcQualType type;
        CcEnumerator* enumerator;
    };
};

// Lookup an ordinary identifier, checking all four maps at each scope
// level before ascending. Returns true if found.
static
_Bool
cc_scope_lookup_symbol(CcScope*, Atom, int walk, CcSymbol* out);

static
CcVariable* _Nullable
cc_scope_lookup_var(CcScope*, Atom, int walk);

static
int
cc_scope_insert_var(Allocator, CcScope*, Atom, CcVariable*);

static
CcQualType
cc_scope_lookup_typedef(CcScope*, Atom, int walk);

static
int
cc_scope_insert_typedef(Allocator, CcScope*, Atom, CcQualType, SrcLoc);

static
CcFunc* _Nullable
cc_scope_lookup_func(CcScope*, Atom, int walk);

static
int
cc_scope_insert_func(Allocator, CcScope*, Atom, CcFunc*);

static
CcStruct* _Nullable
cc_scope_lookup_struct_tag(CcScope*, Atom, int walk);

static
int
cc_scope_insert_struct_tag(Allocator, CcScope*, Atom, CcStruct*);

static
CcUnion* _Nullable
cc_scope_lookup_union_tag(CcScope*, Atom, int walk);

static
int
cc_scope_insert_union_tag(Allocator, CcScope*, Atom, CcUnion*);

static
CcEnum* _Nullable
cc_scope_lookup_enum_tag(CcScope*, Atom, int walk);

static
int
cc_scope_insert_enum_tag(Allocator, CcScope*, Atom, CcEnum*);

static
CcEnumerator* _Nullable
cc_scope_lookup_enumerator(CcScope*, Atom, int walk);

static
int
cc_scope_insert_enumerator(Allocator, CcScope*, Atom, CcEnumerator*);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
