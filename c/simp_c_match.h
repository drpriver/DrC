#ifndef SIMP_C_MATCH_H
#define SIMP_C_MATCH_H
#include "../../Drp/stringview.h"
#include "../../Drp/typed_enum.h"
#include "simp_c_lex.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#else
#ifndef _Nonnull
#define _Nonnull
#endif
#ifndef _Nullable
#define _Nullable
#endif
#endif

#ifndef SIMP_C_MATCH_API
#define SIMP_C_MATCH_API static
#endif

enum SimpSkip TYPED_ENUM(unsigned) {
    SIMP_SKIP_NONE = 0x0,
    SIMP_SKIP_WS = 0x1,
    SIMP_SKIP_COMMENTS = 0x2,
    SIMP_SKIP_PREPROC = 0x4,
    SIMP_SKIP_WS_COMMENTS = SIMP_SKIP_WS | SIMP_SKIP_COMMENTS,
    SIMP_SKIP_WS_COMMENTS_PREPROC = SIMP_SKIP_COMMENTS | SIMP_SKIP_WS | SIMP_SKIP_PREPROC,
};
TYPEDEF_ENUM(SimpSkip, unsigned);

typedef struct SimpSrcLoc SimpSrcLoc;
struct SimpSrcLoc {
    const char* p;
    int line, col;
};

SIMP_C_MATCH_API
void
simp_find_line_col(SimpSrcLoc* last_loc, StringView src, CToken* tok);

SIMP_C_MATCH_API
void
simp_skip(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip);

SIMP_C_MATCH_API
void
simp_skip_ws_preproc(CToken*_Nonnull*_Nonnull p_it, CToken* end);

SIMP_C_MATCH_API
void
simp_skip_ws_no_preproc(CToken*_Nonnull*_Nonnull p_it, CToken* end);

SIMP_C_MATCH_API
_Bool
simp_match_punct(CToken*_Nonnull*_Nonnull p_it, CToken* end, int punct, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_match_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_match_ident_or_kw(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_match_block(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip);

// Matches a sequence of tokens terminated by a comma (does not consume the comma),
// but commas inside balanced parens are correctly handled, like for
// an argument to a function like macro.
SIMP_C_MATCH_API
_Bool
simp_match_macro_arg(CToken*_Nonnull*_Nonnull p_it, CToken* end);

// Matches a sequence of tokens terminated by an rparen (does not consume the rparen)
SIMP_C_MATCH_API
_Bool
simp_match_macro_args(CToken*_Nonnull*_Nonnull p_it, CToken* end);

SIMP_C_MATCH_API
_Bool
simp_match_balanced_sequence(CToken*_Nonnull*_Nonnull p_it, CToken* end, int start_punct, SimpSkip);

SIMP_C_MATCH_API
CToken*_Nullable
simp_match_token_type(CToken* _Nonnull*_Nonnull p_it, CToken* end, CTokenType type, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_ident_or_kw(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_ident_or_number(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_match_string(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_string(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_string_literal(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_match_any_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip);

SIMP_C_MATCH_API
_Bool
simp_take_constant_or_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip);


#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
