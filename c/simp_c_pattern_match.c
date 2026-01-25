#ifndef SIMP_C_PATTERN_MATCH_C
#define SIMP_C_PATTERN_MATCH_C
#include <string.h>
#include "simp_c_pattern_match.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

#ifndef __builtin_debugtrap
#if defined(__GNUC__) && ! defined(__clang__)
#define __builtin_debugtrap() __builtin_trap()
#elif defined(_MSC_VER)
#define __builtin_debugtrap() __debugbreak()
#endif
#endif

static const SimpPatternMatchArg SPM_MATCHED_SENTINEL = {.kind = SPMK_INVALID};

SIMP_C_MATCH_API
_Bool
simp_pattern_match_(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip, SimpPatternMatchArg* pieces, size_t count){
    CToken* start = *p_it;
    for(size_t i = 0; i < count; i++){
        switch(pieces[i].kind){
            case SPMK_INVALID:
                __builtin_debugtrap();
                return 0;
            case SPMK_PUNCT:
                if(!simp_match_punct(p_it, end, pieces[i].punct, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_IDENT:
                if(!simp_match_ident(p_it, end, pieces[i].ident, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_IDENT_OR_KW:
                if(!simp_match_ident_or_kw(p_it, end, pieces[i].ident, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_TAKE_IDENT:
                if(!simp_take_ident(p_it, end, pieces[i].outident, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_TAKE_IDENT_OR_KW:
                if(!simp_take_ident_or_kw(p_it, end, pieces[i].outident, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_TAKE_CONST_OR_IDENT:
                if(!simp_take_constant_or_ident(p_it, end, pieces[i].outident, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_BLOCK:
                if(!simp_match_block(p_it, end, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_PARENS:
                if(!simp_match_balanced_sequence(p_it, end, '(', skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_BRACKETS:
                if(!simp_match_balanced_sequence(p_it, end, '[', skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_MACRO_ARG:
                if(!simp_match_macro_arg(p_it, end)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_MACRO_ARGS:
                if(!simp_match_macro_args(p_it, end)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_TAKE_MACRO_ARG:{
                CToken* it = *p_it;
                if(!simp_match_macro_arg(p_it, end)){
                    *p_it = start;
                    return 0;
                }
                *pieces[i].macro.start = it;
                *pieces[i].macro.end = *p_it;
            }break;
            case SPMK_TAKE_MACRO_ARGS:{
                CToken* it = *p_it;
                if(!simp_match_macro_args(p_it, end)){
                    *p_it = start;
                    return 0;
                }
                *pieces[i].macro.start = it;
                *pieces[i].macro.end = *p_it;
            }break;
            case SPMK_STR:
                if(!simp_match_string(p_it, end, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_TAKE_STR:
                if(!simp_take_string(p_it, end, pieces[i].outstring, skip)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_OPTIONAL:{
                CToken* it = *p_it;
                _Bool did_match = simp_pattern_match_(p_it, end, skip, pieces[i].optional.args, pieces[i].optional.count);
                if(!did_match) *p_it = it;
                if(pieces[i].optional.matched) *pieces[i].optional.matched = did_match;
                break;
            }
            case SPMK_ALT:{
                _Bool did_match = 0;
                for(size_t j = 0; j < pieces[i].alt.count; j++){
                    CToken* save = *p_it;
                    if(simp_pattern_match_(p_it, end, skip, &pieces[i].alt.args[j], 1)){
                        did_match = 1;
                        if(pieces[i].alt.which) *pieces[i].alt.which = j;
                        break;
                    }
                    *p_it = save;
                }
                if(!did_match){
                    *p_it = start;
                    return 0;
                }
                break;
            }
            case SPMK_SEQ:
                if(!simp_pattern_match_(p_it, end, skip, pieces[i].seq.args, pieces[i].seq.count)){
                    *p_it = start;
                    return 0;
                }
                break;
            case SPMK_ANY: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || it->type == CTOK_EOF){
                    *p_it = start;
                    return 0;
                }
                *p_it = it + 1;
                break;
            }
            case SPMK_ANY_IDENT: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || (it->type != CTOK_IDENTIFIER)){
                    *p_it = start;
                    return 0;
                }
                *p_it = it + 1;
                break;
            }
            case SPMK_ANY_IDENT_KW: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || (it->type != CTOK_IDENTIFIER && it->type != CTOK_KEYWORD)){
                    *p_it = start;
                    return 0;
                }
                *p_it = it + 1;
                break;
            }
            case SPMK_CAPTURE: {
                CToken* cap_start = *p_it;
                simp_skip(&cap_start, end, skip);
                if(!simp_pattern_match_(p_it, end, skip, pieces[i].capture.inner, pieces[i].capture.count)){
                    *p_it = start;
                    return 0;
                }
                *pieces[i].capture.start = cap_start;
                *pieces[i].capture.end = *p_it;
                break;
            }
            case SPMK_TAKE_TOK: {
                CToken* tok_start = *p_it;
                simp_skip(&tok_start, end, skip);
                if(!simp_pattern_match_(p_it, end, skip, pieces[i].take_tok.inner, 1)){
                    *p_it = start;
                    return 0;
                }
                *pieces[i].take_tok.outtok = tok_start;
                break;
            }
            case SPMK_REPEAT: {
                size_t rep_count = 0;
                while(rep_count < pieces[i].repeat.max){
                    CToken* save = *p_it;
                    if(!simp_pattern_match_(p_it, end, skip, pieces[i].repeat.inner, pieces[i].repeat.count)){
                        *p_it = save;
                        break;
                    }
                    rep_count++;
                }
                if(rep_count < pieces[i].repeat.min){
                    *p_it = start;
                    return 0;
                }
                if(pieces[i].repeat.matched_count) *pieces[i].repeat.matched_count = rep_count;
                break;
            }
        }
    }
    return 1;
}

SIMP_C_MATCH_API
SimpPatternMatchArg*_Nullable
simp_pattern_match_r_(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip, SimpPatternMatchArg* pieces, size_t count){
    CToken* start = *p_it;
    for(size_t i = 0; i < count; i++){
        _Bool matched = 0;
        switch(pieces[i].kind){
            case SPMK_INVALID:
                __builtin_debugtrap();
                return &pieces[i];
            case SPMK_PUNCT:
                matched = simp_match_punct(p_it, end, pieces[i].punct, skip);
                break;
            case SPMK_IDENT:
                matched = simp_match_ident(p_it, end, pieces[i].ident, skip);
                break;
            case SPMK_IDENT_OR_KW:
                matched = simp_match_ident_or_kw(p_it, end, pieces[i].ident, skip);
                break;
            case SPMK_TAKE_IDENT:
                matched = simp_take_ident(p_it, end, pieces[i].outident, skip);
                break;
            case SPMK_TAKE_IDENT_OR_KW:
                matched = simp_take_ident_or_kw(p_it, end, pieces[i].outident, skip);
                break;
            case SPMK_TAKE_CONST_OR_IDENT:
                matched = simp_take_constant_or_ident(p_it, end, pieces[i].outident, skip);
                break;
            case SPMK_BLOCK:
                matched = simp_match_block(p_it, end, skip);
                break;
            case SPMK_PARENS:
                matched = simp_match_balanced_sequence(p_it, end, '(', skip);
                break;
            case SPMK_BRACKETS:
                matched = simp_match_balanced_sequence(p_it, end, '[', skip);
                break;
            case SPMK_MACRO_ARG:
                matched = simp_match_macro_arg(p_it, end);
                break;
            case SPMK_MACRO_ARGS:
                matched = simp_match_macro_args(p_it, end);
                break;
            case SPMK_TAKE_MACRO_ARG:{
                CToken* it = *p_it;
                if(!simp_match_macro_arg(p_it, end)){
                    matched = 0;
                    break;
                }
                *pieces[i].macro.start = it;
                *pieces[i].macro.end = *p_it;
                matched = 1;
            }break;
            case SPMK_TAKE_MACRO_ARGS:{
                CToken* it = *p_it;
                if(!simp_match_macro_args(p_it, end)){
                    matched = 0;
                    break;
                }
                *pieces[i].macro.start = it;
                *pieces[i].macro.end = *p_it;
                matched = 1;
            }break;
            case SPMK_STR:
                matched = simp_match_string(p_it, end, skip);
                break;
            case SPMK_TAKE_STR:
                matched = simp_take_string(p_it, end, pieces[i].outstring, skip);
                break;
            case SPMK_OPTIONAL:{
                CToken* it = *p_it;
                _Bool did_match = simp_pattern_match_(p_it, end, skip, pieces[i].optional.args, pieces[i].optional.count);
                if(!did_match) *p_it = it;
                if(pieces[i].optional.matched) *pieces[i].optional.matched = did_match;
                matched = 1;
                break;
            }
            case SPMK_ALT:{
                for(size_t j = 0; j < pieces[i].alt.count; j++){
                    CToken* save = *p_it;
                    if(simp_pattern_match_(p_it, end, skip, &pieces[i].alt.args[j], 1)){
                        matched = 1;
                        if(pieces[i].alt.which) *pieces[i].alt.which = j;
                        break;
                    }
                    *p_it = save;
                }
                break;
            }
            case SPMK_SEQ:
                matched = simp_pattern_match_(p_it, end, skip, pieces[i].seq.args, pieces[i].seq.count);
                break;
            case SPMK_ANY: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || it->type == CTOK_EOF){
                    matched = 0;
                    break;
                }
                *p_it = it + 1;
                matched = 1;
                break;
            }
            case SPMK_ANY_IDENT: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || (it->type != CTOK_IDENTIFIER)){
                    matched = 0;
                    break;
                }
                *p_it = it + 1;
                matched = 1;
                break;
            }
            case SPMK_ANY_IDENT_KW: {
                CToken* it = *p_it;
                simp_skip(&it, end, skip);
                if(it >= end || (it->type != CTOK_IDENTIFIER && it->type != CTOK_KEYWORD)){
                    matched = 0;
                    break;
                }
                *p_it = it + 1;
                matched = 1;
                break;
            }
            case SPMK_CAPTURE: {
                CToken* cap_start = *p_it;
                simp_skip(&cap_start, end, skip);
                if(!simp_pattern_match_(p_it, end, skip, pieces[i].capture.inner, pieces[i].capture.count)){
                    matched = 0;
                    break;
                }
                *pieces[i].capture.start = cap_start;
                *pieces[i].capture.end = *p_it;
                matched = 1;
                break;
            }
            case SPMK_TAKE_TOK: {
                CToken* tok_start = *p_it;
                simp_skip(&tok_start, end, skip);
                if(!simp_pattern_match_(p_it, end, skip, pieces[i].take_tok.inner, 1)){
                    matched = 0;
                    break;
                }
                *pieces[i].take_tok.outtok = tok_start;
                matched = 1;
                break;
            }
            case SPMK_REPEAT: {
                size_t rep_count = 0;
                while(rep_count < pieces[i].repeat.max){
                    CToken* save = *p_it;
                    if(!simp_pattern_match_(p_it, end, skip, pieces[i].repeat.inner, pieces[i].repeat.count)){
                        *p_it = save;
                        break;
                    }
                    rep_count++;
                }
                if(rep_count < pieces[i].repeat.min){
                    matched = 0;
                    break;
                }
                if(pieces[i].repeat.matched_count) *pieces[i].repeat.matched_count = rep_count;
                matched = 1;
                break;
            }
        }
        if(!matched){
            if(i == 0){
                *p_it = start;
                return NULL;
            }
            return &pieces[i];
        }
    }
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wcast-qual"
    #endif
    return (SimpPatternMatchArg*)SPM_MATCHED;
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
}

SIMP_C_MATCH_API
size_t
spm_expected_str(SimpPatternMatchArg* arg, char* buf, size_t bufsize){
    size_t n = 0;
    #define WRITE(s, len) do { \
        if(n + (len) < bufsize) memcpy(buf + n, s, len); \
        n += (len); \
    } while(0)
    #define WRITE_LIT(lit) WRITE("" lit, sizeof(lit) - 1)

    switch(arg->kind){
        case SPMK_INVALID:
            WRITE_LIT("(invalid)");
            break;
        case SPMK_IDENT_OR_KW:
        case SPMK_IDENT:
            WRITE_LIT("'");
            WRITE(arg->ident.text, arg->ident.length);
            WRITE_LIT("'");
            break;
        case SPMK_TAKE_IDENT:
        case SPMK_TAKE_IDENT_OR_KW:
            WRITE_LIT("identifier");
            break;
        case SPMK_TAKE_CONST_OR_IDENT:
            WRITE_LIT("constant or identifier");
            break;
        case SPMK_PUNCT: {
            WRITE_LIT("'");
            CPunct p = arg->punct;
            if(p <= 0xff)
                WRITE(&p, 1);
            else if(p <= 0xffff)
                WRITE(&p, 2);
            else
                WRITE(&p, 3);
            WRITE_LIT("'");
            break;
        }
        case SPMK_PARENS:
            WRITE_LIT("(...)");
            break;
        case SPMK_BRACKETS:
            WRITE_LIT("[...]");
            break;
        case SPMK_BLOCK:
            WRITE_LIT("{...} (block)");
            break;
        case SPMK_TAKE_MACRO_ARG:
        case SPMK_MACRO_ARG:
            WRITE_LIT("macro argument");
            break;
        case SPMK_TAKE_MACRO_ARGS:
        case SPMK_MACRO_ARGS:
            WRITE_LIT("macro arguments");
            break;
        case SPMK_STR:
        case SPMK_TAKE_STR:
            WRITE_LIT("\"...\" (string literal)");
            break;
        case SPMK_OPTIONAL:
            WRITE_LIT("optional ");
            for(size_t i = 0; i < arg->optional.count; i++){
                if(i > 0) WRITE_LIT(" ");
                size_t written = spm_expected_str(&arg->optional.args[i], buf + n, bufsize > n ? bufsize - n : 0);
                n += written;
            }
            break;
        case SPMK_ALT:
            WRITE_LIT("one of: ");
            for(size_t i = 0; i < arg->alt.count; i++){
                if(i > 0) WRITE_LIT(", ");
                size_t written = spm_expected_str(&arg->alt.args[i], buf + n, bufsize > n ? bufsize - n : 0);
                n += written;
            }
            break;
        case SPMK_SEQ:
            for(size_t i = 0; i < arg->seq.count; i++){
                if(i > 0) WRITE_LIT(" ");
                size_t written = spm_expected_str(&arg->seq.args[i], buf + n, bufsize > n ? bufsize - n : 0);
                n += written;
            }
            break;
        case SPMK_ANY:
            WRITE_LIT("any token");
            break;
        case SPMK_ANY_IDENT:
            WRITE_LIT("any ident");
            break;
        case SPMK_ANY_IDENT_KW:
            WRITE_LIT("any ident or kw");
            break;
        case SPMK_CAPTURE:
            WRITE_LIT("capture ");
            for(size_t i = 0; i < arg->capture.count; i++){
                if(i > 0) WRITE_LIT(" ");
                size_t written = spm_expected_str(&arg->capture.inner[i], buf + n, bufsize > n ? bufsize - n : 0);
                n += written;
            }
            break;
        case SPMK_TAKE_TOK: {
            size_t written = spm_expected_str(arg->take_tok.inner, buf + n, bufsize > n ? bufsize - n : 0);
            n += written;
            break;
        }
        case SPMK_REPEAT:
            if(arg->repeat.min == 0)
                WRITE_LIT("zero or more of ");
            else if(arg->repeat.min == 1)
                WRITE_LIT("one or more of ");
            else {
                WRITE_LIT("FIXME: some number of ");
            }
            for(size_t i = 0; i < arg->repeat.count; i++){
                if(i > 0) WRITE_LIT(" ");
                size_t written = spm_expected_str(&arg->repeat.inner[i], buf + n, bufsize > n ? bufsize - n : 0);
                n += written;
            }
            break;
    }
    if(bufsize > 0) buf[n < bufsize ? n : bufsize - 1] = '\0';
    #undef WRITE
    #undef WRITE_LIT
    return n;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
