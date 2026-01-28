#ifndef SIMP_C_MATCH_C
#define SIMP_C_MATCH_C
#include "simp_c_match.h"
#include "../../Drp/parse_numbers.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

SIMP_C_MATCH_API
void
simp_skip_ws_preproc(CToken*_Nonnull*_Nonnull p_it, CToken* end){
    while(*p_it < end && ((*p_it)->type == CTOK_WHITESPACE || (*p_it)->type == CTOK_COMMENT || (*p_it)->type == CTOK_PREPROC))
        *p_it = *p_it + 1;
}

SIMP_C_MATCH_API
void
simp_skip_ws_no_preproc(CToken*_Nonnull*_Nonnull p_it, CToken* end){
    while(*p_it < end && ((*p_it)->type == CTOK_WHITESPACE || (*p_it)->type == CTOK_COMMENT))
        *p_it = *p_it + 1;
}

SIMP_C_MATCH_API
void
simp_skip(CToken*_Nonnull *_Nonnull p_it, CToken* end, SimpSkip skip){
    if(!skip) return;
    if(skip & SIMP_SKIP_PREPROC)
        simp_skip_ws_preproc(p_it, end);
    else
        simp_skip_ws_no_preproc(p_it, end);
}

SIMP_C_MATCH_API
CToken*_Nullable
simp_match_token_type(CToken* _Nonnull*_Nonnull p_it, CToken* end, CTokenType type, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type != type) return NULL;
    (*p_it)++;
    return it;
}


SIMP_C_MATCH_API
_Bool
simp_match_punct(CToken*_Nonnull*_Nonnull p_it, CToken* end, int punct, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type != CTOK_PUNCTUATOR) return 0;
    if(it->subtype != punct) return 0;
    (*p_it)++;
    return 1;
}

SIMP_C_MATCH_API
_Bool
simp_match_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView word, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type != CTOK_IDENTIFIER) return 0;
    if(!sv_equals(it->content, word)) return 0;
    (*p_it)++;
    return 1;
}

SIMP_C_MATCH_API
_Bool
simp_match_ident_or_kw(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView word, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type != CTOK_IDENTIFIER && it->type != CTOK_KEYWORD) return 0;
    if(!sv_equals(it->content, word)) return 0;
    (*p_it)++;
    return 1;
}

SIMP_C_MATCH_API
_Bool
simp_take_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip skip){
    CToken* it = simp_match_token_type(p_it, end, CTOK_IDENTIFIER, skip);
    if(!it) return 0;
    *word = it->content;
    return 1;
}

SIMP_C_MATCH_API
_Bool
simp_take_ident_or_kw(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip skip){
    CToken* it = simp_match_token_type(p_it, end, CTOK_IDENTIFIER, skip);
    if(!it) it = simp_match_token_type(p_it, end, CTOK_KEYWORD, skip);
    if(!it) return 0;
    *word = it->content;
    return 1;
}

SIMP_C_MATCH_API
_Bool
simp_take_ident_or_number(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* outword, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type == CTOK_IDENTIFIER || (it->type == CTOK_CONSTANT && it->subtype == CC_INTEGER)){
        *outword = it->content;
        *p_it = it+1;
        return 1;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_take_string(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* outword, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type == CTOK_STRING_LITERAL){
        // Strip quotes
        outword->text = it->content.text + 1;
        outword->length = it->content.length - 2;
        *p_it = it+1;
        return 1;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_string(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type == CTOK_STRING_LITERAL){
        *p_it = it+1;
        return 1;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_block(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip){
    CToken* it = *p_it;
    if(!simp_match_punct(p_it, end, '{', skip))
        return 0;
    int brace_depth = 1;
    while(*p_it < end){
        simp_skip_ws_preproc(p_it, end);
        if(simp_match_punct(p_it, end, '{', SIMP_SKIP_WS_COMMENTS)){
            brace_depth++;
            continue;
        }
        if(simp_match_punct(p_it, end, '}', SIMP_SKIP_WS_COMMENTS)){
            brace_depth--;
            if(!brace_depth) return 1;
            continue;
        }
        (*p_it)++;
    }
    *p_it = it;
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_macro_arg(CToken*_Nonnull*_Nonnull p_it, CToken* end){
    int paren = 0;
    CToken* it = *p_it;
    while(it != end){
        if(simp_match_punct(&it, end, '(', SIMP_SKIP_WS_COMMENTS_PREPROC)){
            paren++;
            continue;
        }
        if(simp_match_punct(&it, end, ')', SIMP_SKIP_WS_COMMENTS_PREPROC)){
            if(!paren){
                *p_it = it-1; // leave on the ')'
                return 1;
            }
            paren--;
            continue;
        }
        if(!paren && simp_match_punct(&it, end, ',', SIMP_SKIP_WS_COMMENTS_PREPROC)){
            *p_it = it-1; // leave on the ','
            return 1;
        }
        it++;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_macro_args(CToken*_Nonnull*_Nonnull p_it, CToken* end){
    int paren = 0;
    CToken* it = *p_it;
    while(it != end){
        if(simp_match_punct(&it, end, '(', SIMP_SKIP_WS_COMMENTS_PREPROC)){
            paren++;
            continue;
        }
        if(simp_match_punct(&it, end, ')', SIMP_SKIP_WS_COMMENTS_PREPROC)){
            if(!paren){
                *p_it = it-1; // leave on the ')'
                return 1;
            }
            paren--;
            continue;
        }
        it++;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_balanced_sequence(CToken*_Nonnull*_Nonnull p_it, CToken* end, int start_punct, SimpSkip skip){
    int end_punct;
    switch(start_punct){
        case '{':
            end_punct = '}';
            break;
        case '(':
            end_punct = ')';
            break;
        case '[':
            end_punct = ']';
            break;
        default:
            __builtin_debugtrap();
            return 0;
    }
    CToken* it = *p_it;
    if(!simp_match_punct(p_it, end, start_punct, skip))
        return 0;
    int brace_depth = 1;
    while(*p_it < end){
        simp_skip_ws_preproc(p_it, end);
        if(simp_match_punct(p_it, end, start_punct, SIMP_SKIP_WS_COMMENTS)){
            brace_depth++;
            continue;
        }
        if(simp_match_punct(p_it, end, end_punct, SIMP_SKIP_WS_COMMENTS)){
            brace_depth--;
            if(!brace_depth) return 1;
            continue;
        }
        (*p_it)++;
    }
    *p_it = it;
    return 0;
}

SIMP_C_MATCH_API
void
simp_find_line_col(SimpSrcLoc* last_loc, StringView src, CToken* tok){
    int line = 1;
    int col = 1;
    const char* p = src.text;
    const char* end = tok->content.text;
    if(last_loc->p && last_loc->p >= p && last_loc->p <= end){
        line = last_loc->line;
        col = last_loc->col;
        p = last_loc->p;
    }
    while(p < end){
        switch(*p){
            case '\n': line++; col = 1; break;
            default: col++; break;
        }
        p++;
    }
    last_loc->col = col;
    last_loc->line = line;
    last_loc->p = p;
}

SIMP_C_MATCH_API
_Bool
simp_take_string_literal(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type == CTOK_STRING_LITERAL){
        *word = it->content;
        *p_it = it+1;
        return 1;
    }
    return 0;
}

SIMP_C_MATCH_API
_Bool
simp_match_any_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, SimpSkip skip){
    CToken* it = simp_match_token_type(p_it, end, CTOK_IDENTIFIER, skip);
    return it != NULL;
}

SIMP_C_MATCH_API
_Bool
simp_take_constant_or_ident(CToken*_Nonnull*_Nonnull p_it, CToken* end, StringView* word, SimpSkip skip){
    simp_skip(p_it, end, skip);
    CToken* it = *p_it;
    if(it >= end) return 0;
    if(it->type == CTOK_CONSTANT || it->type == CTOK_IDENTIFIER){
        *word = it->content;
        *p_it = it+1;
        return 1;
    }
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
