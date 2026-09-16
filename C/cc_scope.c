#ifndef C_CC_SCOPE_C
#define C_CC_SCOPE_C
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "cc_scope.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
CcVariable* _Nullable
cc_scope_lookup_var(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcVariable* v = AM_get(&s->variables, name);
        if(v) return v;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_var(Allocator al, CcScope* scope, Atom name, CcVariable* var){
    return AM_put(&scope->variables, al, name, var);
}

static
CcQualType
cc_scope_lookup_typedef(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcTypedef* v = AM16_get(&s->typedefs, name);
        if(v) return v->type;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return (CcQualType){0};
}

static
SrcLoc
cc_typedef_loc(const CcTypedef* td){
    if(td->loc.bits) return td->loc;
    // Automatic aliases follow the tag as a forward declaration is completed.
    switch(ccqt_kind(td->type)){
        case CC_STRUCT: return ccqt_as_struct(td->type)->loc;
        case CC_UNION: return ccqt_as_union(td->type)->loc;
        case CC_ENUM: return ccqt_as_enum(td->type)->loc;
        default: return (SrcLoc){0};
    }
}

static
int
cc_scope_insert_typedef(Allocator al, CcScope* scope, Atom name, CcQualType type, SrcLoc loc){
    uint64_t payload[2] = {type.bits, loc.bits};
    return AM16_put(&scope->typedefs, al, name, payload);
}

static
CcFunc* _Nullable
cc_scope_lookup_func(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcFunc* f = AM_get(&s->functions, name);
        if(f) return f;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_func(Allocator al, CcScope* scope, Atom name, CcFunc* func){
    return AM_put(&scope->functions, al, name, func);
}

static
CcStruct* _Nullable
cc_scope_lookup_struct_tag(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcStruct* st = AM_get(&s->structs, name);
        if(st) return st;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_struct_tag(Allocator al, CcScope* scope, Atom name, CcStruct* st){
    return AM_put(&scope->structs, al, name, st);
}

static
CcUnion* _Nullable
cc_scope_lookup_union_tag(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcUnion* u = AM_get(&s->unions, name);
        if(u) return u;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_union_tag(Allocator al, CcScope* scope, Atom name, CcUnion* u){
    return AM_put(&scope->unions, al, name, u);
}

static
CcEnum* _Nullable
cc_scope_lookup_enum_tag(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcEnum* e = AM_get(&s->enums, name);
        if(e) return e;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_enum_tag(Allocator al, CcScope* scope, Atom name, CcEnum* e){
    return AM_put(&scope->enums, al, name, e);
}

static
CcEnumerator* _Nullable
cc_scope_lookup_enumerator(CcScope* scope, Atom name, int walk){
    for(CcScope* s = scope; s; s = s->parent){
        CcEnumerator* e = AM_get(&s->enumerators, name);
        if(e) return e;
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return NULL;
}

static
int
cc_scope_insert_enumerator(Allocator al, CcScope* scope, Atom name, CcEnumerator* e){
    return AM_put(&scope->enumerators, al, name, e);
}

static
_Bool
cc_scope_lookup_symbol(CcScope* scope, Atom name, int walk, CcSymbol* out){
    for(CcScope* s = scope; s; s = s->parent){
        CcTypedef* td = AM16_get(&s->typedefs, name);
        if(td){
            out->kind = CC_SYM_TYPEDEF;
            out->type = td->type;
            return 1;
        }
        CcVariable* v = AM_get(&s->variables, name);
        if(v){
            out->kind = CC_SYM_VAR;
            out->var = v;
            return 1;
        }
        CcFunc* f = AM_get(&s->functions, name);
        if(f){
            out->kind = CC_SYM_FUNC;
            out->func = f;
            return 1;
        }
        CcEnumerator* e = AM_get(&s->enumerators, name);
        if(e){
            out->kind = CC_SYM_ENUMERATOR;
            out->enumerator = e;
            return 1;
        }
        if(walk == CC_SCOPE_NO_WALK) break;
    }
    return 0;
}

static inline
void
cc_scope_clear(CcScope* scope){
    scope->deferred_methods.count = 0;
    AM16_clear(&scope->typedefs);
    AM_clear(&scope->variables);
    AM_clear(&scope->functions);
    AM_clear(&scope->structs);
    AM_clear(&scope->unions);
    AM_clear(&scope->enums);
    AM_clear(&scope->enumerators);
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
