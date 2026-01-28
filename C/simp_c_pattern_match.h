#ifndef SIMP_C_PATTERN_MATCH_H
#define SIMP_C_PATTERN_MATCH_H
#include "../../Drp/typed_enum.h"
#include "simp_c_lex.h"
#include "simp_c_match.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Null_unspecified
#define _Null_unspecified
#endif
#ifndef _Nullable
#define _Nullable
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


typedef struct SimpPatternMatchArg SimpPatternMatchArg;
struct SimpPatternMatchArg {
    enum TYPED_ENUM(uint32_t) {
        SPMK_INVALID = 0,
        SPMK_IDENT,
        SPMK_IDENT_OR_KW,
        SPMK_TAKE_IDENT,
        SPMK_TAKE_IDENT_OR_KW,
        SPMK_TAKE_CONST_OR_IDENT,
        SPMK_PUNCT,
        SPMK_BLOCK,
        SPMK_PARENS,
        SPMK_BRACKETS,
        SPMK_MACRO_ARG,
        SPMK_TAKE_MACRO_ARG,
        SPMK_MACRO_ARGS,
        SPMK_TAKE_MACRO_ARGS,
        SPMK_STR,
        SPMK_TAKE_STR,
        SPMK_OPTIONAL,
        SPMK_ALT,
        SPMK_SEQ,
        SPMK_ANY,
        SPMK_ANY_IDENT,
        SPMK_ANY_IDENT_KW,
        SPMK_CAPTURE,
        SPMK_TAKE_TOK,
        SPMK_REPEAT,
    } kind;
    union {
        CPunct punct;
        StringView ident;
        StringView* outident;
        StringView* outstring;
        struct {
            SimpPatternMatchArg* args;
            size_t count;
            _Bool*_Nullable matched;
        } optional;
        struct {
            SimpPatternMatchArg* args;
            size_t count;
            size_t*_Nullable which;
        } alt;
        struct {
            SimpPatternMatchArg* args;
            size_t count;
        } seq;
        struct {
            CToken*_Null_unspecified*_Nonnull start;
            CToken*_Null_unspecified*_Nonnull end;
        } macro;
        struct {
            CToken*_Null_unspecified*_Nonnull start;
            CToken*_Null_unspecified*_Nonnull end;
            SimpPatternMatchArg* inner;
            size_t count;
        } capture;
        struct {
            CToken*_Null_unspecified*_Nonnull outtok;
            SimpPatternMatchArg* inner;
        } take_tok;
        struct {
            SimpPatternMatchArg* inner;
            size_t count;
            size_t min;
            size_t max;
            size_t*_Nullable matched_count;
        } repeat;
    };
};

static const SimpPatternMatchArg SPM_BLOCK = {.kind = SPMK_BLOCK};
static const SimpPatternMatchArg SPM_PARENS = {.kind = SPMK_PARENS};
static const SimpPatternMatchArg SPM_BRACKETS = {.kind = SPMK_BRACKETS};
static const SimpPatternMatchArg SPM_MACRO_ARG = {.kind = SPMK_MACRO_ARG};
static const SimpPatternMatchArg SPM_MACRO_ARGS = {.kind = SPMK_MACRO_ARGS};
static const SimpPatternMatchArg SPM_STR = {.kind = SPMK_STR};
static const SimpPatternMatchArg SPM_ANY = {.kind = SPMK_ANY};
static const SimpPatternMatchArg SPM_ANY_IDENT_KW = {.kind = SPMK_ANY_IDENT_KW};
static const SimpPatternMatchArg SPM_ANY_IDENT = {.kind = SPMK_ANY_IDENT};

force_inline
SimpPatternMatchArg
SPM_str(StringView* outstring){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_STR,
        .outstring = outstring,
    };
}

force_inline
SimpPatternMatchArg
SPM_macro_arg(CToken*_Null_unspecified*_Nonnull start, CToken*_Null_unspecified*_Nonnull end){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_MACRO_ARG,
        .macro = {
            .start = start,
            .end = end,
        },
    };
}

force_inline
SimpPatternMatchArg
SPM_macro_args(CToken*_Null_unspecified*_Nonnull start, CToken*_Null_unspecified*_Nonnull end){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_MACRO_ARGS,
        .macro = {
            .start = start,
            .end = end,
        },
    };
}

force_inline
SimpPatternMatchArg
SPM_ident_kw(StringView* outident){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_IDENT_OR_KW,
        .outident = outident,
    };
}

force_inline
SimpPatternMatchArg
SPM_const(StringView* outident){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_CONST_OR_IDENT,
        .outident = outident,
    };
}

force_inline
SimpPatternMatchArg
SPM_opt_(_Bool*_Nullable matched, SimpPatternMatchArg* args, size_t count){
    return (SimpPatternMatchArg){
        .kind = SPMK_OPTIONAL,
        .optional = {args, count, matched},
    };
}

force_inline
SimpPatternMatchArg
SPM_alt_(SimpPatternMatchArg* args, size_t count){
    return (SimpPatternMatchArg){
        .kind = SPMK_ALT,
        .alt = {args, count, NULL},
    };
}

force_inline
SimpPatternMatchArg
SPM_alt_which_(size_t*_Nullable which, SimpPatternMatchArg* args, size_t count){
    return (SimpPatternMatchArg){
        .kind = SPMK_ALT,
        .alt = {args, count, which},
    };
}

force_inline
SimpPatternMatchArg
SPM_seq_(SimpPatternMatchArg* args, size_t count){
    return (SimpPatternMatchArg){
        .kind = SPMK_SEQ,
        .seq = {args, count},
    };
}

force_inline
SimpPatternMatchArg
SPM_cap_(CToken*_Null_unspecified*_Nonnull start, CToken*_Null_unspecified*_Nonnull end, SimpPatternMatchArg* inner, size_t count){
    return (SimpPatternMatchArg){
        .kind = SPMK_CAPTURE,
        .capture = {
            .start = start,
            .end = end,
            .inner = inner,
            .count = count,
        },
    };
}

force_inline
SimpPatternMatchArg
SPM_tok_(CToken*_Null_unspecified*_Nonnull outtok, SimpPatternMatchArg* inner){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_TOK,
        .take_tok = {
            .outtok = outtok,
            .inner = inner,
        },
    };
}

force_inline
SimpPatternMatchArg
SPM_repeat_(size_t*_Nullable matched_count, size_t min, size_t max, SimpPatternMatchArg* inner, size_t inner_count){
    return (SimpPatternMatchArg){
        .kind = SPMK_REPEAT,
        .repeat = {
            .inner = inner,
            .count = inner_count,
            .min = min,
            .max = max,
            .matched_count = matched_count,
        },
    };
}

force_inline
SimpPatternMatchArg
SPM_star_(size_t*_Nullable matched_count, SimpPatternMatchArg* inner, size_t inner_count){
    return SPM_repeat_(matched_count, 0, SIZE_MAX, inner, inner_count);
}

force_inline
SimpPatternMatchArg
SPM_plus_(size_t*_Nullable matched_count, SimpPatternMatchArg* inner, size_t inner_count){
    return SPM_repeat_(matched_count, 1, SIZE_MAX, inner, inner_count);
}

force_inline
SimpPatternMatchArg
spmp_punct(CPunct p){
    return (SimpPatternMatchArg){
        .kind = SPMK_PUNCT,
        .punct = p,
    };
}

force_inline
SimpPatternMatchArg
spmp_ident_kw_cstr(const char* literal, size_t size_including_nul){
    return (SimpPatternMatchArg){
        .kind = SPMK_IDENT_OR_KW,
        .ident = {
            .length = size_including_nul-1,
            .text = literal,
        },
    };
}
force_inline
SimpPatternMatchArg
spmp_ident_kw(StringView ident){
    return (SimpPatternMatchArg){
        .kind = SPMK_IDENT_OR_KW,
        .ident = ident,
    };
}
force_inline
SimpPatternMatchArg
spmp_take_ident(StringView* ident){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_IDENT,
        .outident = ident,
    };
}

force_inline
SimpPatternMatchArg
spmp_take_ident_kw(StringView* ident){
    return (SimpPatternMatchArg){
        .kind = SPMK_TAKE_IDENT_OR_KW,
        .outident = ident,
    };
}

// Helpers to strip off the sizeof 2nd arg.
force_inline SimpPatternMatchArg spmp_punct_(CPunct p, size_t unused){ (void)unused; return spmp_punct(p); }
force_inline SimpPatternMatchArg spmp_ident_kw_(StringView ident, size_t unused){(void)unused;return spmp_ident_kw(ident);}
force_inline SimpPatternMatchArg spmp_take_ident_(StringView* ident, size_t unused){(void)unused;return spmp_take_ident(ident);}
force_inline SimpPatternMatchArg spmp_take_ident_kw_(StringView* ident, size_t unused){(void)unused;return spmp_take_ident_kw(ident);}
force_inline SimpPatternMatchArg spmp_id_(SimpPatternMatchArg a, size_t unused){(void)unused; return a;}

// _Generic dispatch: int handles enum values, StringView for ident, StringView* for capture
#define spmp(x) _Generic((x), \
    SimpPatternMatchArg: spmp_id_, \
    int: spmp_punct_, \
    const char*: spmp_ident_kw_cstr, \
    char*: spmp_ident_kw_cstr, \
    StringView: spmp_ident_kw_, \
    StringView*: spmp_take_ident_kw_)((x), sizeof (x))

// MAP macro to apply spmp to each variadic argument (up to 16)
#define SPMP_MAP_1(x)       spmp(x)
#define SPMP_MAP_2(x, ...)  spmp(x), SPMP_MAP_1(__VA_ARGS__)
#define SPMP_MAP_3(x, ...)  spmp(x), SPMP_MAP_2(__VA_ARGS__)
#define SPMP_MAP_4(x, ...)  spmp(x), SPMP_MAP_3(__VA_ARGS__)
#define SPMP_MAP_5(x, ...)  spmp(x), SPMP_MAP_4(__VA_ARGS__)
#define SPMP_MAP_6(x, ...)  spmp(x), SPMP_MAP_5(__VA_ARGS__)
#define SPMP_MAP_7(x, ...)  spmp(x), SPMP_MAP_6(__VA_ARGS__)
#define SPMP_MAP_8(x, ...)  spmp(x), SPMP_MAP_7(__VA_ARGS__)
#define SPMP_MAP_9(x, ...)  spmp(x), SPMP_MAP_8(__VA_ARGS__)
#define SPMP_MAP_10(x, ...) spmp(x), SPMP_MAP_9(__VA_ARGS__)
#define SPMP_MAP_11(x, ...) spmp(x), SPMP_MAP_10(__VA_ARGS__)
#define SPMP_MAP_12(x, ...) spmp(x), SPMP_MAP_11(__VA_ARGS__)
#define SPMP_MAP_13(x, ...) spmp(x), SPMP_MAP_12(__VA_ARGS__)
#define SPMP_MAP_14(x, ...) spmp(x), SPMP_MAP_13(__VA_ARGS__)
#define SPMP_MAP_15(x, ...) spmp(x), SPMP_MAP_14(__VA_ARGS__)
#define SPMP_MAP_16(x, ...) spmp(x), SPMP_MAP_15(__VA_ARGS__)

#define SPMP_COUNT_1   1
#define SPMP_COUNT_2   2
#define SPMP_COUNT_3   3
#define SPMP_COUNT_4   4
#define SPMP_COUNT_5   5
#define SPMP_COUNT_6   6
#define SPMP_COUNT_7   7
#define SPMP_COUNT_8   8
#define SPMP_COUNT_9   9
#define SPMP_COUNT_10 10
#define SPMP_COUNT_11 11
#define SPMP_COUNT_12 12
#define SPMP_COUNT_13 13
#define SPMP_COUNT_14 14
#define SPMP_COUNT_15 15
#define SPMP_COUNT_16 16

#define SPMP_GET_MACRO(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,NAME,...) NAME
#define SPMP_MAP(...) SPMP_GET_MACRO(__VA_ARGS__, \
    SPMP_MAP_16,SPMP_MAP_15,SPMP_MAP_14,SPMP_MAP_13, \
    SPMP_MAP_12,SPMP_MAP_11,SPMP_MAP_10,SPMP_MAP_9, \
    SPMP_MAP_8,SPMP_MAP_7,SPMP_MAP_6,SPMP_MAP_5, \
    SPMP_MAP_4,SPMP_MAP_3,SPMP_MAP_2,SPMP_MAP_1,)(__VA_ARGS__)
#define SPMP_COUNT(...) SPMP_GET_MACRO(__VA_ARGS__, \
    SPMP_COUNT_16,SPMP_COUNT_15,SPMP_COUNT_14,SPMP_COUNT_13, \
    SPMP_COUNT_12,SPMP_COUNT_11,SPMP_COUNT_10,SPMP_COUNT_9, \
    SPMP_COUNT_8,SPMP_COUNT_7,SPMP_COUNT_6,SPMP_COUNT_5, \
    SPMP_COUNT_4,SPMP_COUNT_3,SPMP_COUNT_2,SPMP_COUNT_1,)

#define _simp_match_pieces(...) (SimpPatternMatchArg[]){ SPMP_MAP(__VA_ARGS__) }
#define _simp_match_count(...) SPMP_COUNT(__VA_ARGS__)

SIMP_C_MATCH_API
_Bool
simp_pattern_match_(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip, SimpPatternMatchArg* pieces, size_t count);

#define SPM_opt(matched, ...) SPM_opt_(matched, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_alt(...) SPM_alt_(_simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_alt_which(which, ...) SPM_alt_which_(which, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_seq(...) SPM_seq_(_simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_cap(s, e, ...) SPM_cap_(s, e, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_tok(outtok, x) SPM_tok_(outtok, (SimpPatternMatchArg[]){spmp(x)})
#define SPM_repeat(count, min, max, ...) SPM_repeat_(count, min, max, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_star(count, ...) SPM_star_(count, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define SPM_plus(count, ...) SPM_plus_(count, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))
#define simp_pattern_match(pit, end, skip, ...) \
    simp_pattern_match_(pit, end, skip, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))

// Extended version that returns pointer to failed arg for error reporting.
// Returns:
//   NULL = first element didn't match (not committed, p_it restored)
//   SPM_MATCHED = all elements matched (success)
//   pointer to failed SimpPatternMatchArg = error (p_it left at failed token)
static const SimpPatternMatchArg SPM_MATCHED_SENTINEL;
#define SPM_MATCHED (&SPM_MATCHED_SENTINEL)

SIMP_C_MATCH_API
SimpPatternMatchArg*_Nullable
simp_pattern_match_r_(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip, SimpPatternMatchArg* pieces, size_t count);

#define simp_pattern_match_r(pit, end, skip, ...) \
    simp_pattern_match_r_(pit, end, skip, _simp_match_pieces(__VA_ARGS__), _simp_match_count(__VA_ARGS__))

// Write human-readable description of what was expected into buf.
// Returns number of bytes written (excluding null terminator).
// Always null-terminates if bufsize > 0.
SIMP_C_MATCH_API
size_t
spm_expected_str(SimpPatternMatchArg* arg, char* buf, size_t bufsize);

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
