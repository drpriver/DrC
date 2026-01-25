#ifndef SIMP_C_PARSE_C
#define SIMP_C_PARSE_C
#include "simp_c_lex.h"
#include "simp_c_parse.h"
#include "../../Drp/msb_sprintf.h"
#include "../../Drp/atomf.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#define TODO() do { \
    const CToken* t = simp_curr_tok(ctx); \
    simp_c_logf(ctx, "Unimplemented at %s:%d (on '%.*s')", __FILE__, __LINE__, (int)t->content.length, t->content.text); \
    return 1; \
} while(0)

static inline
Scope*
simp_scope(CParseCtx* ctx){
    return &ctx->scopes.data[ctx->current_scope];
}

static
inline
int
simp_push_scope(CParseCtx* ctx){
    int err = 0;
    ctx->current_scope++;
    if(ctx->current_scope < ctx->scopes.count){
        Scope* sc = &ctx->scopes.data[ctx->current_scope];
        AM_clear(&sc->tag_table);
        AM_clear(&sc->typedef_table);
        AM_clear(&sc->decls);
    }
    else {
        err = ma_push(Scope)(&ctx->scopes, ctx->allocator, (Scope){0});
    }
    return err;

}
static
inline
void
simp_pop_scope(CParseCtx* ctx){
    ctx->current_scope--;
}

static inline
void*
type_wash(const void* p){
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-qual"
    #if !defined(__clang__)
        #pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
    #endif
#elif defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable: 4090)
#else
#endif
    return (void*)p;
#if defined(__clang__) || defined(__GNUC__)
    #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
    #pragma warning(pop)
#endif
}

static inline
void
simp_skip_ws(CParseCtx* ctx){
    for(;;){
        if(ctx->current_token >= ctx->tokens.count)
            return;
        CToken* tok = &ctx->tokens.data[ctx->current_token];
        if(tok->type == CTOK_WHITESPACE || tok->type == CTOK_COMMENT){
            ctx->current_token++;
            continue;
        }
        if(tok->type == CTOK_PREPROC){
            // TODO: handle #pragma
            ctx->current_token++;
            continue;
        }
        return;
    }
}

static inline
void
simp_next_token(CParseCtx* ctx){
    ctx->current_token++;
    simp_skip_ws(ctx);
}

static inline
const CToken*
simp_peek_tok_n(CParseCtx* ctx, int n){
    size_t idx = ctx->current_token + n;
    while(idx < ctx->tokens.count){
        CToken* tok = &ctx->tokens.data[idx];
        if(tok->type == CTOK_WHITESPACE || tok->type == CTOK_COMMENT){
            idx++;
            continue;
        }
        if(tok->type == CTOK_PREPROC){
            idx++;
            continue;
        }
        return tok;
    }
    static const CToken eof = {
        .type = CTOK_EOF,
    };
    return &eof;
}

static inline
const CToken*
simp_curr_tok(CParseCtx* ctx){
    return simp_peek_tok_n(ctx, 0);
}

static inline
const CToken*
simp_peek_tok(CParseCtx* ctx){
    return simp_peek_tok_n(ctx, 1);
}

static inline
_Bool
simp_match_punct(CParseCtx* ctx, CPunct punc){
    simp_skip_ws(ctx);
    const CToken* cur = simp_curr_tok(ctx);
    if(cur->type == CTOK_PUNCTUATOR && cur->subtype == punc){
        simp_next_token(ctx);
        return 1;
    }
    return 0;
}

static
inline
const CToken*_Nullable
simp_match(CParseCtx* ctx, CTokenType type){
    const CToken* cur = simp_curr_tok(ctx);
    if(cur->type == type){
        simp_next_token(ctx);
        return cur;
    }
    return NULL;
}

static
inline
const CToken*_Nullable
simp_match2(CParseCtx* ctx, CTokenType type, unsigned subtype){
    const CToken* cur = simp_curr_tok(ctx);
    if(cur->type == type && cur->subtype == subtype){
        simp_next_token(ctx);
        return cur;
    }
    return NULL;
}

enum Specifier {
    SPEC_NONE         = 0x00000000,
    SPEC_AUTO         = 0x00000001,
    SPEC_CONSTEXPR    = 0x00000002,
    SPEC_EXTERN       = 0x00000004,
    SPEC_REGISTER     = 0x00000008,
    SPEC_STATIC       = 0x00000010,
    SPEC_THREAD_LOCAL = 0x00000020,
    SPEC_TYPEDEF      = 0x00000040,
    SPEC_UNSIGNED     = 0x00000080,
    SPEC_SIGNED       = 0x00000100,
    SPEC_LONG         = 0x00000200,
    SPEC_LONG_LONG    = 0x00000400,
    SPEC_SHORT        = 0x00000800,
    SPEC_INT          = 0x00001000,
    SPEC_CHAR         = 0x00002000,
    SPEC_TYPED        = 0x00004000,
    SPEC_INLINE       = 0x00008000,
    SPEC_NORETURN     = 0x00010000,
    SPEC_CONST        = 0x00020000,
    SPEC_VOLATILE     = 0x00040000,
    SPEC_ATOMIC       = 0x00080000,
    SPEC_RESTRICT     = 0x00100000,
};

typedef struct CAttributes CAttributes;
struct CAttributes {
    char _pad;
};

typedef struct CSpecifier CSpecifier;
struct CSpecifier {
    CAttributes attrs;
    CType*_Null_unspecified type;
    uint32_t spec;
};

static
CType*
simp_dup_type(CParseCtx* ctx, const CType* type);

static inline
int
simp_c_add_spec(CParseCtx* ctx, uint32_t* specifier, uint32_t add){
    uint32_t s = *specifier;
    if(s & add){
        if(add == SPEC_LONG){
            if(s & SPEC_LONG_LONG) {
                simp_c_logf(ctx, "long long long");
                return 1;
            }
            s |= SPEC_LONG_LONG;
        }
        else{
            simp_c_logf(ctx, "duplicate specifier");
            return 1;
        }
    }
    else
        s |= add;
    *specifier = s;
    return 0;
}

static int simp_c_parse_declaration(CParseCtx* ctx, _Bool default_extern);
static int simp_c_parse_enum(CParseCtx* ctx, CType*_Nullable*_Nonnull type);
static int simp_c_parse_struct(CParseCtx* ctx, CType*_Nullable*_Nonnull type);
static int simp_c_parse_union(CParseCtx* ctx, CType*_Nullable*_Nonnull type);
static int simp_c_parse_typeof(CParseCtx* ctx, CType*_Nullable*_Nonnull type);
static int simp_c_parse_typeof_unqual(CParseCtx* ctx, CType*_Nullable*_Nonnull type);
static int simp_c_parse_typename(CParseCtx* ctx, CType*_Nullable*_Nonnull type);

static int simp_c_resolve_type_with_specifier(CParseCtx* ctx, uint32_t specifier, CType*_Nullable*_Nonnull type);

static
int
simp_c_parse_external_declaration(CParseCtx* ctx){
    int err = simp_c_parse_declaration(ctx, 1);
    return err;
}

static int simp_c_parse_declaration_specifier(CParseCtx* ctx, uint32_t* specifier, CType*_Nullable*_Nonnull type, _Bool* keep_going);

_Alignas(Atom_) static const char ATOM_void_[] = "\x04\x00\x00\x00\xf6\xb3\x54\x93""void";
#define ATOM_void ((Atom)ATOM_void_)
_Alignas(Atom_) static const char ATOM_auto_[] = "\x04\x00\x00\x00\xea\xcb\xdc\x02""auto";
#define ATOM_auto ((Atom)ATOM_auto_)
_Alignas(Atom_) static const char ATOM_char_[] = "\x04\x00\x00\x00\xd6\x1e\x4c\x03""char";
#define ATOM_char ((Atom)ATOM_char_)
_Alignas(Atom_) static const char ATOM_uchar_[] = "\x0d\x00\x00\x00\x1a\x1f\x2d\xb3""unsigned char";
#define ATOM_uchar ((Atom)ATOM_uchar_)
_Alignas(Atom_) static const char ATOM_schar_[] = "\x0b\x00\x00\x00\x99\x6b\x60\x36""signed char";
#define ATOM_schar ((Atom)ATOM_schar_)
_Alignas(Atom_) static const char ATOM_short_[] = "\x05\x00\x00\x00\x4e\x20\xb1\x25""short";
#define ATOM_short ((Atom)ATOM_short_)
_Alignas(Atom_) static const char ATOM_ushort_[] = "\x0e\x00\x00\x00\x5b\xa8\xcf\xab""unsigned short";
#define ATOM_ushort ((Atom)ATOM_ushort_)
_Alignas(Atom_) static const char ATOM_int_[] = "\x03\x00\x00\x00\xe8\xa3\x74\x79""int";
#define ATOM_int ((Atom)ATOM_int_)
_Alignas(Atom_) static const char ATOM_unsigned_[] = "\x08\x00\x00\x00\xaf\x28\x15\xf8""unsigned";
#define ATOM_unsigned ((Atom)ATOM_unsigned_)
_Alignas(Atom_) static const char ATOM_long_[] = "\x04\x00\x00\x00\x1d\x78\x2e\x5a""long";
#define ATOM_long ((Atom)ATOM_long_)
_Alignas(Atom_) static const char ATOM_ulong_[] = "\x0d\x00\x00\x00\xd1\x79\x4f\xea""unsigned long";
#define ATOM_ulong ((Atom)ATOM_ulong_)
_Alignas(Atom_) static const char ATOM_llong_[] = "\x09\x00\x00\x00\xb6\x18\x13\xc2""long long";
#define ATOM_llong ((Atom)ATOM_llong_)
_Alignas(Atom_) static const char ATOM_ullong_[] = "\x12\x00\x00\x00\x14\xb9\x6a\x5b""unsigned long long";
#define ATOM_ullong ((Atom)ATOM_ullong_)
_Alignas(Atom_) static const char ATOM_float_[] = "\x05\x00\x00\x00\x6a\xf8\xc7\x02""float";
#define ATOM_float ((Atom)ATOM_float_)
_Alignas(Atom_) static const char ATOM_double_[] = "\x06\x00\x00\x00\xfb\xe8\x16\x25""double";
#define ATOM_double ((Atom)ATOM_double_)
_Alignas(Atom_) static const char ATOM_ldouble_[] = "\x0b\x00\x00\x00\x7e\x91\x6b\x69""long double";
#define ATOM_ldouble ((Atom)ATOM_ldouble_)
_Alignas(Atom_) static const char ATOM_Float16_[] = "\x08\x00\x00\x00\x65\x87\x9b\x1d""_Float16";
#define ATOM_Float16 ((Atom)ATOM_Float16_)
_Alignas(Atom_) static const char ATOM_Float32_[] = "\x08\x00\x00\x00\x94\x20\x44\xfd""_Float32";
#define ATOM_Float32 ((Atom)ATOM_Float32_)
_Alignas(Atom_) static const char ATOM_Float64_[] = "\x08\x00\x00\x00\xd7\x3e\xcd\x86""_Float64";
#define ATOM_Float64 ((Atom)ATOM_Float64_)
_Alignas(Atom_) static const char ATOM_Float128_[] = "\x09\x00\x00\x00\x5b\x6c\x9a\xa0""_Float128";
#define ATOM_Float128 ((Atom)ATOM_Float128_)
_Alignas(Atom_) static const char ATOM_Decimal32_[] = "\x0a\x00\x00\x00\x57\xe4\xcb\xb3""_Decimal32";
#define ATOM_Decimal32 ((Atom)ATOM_Decimal32_)
_Alignas(Atom_) static const char ATOM_Decimal64_[] = "\x0a\x00\x00\x00\x14\xfa\x42\xc8""_Decimal64";
#define ATOM_Decimal64 ((Atom)ATOM_Decimal64_)
_Alignas(Atom_) static const char ATOM_Decimal128_[] = "\x0b\x00\x00\x00\xaf\x36\x09\x70""_Decimal128";
#define ATOM_Decimal128 ((Atom)ATOM_Decimal128_)

static const Atom _Nonnull CT_ATOMS[] = {
    (Atom)ATOM_void,
    (Atom)ATOM_auto,
    (Atom)ATOM_char,
    (Atom)ATOM_uchar,
    (Atom)ATOM_schar,
    (Atom)ATOM_short,
    (Atom)ATOM_ushort,
    (Atom)ATOM_int,
    (Atom)ATOM_unsigned,
    (Atom)ATOM_long,
    (Atom)ATOM_ulong,
    (Atom)ATOM_llong,
    (Atom)ATOM_ullong,
    (Atom)ATOM_float,
    (Atom)ATOM_double,
    (Atom)ATOM_ldouble,
    (Atom)ATOM_Float16,
    (Atom)ATOM_Float32,
    (Atom)ATOM_Float64,
    (Atom)ATOM_Float128,
    (Atom)ATOM_Decimal32,
    (Atom)ATOM_Decimal64,
    (Atom)ATOM_Decimal128,
};

static
int
simp_c_register_atoms(CParseCtx* ctx){
    AtomTable* at = &ctx->at;
    for(size_t i = 0; i < sizeof CT_ATOMS / sizeof CT_ATOMS[0];i++){
        if(CT_ATOMS[i] == nil_atom) continue;
        int e = AT_store_atom(at, CT_ATOMS[i]);
        if(e != 0) __builtin_debugtrap();
    }
    return 0;
}

static const CTypeBasic CT_void       = {.name=ATOM_void,       .kind=CT_BASIC};
static const CTypeBasic CT_auto       = {.name=ATOM_auto,       .kind=CT_BASIC};
static const CTypeBasic CT_char       = {.name=ATOM_char,       .kind=CT_BASIC};
static const CTypeBasic CT_uchar      = {.name=ATOM_uchar,      .kind=CT_BASIC};
static const CTypeBasic CT_schar      = {.name=ATOM_schar,      .kind=CT_BASIC};
static const CTypeBasic CT_short      = {.name=ATOM_short,      .kind=CT_BASIC};
static const CTypeBasic CT_ushort     = {.name=ATOM_ushort,     .kind=CT_BASIC};
static const CTypeBasic CT_int        = {.name=ATOM_int,        .kind=CT_BASIC};
static const CTypeBasic CT_uint       = {.name=ATOM_unsigned,   .kind=CT_BASIC};
static const CTypeBasic CT_long       = {.name=ATOM_long,       .kind=CT_BASIC};
static const CTypeBasic CT_ulong      = {.name=ATOM_ulong,      .kind=CT_BASIC};
static const CTypeBasic CT_llong      = {.name=ATOM_llong,      .kind=CT_BASIC};
static const CTypeBasic CT_ullong     = {.name=ATOM_ullong,     .kind=CT_BASIC};
static const CTypeBasic CT_float      = {.name=ATOM_float,      .kind=CT_BASIC};
static const CTypeBasic CT_double     = {.name=ATOM_double,     .kind=CT_BASIC};
static const CTypeBasic CT_ldouble    = {.name=ATOM_ldouble,    .kind=CT_BASIC};
static const CTypeBasic CT_float16    = {.name=ATOM_Float16,    .kind=CT_BASIC};
static const CTypeBasic CT_float32    = {.name=ATOM_Float32,    .kind=CT_BASIC};
static const CTypeBasic CT_float64    = {.name=ATOM_Float64,    .kind=CT_BASIC};
static const CTypeBasic CT_float128   = {.name=ATOM_Float128,   .kind=CT_BASIC};
static const CTypeBasic CT_decimal32  = {.name=ATOM_Decimal32,  .kind=CT_BASIC};
static const CTypeBasic CT_decimal64  = {.name=ATOM_Decimal64,  .kind=CT_BASIC};
static const CTypeBasic CT_decimal128 = {.name=ATOM_Decimal128, .kind=CT_BASIC};

static
int
simp_c_parse_attributes(CParseCtx* ctx, CAttributes* attrs);


typedef struct CPointerSpec CPointerSpec;
struct CPointerSpec {
    uint32_t spec;
    CAttributes attr;
};

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#define MARRAY_T CPointerSpec
#include "../../Drp/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

typedef struct CDeclarator CDeclarator;
struct CDeclarator {
    CAttributes attrs;
    const CToken*_Null_unspecified ident;
    Marray(CPointerSpec) pointer;
};
#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#define MARRAY_T CDeclarator
#include "../../Drp/Marray.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif
static
int
simp_c_parse_declarator(CParseCtx* ctx, CDeclarator* decl);

static
int
simp_c_emit_decl(CParseCtx* ctx, CDeclarator* decl, uint32_t spec, const CType* type);

static
int
simp_c_parse_declaration(CParseCtx* ctx, _Bool default_extern){
    (void)default_extern;
    // allow empty semis as macros can produce it.
    while(simp_match_punct(ctx, ';'))
        ;
    // (6.7.11) static_assert-declaration:
    //      static_assert ( constant-expression, string-literal ) ;
    //      static_assert ( constant-expression ) ;
    if(simp_match2(ctx, CTOK_KEYWORD, CKW_static_assert)){
        // TODO: parse constant-expression, etc.
        // Just skip over the static assert for now.
        if(!simp_match_punct(ctx, '(')){
            simp_c_logf(ctx, "%d: expected '('\n", __LINE__);
            return 1;
        }
        int parens = 0;
        for(;;){
            if(simp_match(ctx, CTOK_EOF)){
                simp_c_logf(ctx, "%d: EOF\n", __LINE__);
                return 1;
            }
            if(simp_match_punct(ctx, '(')){
                parens++;
                continue;
            }
            if(simp_match_punct(ctx, ')')){
                if(parens){
                    parens--;
                    continue;
                }
                break;
            }
            simp_next_token(ctx);
        }
        if(!simp_match_punct(ctx, ';')){
            simp_c_logf(ctx, "%d: expected ';'\n", __LINE__);
            return 1;
        }
        return 0;
    }
    // (6.7) declaration:
    //      declaration-specifiers init-declarator-list(opt) ;
    //      attribute-specifier-sequence declaration-specifiers init-declarator-list ;
    //      static_assert-declaration
    //      attribute-declaration
    int err = 0;
    CSpecifier specifier;
    err = simp_c_parse_attributes(ctx, &specifier.attrs);
    if(err) return err;
    // (6.7) attribute-declaration:
    //      attribute-specifier-sequence ;
    if(simp_match_punct(ctx, ';')){
        // weird, but allowed
        return 0;
    }
    // (6.7) declaration-specifiers:
    //      declaration-specifier attribute-specifier-sequence(opt)
    //      declaration-specifier declaration-specifiers
    _Bool keep_going = 1;
    for(;;){
        if(simp_match(ctx, CTOK_EOF))
            return 0;
        err = simp_c_parse_declaration_specifier(ctx, &specifier.spec, &specifier.type, &keep_going);
        if(err) return err;
        err = simp_c_parse_attributes(ctx, &specifier.attrs);
        if(err) return err;
        if(!keep_going) break;
    }
    if(simp_match_punct(ctx, ';')){
        // weird, but allowed
        return 0;
    }
    if(!specifier.spec && !specifier.type){
        simp_c_logf(ctx, "Expected specifier or type\n");
        const CToken* tok = simp_curr_tok(ctx);
        simp_c_logf(ctx, "at '%.*s'\n", (int)tok->content.length, tok->content.text);
        return 1;
    }
    err = simp_c_resolve_type_with_specifier(ctx, specifier.spec, &specifier.type);
    if(err) return err;
    if(!specifier.type){
        simp_c_logf(ctx, "declaration without a type");
        return 1;
    }
    // (6.7) init-declarator-list:
    //      init-declarator
    //      init-declarator-list , init-declarator
    // (6.7) init-declarator:
    //      declarator
    //      declarator = initializer
    Marray(CDeclarator) decls = {0};
    for(;;){
        CDeclarator decl = {0};
        err = simp_c_parse_declarator(ctx, &decl);
        if(err) return err;
        err = ma_push(CDeclarator)(&decls, ctx->allocator, decl);
        const CToken* tok = simp_curr_tok(ctx);
        if(tok->type == CTOK_PUNCTUATOR){
            if(decls.count == 1 && tok->subtype == '{'){
                simp_next_token(ctx);
                // TODO: function definition
                int brackets = 0;
                for(;;){
                    if(simp_match(ctx, CTOK_EOF)){
                        simp_c_logf(ctx, "%d: EOF\n", __LINE__);
                        return 1;
                    }
                    if(simp_match_punct(ctx, '{')){
                        brackets++;
                        continue;
                    }
                    if(simp_match_punct(ctx, '}')){
                        if(brackets){
                            brackets--;
                            continue;
                        }
                        else {
                            break;
                        }
                    }
                    simp_next_token(ctx);
                }
                break;
            }
            if(tok->subtype == CP_assign){
                simp_next_token(ctx);
                TODO();
                tok = simp_curr_tok(ctx);
                if(tok->type != CTOK_PUNCTUATOR){
                    simp_c_logf(ctx, "%d: wtf", __LINE__);
                    return 1;
                }
            }
            err = simp_c_emit_decl(ctx, &decl, specifier.spec, specifier.type);
            if(err) return err;
            if(tok->subtype == ';'){
                simp_next_token(ctx);
                break;
            }
            if(tok->subtype == ','){
                simp_next_token(ctx);
                continue;
            }
        }
        simp_c_logf(ctx, "%d: expected declarator\n", __LINE__);
        return 1;
    }
    return 0;
}


static
int
simp_c_parse_declaration_specifier(CParseCtx* ctx, uint32_t* specifier, CType*_Nullable*_Nonnull type, _Bool* keep_going){
    // (6.7) declaration-specifier:
    //      storage-class-specifier
    //      type-specifier-qualifier
    //      function-specifier
    // (6.7.1) storage-class-specifier:
    //      auto
    //      constexpr
    //      extern
    //      register
    //      static
    //      thread_local
    //      typedef
    // (6.7.2.1) type-specifier-qualifier:
    //      type-specifier
    //      type-qualifier
    //      alignment-specifier
    // (6.7.2) type-specifier:
    //      void
    //      char
    //      short
    //      int
    //      long
    //      float
    //      double
    //      signed
    //      unsigned
    //      _BitInt ( constant-expression )
    //      bool
    //      _Complex
    //      _Decimal32
    //      _Decimal64
    //      _Decimal128
    //      atomic-type-specifier
    //      struct-or-union-specifier
    //      enum-specifier
    //      typedef-name
    //      typeof-specifier
    // (6.7.3) type-qualifier:
    //      const
    //      restrict
    //      volatile
    //      _Atomic
    // (6.7.5) alignment-specifier:
    //      alignas ( type-name )
    //      alignas ( constant-expression )
    // (6.7.4) function-specifier:
    //      inline
    //      _Noreturn
    const CToken* tok = simp_curr_tok(ctx);
    #define REDUNDANT_TYPE() do { \
        if(*type){ \
            simp_c_logf(ctx, "Redundant type"); \
            return 1; \
        } \
    }while(0)
    {
    int err = 0;
    if(tok->type == CTOK_KEYWORD){
        switch((CKeyword)tok->subtype){
            case CKW_int:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_INT);
                break;
            case CKW_long:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_LONG);
                break;
            case CKW_char:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_CHAR);
                break;
            case CKW_auto:
                err = simp_c_add_spec(ctx, specifier, SPEC_AUTO);
                break;
            case CKW_bool:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_AUTO);
                break;
            case CKW_enum:
                REDUNDANT_TYPE();
                err = simp_c_parse_enum(ctx, type);
                break;
            case CKW_void:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_void);
                break;
            case CKW_float:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_float);
                break;
            case CKW_const:
                err = simp_c_add_spec(ctx, specifier, SPEC_CONST);
                break;
            case CKW_short:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_SHORT);
                break;
            case CKW_union:
                REDUNDANT_TYPE();
                err = simp_c_parse_union(ctx, type);
                break;
            case CKW_double:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_double);
                break;
            case CKW_extern:
                err = simp_c_add_spec(ctx, specifier, SPEC_EXTERN);
                break;
            case CKW_inline:
                err = simp_c_add_spec(ctx, specifier, SPEC_INLINE);
                break;
            case CKW_signed:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_SIGNED);
                break;
            case CKW_static:
                err = simp_c_add_spec(ctx, specifier, SPEC_STATIC);
                break;
            case CKW_struct:
                REDUNDANT_TYPE();
                err = simp_c_parse_struct(ctx, type);
                break;
            case CKW_typeof:
                REDUNDANT_TYPE();
                err = simp_c_parse_typeof(ctx, type);
                break;
            case CKW_alignas:
                TODO();
            case CKW_typedef:
                err = simp_c_add_spec(ctx, specifier, SPEC_TYPEDEF);
                break;
            case CKW__Atomic:
                err =  simp_c_add_spec(ctx, specifier, SPEC_ATOMIC);
                break;
            case CKW__BitInt:
                REDUNDANT_TYPE();
                TODO();
            case CKW__Complex:
                TODO();
            case CKW_register:
                err = simp_c_add_spec(ctx, specifier, SPEC_REGISTER);
                break;
            case CKW_restrict:
                err = simp_c_add_spec(ctx, specifier, SPEC_RESTRICT);
                break;
            case CKW_unsigned:
                REDUNDANT_TYPE();
                err = simp_c_add_spec(ctx, specifier, SPEC_UNSIGNED);
                break;
            case CKW_volatile:
                err = simp_c_add_spec(ctx, specifier, SPEC_VOLATILE);
                break;
            case CKW__Float16:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_float16);
                break;
            case CKW__Float32:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_float32);
                break;
            case CKW__Float64:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_float64);
                break;
            case CKW__Float128:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_float128);
                break;
            case CKW_constexpr:
                err = simp_c_add_spec(ctx, specifier, SPEC_CONSTEXPR);
                break;
            case CKW__Imaginary:
                REDUNDANT_TYPE();
                TODO();
            case CKW__Noreturn:
                err = simp_c_add_spec(ctx, specifier, SPEC_NORETURN);
                break;
            case CKW__Decimal32:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_decimal32);
                break;
            case CKW__Decimal64:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_decimal64);
                break;
            case CKW__Decimal128:
                REDUNDANT_TYPE();
                *type = type_wash(&CT_decimal128);
                break;
            case CKW_thread_local:
                err = simp_c_add_spec(ctx, specifier, SPEC_THREAD_LOCAL);
                break;
            case CKW_typeof_unqual:
                REDUNDANT_TYPE();
                err = simp_c_parse_typeof_unqual(ctx, type);
                break;
            default:
                simp_c_logf(ctx, "Unexpected keyword: %*.s\n", (int)tok->content.length, tok->content.text);
                TODO();
        }
        if(!err && tok == simp_curr_tok(ctx))
            simp_next_token(ctx);
        return err;
    }
    }
    if(tok->type == CTOK_IDENTIFIER){
        if(!*type){
            simp_c_parse_typename(ctx, type);
            if(*type) return 0;
        }
        *keep_going = 0;
        return 0;
    }
    *keep_going = 0;
    return 0;
}


SIMP_C_LEX_API
int
simp_c_parse_tu(CParseCtx* ctx){
    ctx->current_scope = (size_t)-1;
    int err = simp_push_scope(ctx);
    if(err) return err;
    for(;;){
        simp_skip_ws(ctx);
        if(simp_curr_tok(ctx)->type == CTOK_EOF)
            return 0;
        err = simp_c_parse_external_declaration(ctx);
        if(err) return err;
    }
}

static
int
simp_c_parse_enum(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    TODO();
    (void)ctx; (void)type; return 1;
}
static
int
simp_c_parse_struct(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    TODO();
    (void)ctx; (void)type; return 1;
}
static
int
simp_c_parse_union(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    TODO();
    (void)ctx; (void)type; return 1;
}
static
int
simp_c_parse_typeof(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    TODO();
    (void)ctx; (void)type; return 1;
}
static
int
simp_c_parse_typeof_unqual(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    TODO();
    (void)ctx; (void)type; return 1;
}

static
int
simp_c_parse_typename(CParseCtx* ctx, CType*_Nullable*_Nonnull type){
    (void)ctx; (void)type; return 0;
    TODO();
}

SIMP_C_LEX_API
void
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
simp_c_logf(CParseCtx* ctx, const char* fmt, ...){
    MStringBuilder sb = {.allocator=ctx->scratch};
    va_list ap;
    va_start(ap, fmt);
    msb_vsprintf(&sb, fmt, ap);
    va_end(ap);
    if(!sb.cursor || sb.data[sb.cursor-1] != '\n')
        msb_write_char(&sb, '\n');
    if(sb.errored) goto finally;
    StringView s = msb_borrow_sv(&sb);
    if(ctx->logger.write_func)
        ctx->logger.write_func(ctx->logger.handle, s.text, s.length);
    finally:
    msb_destroy(&sb);
}
static
int
simp_c_resolve_type_with_specifier(CParseCtx* ctx, uint32_t specifier, CType*_Nullable*_Nonnull type){
    const CType* t = *type;
    if(t){
        if(t == (const CType*)&CT_double){
            if(specifier & SPEC_LONG){
                t = (const CType*)&CT_ldouble;
            }
            if(specifier & (SPEC_UNSIGNED|SPEC_SIGNED|SPEC_LONG_LONG|SPEC_SHORT|SPEC_INT|SPEC_CHAR)){
                simp_c_logf(ctx, "Invalid decl spec with a double");
                return 1;
            }
        }
        else {
            if(specifier & (SPEC_UNSIGNED|SPEC_SIGNED|SPEC_LONG_LONG|SPEC_SHORT|SPEC_INT|SPEC_CHAR|SPEC_LONG)){
                simp_c_logf(ctx, "Invalid decl spec with a type");
                return 1;
            }
        }
    }
    else {
        uint32_t masked = specifier & (SPEC_UNSIGNED|SPEC_SIGNED|SPEC_LONG|SPEC_LONG_LONG|SPEC_SHORT|SPEC_INT|SPEC_CHAR);
        switch(masked){
            case SPEC_CHAR:
                t = (const CType*)&CT_char;
                break;
            case SPEC_CHAR|SPEC_SIGNED:
                t = (const CType*)&CT_schar;
                break;
            case SPEC_CHAR|SPEC_UNSIGNED:
                t = (const CType*)&CT_uchar;
                break;
            case SPEC_SHORT:
            case SPEC_SHORT|SPEC_INT:
            case SPEC_SHORT|SPEC_INT|SPEC_SIGNED:
            case SPEC_SHORT|SPEC_SIGNED:
                t = (const CType*)&CT_short;
                break;
            case SPEC_SHORT|SPEC_UNSIGNED:
            case SPEC_SHORT|SPEC_INT|SPEC_UNSIGNED:
                t = (const CType*)&CT_ushort;
                break;
            case 0:
            if(specifier & SPEC_AUTO){
                t = type_wash(&CT_auto);
                break;
            }
            FALLTHROUGH;
            case SPEC_INT:
            case SPEC_SIGNED:
            case SPEC_SIGNED|SPEC_INT:
                t = (const CType*)&CT_int;
                break;
            case SPEC_INT|SPEC_UNSIGNED:
            case SPEC_UNSIGNED:
                t = (const CType*)&CT_uint;
                break;
            case SPEC_LONG:
            case SPEC_LONG|SPEC_INT:
            case SPEC_LONG|SPEC_INT|SPEC_SIGNED:
            case SPEC_LONG|SPEC_SIGNED:
                t = (const CType*)&CT_long;
                break;
            case SPEC_LONG|SPEC_UNSIGNED:
            case SPEC_LONG|SPEC_INT|SPEC_UNSIGNED:
                t = (const CType*)&CT_ulong;
                break;
            case SPEC_LONG_LONG:
            case SPEC_LONG_LONG|SPEC_INT:
            case SPEC_LONG_LONG|SPEC_INT|SPEC_SIGNED:
            case SPEC_LONG_LONG|SPEC_SIGNED:
                t = (const CType*)&CT_llong;
                break;
            case SPEC_LONG_LONG|SPEC_UNSIGNED:
            case SPEC_LONG_LONG|SPEC_INT|SPEC_UNSIGNED:
                t = (const CType*)&CT_ullong;
                break;
            default:
                simp_c_logf(ctx, "Invalid type specifier");
                return 1;
        }
    }
    *type = type_wash(t);
    return 0;
}

static
int
simp_c_parse_attributes(CParseCtx* ctx, CAttributes* attrs){
    // (6.7.12.1) attribute-specifier-sequence:
    //      attribute-specifier-sequence(opt) attribute-specifier
    // (6.7.12.1) attribute-specifier:
    //      [ [ attribute-list ] ]
    // (6.7.12.1) attribute-list:
    //      attribute(opt)
    //      attribute-list , attribute(opt)
    // (6.7.12.1) attribute:
    //      attribute-token attribute-argument-clause(opt)
    // (6.7.12.1) attribute-token:
    //      standard-attribute
    //      attribute-prefixed-token
    // (6.7.12.1) standard-attribute:
    //      identifier
    // (6.7.12.1) attribute-prefixed-token:
    //      attribute-prefix :: identifier
    // (6.7.12.1) attribute-prefix:
    //      identifier
    // (6.7.12.1) attribute-argument-clause:
    //      ( balanced-token-sequence(opt) )
    // (6.7.12.1) balanced-token-sequence:
    //      balanced-token
    //      balanced-token-sequence balanced-token
    // (6.7.12.1) balanced-token:
    //      ( balanced-token-sequence(opt) )
    //      [ balanced-token-sequence(opt) ]
    //      { balanced-token-sequence(opt) }
    //      any token other than a parenthesis, a bracket, or a brace
    (void)attrs;
    for(;;){
        {
            const CToken* tok = simp_curr_tok(ctx);
            const CToken* next = simp_peek_tok(ctx);
            if(tok->type == CTOK_PUNCTUATOR && tok->subtype == '[' && next->type == CTOK_PUNCTUATOR && next->subtype == '['){
                simp_next_token(ctx);
                simp_next_token(ctx);
            }
            else {
                break;
            }
        }
        int brackets = 0;
        for(;;){
            if(simp_match_punct(ctx, '[')){
                brackets++;
                continue;
            }
            if(brackets && simp_match_punct(ctx, ']')){
                brackets--;
                continue;
            }
            const CToken* tok = simp_curr_tok(ctx);
            if(!brackets){
                const CToken* next = simp_peek_tok(ctx);
                if(tok->type == CTOK_PUNCTUATOR && tok->subtype == ']' && next->type == CTOK_PUNCTUATOR && next->subtype == ']'){
                    simp_next_token(ctx);
                    simp_next_token(ctx);
                    break;
                }
            }
            if(tok->type == CTOK_EOF){
                simp_c_logf(ctx, "unexpected eof parsing attributes");
                return 1;
            }
            if(0) simp_c_logf(ctx, "attribute token: '%.*s'\n", (int)tok->content.length, tok->content.text);
            simp_next_token(ctx);
        }
    }
    return 0;
}

static
int
simp_c_parse_declarator(CParseCtx* ctx, CDeclarator* decl){
    int err = 0;
    // (6.7.6) declarator:
    //      pointer(opt) direct-declarator
    for(;simp_match_punct(ctx, '*');){
        // (6.7.6) pointer:
        //      * attribute-specifier-sequence(opt) type-qualifier-list(opt)
        //      * attribute-specifier-sequence(opt) type-qualifier-list(opt) pointer
        uint32_t pspec = 0;
        CAttributes pattrs = {0};
        err = simp_c_parse_attributes(ctx, &pattrs);
        if(err) return err;
        // (6.7.6) type-qualifier-list:
        //      type-qualifier
        //      type-qualifier-list type-qualifier
        for(;;){
            const CToken* tok = simp_curr_tok(ctx);
            if(tok->type == CTOK_KEYWORD){
                // (6.7.3) type-qualifier:
                //      const
                //      restrict
                //      volatile
                //      _Atomic
                switch(tok->subtype){
                    case CKW_const:
                        err = simp_c_add_spec(ctx, &pspec, SPEC_CONST);
                        if(err) return 1;
                        simp_next_token(ctx);
                        continue;
                    case CKW_restrict:
                        err = simp_c_add_spec(ctx, &pspec, SPEC_RESTRICT);
                        if(err) return 1;
                        simp_next_token(ctx);
                        continue;
                    case CKW_volatile:
                        err = simp_c_add_spec(ctx, &pspec, SPEC_VOLATILE);
                        if(err) return 1;
                        simp_next_token(ctx);
                        continue;
                    case CKW__Atomic:
                        err = simp_c_add_spec(ctx, &pspec, SPEC_ATOMIC);
                        if(err) return 1;
                        simp_next_token(ctx);
                        continue;
                    default:
                        break;
                }
                break;
            }
            break;
        }
        err = ma_push(CPointerSpec)(&decl->pointer, ctx->allocator, (CPointerSpec){pspec, pattrs});
        if(err) return err;
    }
    // (6.7.6) direct-declarator:
    //      identifier attribute-specifier-sequence(opt)
    //      ( declarator )
    //      array-declarator attribute-specifier-sequence(opt)
    //      function-declarator attribute-specifier-sequence(opt)

    //  identifier attribute-specifier-sequence(opt)
    {
        const CToken* ident = simp_match(ctx, CTOK_IDENTIFIER);
        if(!ident){
            //      ( declarator )
            if(simp_match_punct(ctx, '(')){
                err = simp_c_parse_declarator(ctx, decl);
                if(err) return err;
                if(!simp_match_punct(ctx, ')')){
                    simp_c_logf(ctx, "%d: Expected ')'", __LINE__);
                    return 1;
                }
            }
            else {
                simp_c_logf(ctx, "%d: Expected declarator identifier", __LINE__);
                return 1;
            }
        }
        else {
            // simp_c_logf(ctx, "ident: '%.*s'", (int)ident->content.length, ident->content.text);
            if(decl->ident){
                simp_c_logf(ctx, "%d: multiple idents?", __LINE__);
                return 1;
            }
            decl->ident = ident;
            err = simp_c_parse_attributes(ctx, &decl->attrs);
            if(err) return err;
        }
    }
    for(;;){
        // (6.7.6) array-declarator:
        //      direct-declarator [ type-qualifier-list(opt) assignment-expression(opt) ]
        //      direct-declarator [ static type-qualifier-list(opt) assignment-expression ]
        //      direct-declarator [ type-qualifier-list static assignment-expression ]
        //      direct-declarator [ type-qualifier-list(opt) * ]
        if(simp_match_punct(ctx, '[')){
            int lbracket = 0;
            for(;;){
                if(simp_match_punct(ctx, '[')){
                    lbracket++;
                    continue;
                }
                if(lbracket && simp_match_punct(ctx, ']')){
                    lbracket--;
                    continue;
                }
                if(!lbracket && simp_match_punct(ctx, ']'))
                    break;
                if(simp_match(ctx, CTOK_EOF)) {
                    simp_c_logf(ctx, "%d: unexpected eof", __LINE__);
                    return 1;
                }
                simp_next_token(ctx);
            }
            err = simp_c_parse_attributes(ctx, &decl->attrs);
            if(err) return err;
            continue;
        }
        // (6.7.6) function-declarator:
        //      direct-declarator ( parameter-type-list(opt) )
        if(simp_match_punct(ctx, '(')){
            int lbracket = 0;
            for(;;){
                if(simp_match_punct(ctx, '(')){
                    lbracket++;
                    continue;
                }
                if(lbracket && simp_match_punct(ctx, ')')){
                    lbracket--;
                    continue;
                }
                if(!lbracket && simp_match_punct(ctx, ')'))
                    break;
                if(simp_match(ctx, CTOK_EOF)) {
                    simp_c_logf(ctx, "%d: unexpected eof", __LINE__);
                    return 1;
                }
                // TODO: paramater-type-list
                simp_next_token(ctx);
            }
            err = simp_c_parse_attributes(ctx, &decl->attrs);
            if(err) return err;
            continue;
        }
        break;
    }
    return 0;
}

static
CType*
simp_dup_type(CParseCtx* ctx, const CType* type){
    size_t sz;
    switch((enum CTypeKind)type->kind){
        case CT_BASIC:
            sz = sizeof(CTypeBasic);
            break;
        case CT_TYPEDEF:
            sz = sizeof(CTypeTypedef);
            break;
        case CT_POINTER:
            sz = sizeof(CTypePointer);
            break;
        case CT_ARRAY:
            sz = sizeof(CTypeArray);
            break;
        case CT_STRUCT:
            sz = offsetof(CTypeStruct, members) + sizeof(CField) * ((const CTypeStruct*)type)->mcount;
            break;
        case CT_UNION:
            sz = offsetof(CTypeUnion, members) + sizeof(CField) * ((const CTypeUnion*)type)->mcount;
            break;
        case CT_ENUM:
            sz = offsetof(CTypeEnum, members) + sizeof(CEnumDeclarator) * ((const CTypeEnum*)type)->mcount;
            break;
        case CT_FUNCTION:
            sz = offsetof(CTypeFunction, params) + sizeof(CTypeFunction) * ((const CTypeFunction*)type)->param_count;
            break;
    }
    CType* result = Allocator_dupe(ctx->allocator, type, sz);
    return result;
}

static
const CType*
simp_intern_type(CParseCtx* ctx, const CType* type){
    (void)ctx;
    return type;
}

static
int
simp_c_emit_decl(CParseCtx* ctx, CDeclarator* decl, uint32_t spec, const CType* type){
    MStringBuilder sb = {.allocator=ctx->scratch};
    int err = 0;
    CType* ctype = simp_dup_type(ctx, type);
    if((spec & SPEC_CONST) && !ctype->const_){
        ctype->const_ = 1;
        msb_write_literal(&sb, "const ");
    }
    if((spec & SPEC_RESTRICT) && !ctype->restrict_){
        ctype->restrict_ = 1;
        msb_write_literal(&sb, "restrict ");
    }
    if((spec & SPEC_VOLATILE) && !ctype->volatile_){
        ctype->volatile_ = 1;
        msb_write_literal(&sb, "volatile ");
    }
    if((spec & SPEC_ATOMIC) && !ctype->_Atomic_){
        ctype->_Atomic_ = 1;
        msb_write_literal(&sb, "_Atomic ");
    }
    msb_write_str(&sb, type->name->data, type->name->length);
    {
        StringView sv = msb_borrow_sv(&sb);
        ctype->name = (Atom)AT_atomize(&ctx->at, sv.text, sv.length);
        msb_reset(&sb);
    }
    for(size_t i = 0; i < decl->pointer.count; i++){
        CPointerSpec* sp = &decl->pointer.data[i];
        CTypePointer* t = Allocator_zalloc(ctx->scratch, sizeof *t);
        t->kind = CT_POINTER;
        msb_write_str(&sb, ctype->name->data, ctype->name->length);
        msb_write_literal(&sb, " *");
        t->pointee = ctype;
        if(sp->spec & SPEC_CONST){
            t->const_ = 1;
            msb_write_literal(&sb, " const");
        }
        if(sp->spec & SPEC_RESTRICT){
            t->restrict_ = 1;
            msb_write_literal(&sb, " restrict");
        }
        if(sp->spec & SPEC_VOLATILE){
            t->volatile_ = 1;
            msb_write_literal(&sb, " volatile");
        }
        if(sp->spec & SPEC_ATOMIC){
            t->_Atomic_ = 1;
            msb_write_literal(&sb, " _Atomic");
        }
        StringView sv = msb_borrow_sv(&sb);
        t->name = (Atom)AT_atomize(&ctx->at, sv.text, sv.length);
        msb_reset(&sb);
        ctype = (CType*)t;
    }
    type = simp_intern_type(ctx, ctype);
    simp_c_logf(ctx, "decl: %s %.*s", type->name->data,  (int)decl->ident->content.length, decl->ident->content.text);

    goto finally;

    finally:
    msb_destroy(&sb);
    return err;
}

SIMP_C_LEX_API
Atom
#ifdef __GNUC__
__attribute__((format(printf, 2, 3)))
#endif
simp_c_atomf(CParseCtx* ctx, const char* fmt, ...){
    va_list ap;
    va_start(ap, fmt);
    Atom a = AT_atomize_fv(&ctx->at, fmt, ap);
    va_end(ap);
    return a;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
