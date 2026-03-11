#ifndef CC_REPL_COMPLETION_H
#define CC_REPL_COMPLETION_H
//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#include "C/cc_parser.h"
#include "Drp/get_input.h"
#include "Drp/string_distances.h"
#include <stdlib.h> // malloc, free, qsort

#ifndef MARRAY_STRING_VIEW
#define MARRAY_STRING_VIEW
#define MARRAY_T StringView
#include "Drp/Marray.h"
#endif

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static _Bool is_ident_char(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9') || c == '_';
}

// Preprocessor directive names for tab completion (without the #).
static const StringView cc_directive_strs[] = {
    SV("if"), SV("ifdef"), SV("ifndef"),
    SV("elif"), SV("elifdef"), SV("elifndef"),
    SV("else"), SV("endif"),
    SV("define"), SV("undef"),
    SV("include"), SV("include_next"),
    SV("pragma"),
    SV("error"), SV("warning"),
    SV("line"),
};

// C keyword names for tab completion.
static const StringView cc_keyword_strs[] = {
    SV("do"), SV("if"),
    SV("for"), SV("int"),
    SV("true"), SV("long"), SV("char"), SV("auto"), SV("bool"),
    SV("else"), SV("enum"), SV("case"), SV("goto"), SV("void"),
    SV("break"), SV("false"), SV("float"), SV("const"), SV("short"),
    SV("union"), SV("while"),
    SV("double"), SV("extern"), SV("inline"), SV("return"), SV("signed"),
    SV("sizeof"), SV("static"), SV("struct"), SV("switch"), SV("typeof"),
    SV("alignas"), SV("alignof"), SV("default"), SV("typedef"),
    SV("nullptr"), SV("_Atomic"), SV("_BitInt"),
    SV("_Complex"), SV("continue"), SV("register"), SV("restrict"),
    SV("unsigned"), SV("volatile"), SV("_Generic"), SV("_Countof"),
    SV("_Float16"), SV("_Float32"), SV("_Float64"),
    SV("constexpr"), SV("_Noreturn"), SV("_Float128"),
    SV("_Imaginary"), SV("_Decimal32"), SV("_Decimal64"),
    SV("_Decimal128"),
    SV("thread_local"),
    SV("static_assert"), SV("typeof_unqual"),
};

struct ReplCompleterCtx {
    CcParser* parser;
    Marray(StringView) ordered;
    size_t word_start; // saved from first tab
};

struct CompletionPair {
    StringView name;
    ssize_t distance;
    ssize_t idistance;
    _Bool is_prefix;  // candidate starts with needle
    _Bool is_iprefix; // case-insensitive prefix
};

static
int
completion_pair_cmp(const void* a, const void* b){
    const struct CompletionPair* pa = a;
    const struct CompletionPair* pb = b;
    // Prefix matches first.
    if(pa->is_prefix != pb->is_prefix) return pa->is_prefix ? -1 : 1;
    if(pa->is_iprefix != pb->is_iprefix) return pa->is_iprefix ? -1 : 1;
    // Then by distance.
    if(pa->distance < pb->distance) return -1;
    if(pa->distance > pb->distance) return 1;
    if(pa->idistance < pb->idistance) return -1;
    if(pa->idistance > pb->idistance) return 1;
    return 0;
}

static
int
repl_tab_complete(GetInputCtx* ctx, size_t orig_cursor, size_t orig_len, int n_tabs){
    struct ReplCompleterCtx* rctx = ctx->tab_completion_user_data;
    CcParser* parser = rctx->parser;

    if(n_tabs == 1){
        rctx->ordered.count = 0;

        // Find word start by scanning backward from cursor.
        size_t word_start = orig_cursor;
        while(word_start > 0 && is_ident_char(ctx->altbuff[word_start - 1]))
            word_start--;
        rctx->word_start = word_start;

        const char* needle = ctx->altbuff + word_start;
        size_t needle_len = orig_cursor - word_start;
        if(!needle_len) return 0;

        // Check if there's a # before the word (skip whitespace).
        _Bool is_directive = 0;
        {
            size_t p = word_start;
            while(p > 0 && ctx->altbuff[p-1] == ' ') p--;
            if(p > 0 && ctx->altbuff[p-1] == '#') is_directive = 1;
            else if(p == 0 && word_start == 0) is_directive = 0; // no #
        }

        // Helper macro to collect from a StringView array
        #define COLLECT_FROM_TABLE(table, count) do { \
            for(size_t i = 0; i < (count); i++){ \
                StringView sv = (table)[i]; \
                ssize_t dist = byte_expansion_distance(sv.text, sv.length, needle, needle_len); \
                ssize_t idist = byte_expansion_distance_icase(sv.text, sv.length, needle, needle_len); \
                if(idist < 0) continue; \
                if(dist < 0) dist = (ssize_t)sv.length; \
                _Bool ip = sv.length >= needle_len && !memcmp(sv.text, needle, needle_len); \
                _Bool iip = sv.length >= needle_len && !byte_expansion_distance_icase(sv.text, needle_len, needle, needle_len); \
                pairs[n++] = (struct CompletionPair){sv, dist, idist, ip, iip}; \
            } \
        } while(0)

        // Helper macro to collect from an AtomMapItems
        #define COLLECT_FROM_ATOMMAP(items) do { \
            for(size_t i = 0; i < (items).count; i++){ \
                Atom a = (items).data[i].atom; \
                if(!a || !a->length) continue; \
                ssize_t dist = byte_expansion_distance(a->data, a->length, needle, needle_len); \
                ssize_t idist = byte_expansion_distance_icase(a->data, a->length, needle, needle_len); \
                if(idist < 0) continue; \
                if(dist < 0) dist = (ssize_t)a->length; \
                _Bool ip = a->length >= needle_len && !memcmp(a->data, needle, needle_len); \
                _Bool iip = a->length >= needle_len && !byte_expansion_distance_icase(a->data, needle_len, needle, needle_len); \
                pairs[n++] = (struct CompletionPair){{a->length, a->data}, dist, idist, ip, iip}; \
            } \
        } while(0)

        size_t max_candidates = 0;
        enum { N_KEYWORDS = sizeof cc_keyword_strs / sizeof cc_keyword_strs[0] };
        enum { N_DIRECTIVES = sizeof cc_directive_strs / sizeof cc_directive_strs[0] };

        if(is_directive)
            max_candidates = N_DIRECTIVES;
        else {
            max_candidates += N_KEYWORDS;
            AtomMapItems mi = AM_items(&parser->cpp.macros);
            max_candidates += mi.count;
            max_candidates += AM_items(&parser->global.typedefs).count;
            max_candidates += AM_items(&parser->global.variables).count;
            max_candidates += AM_items(&parser->global.functions).count;
            max_candidates += AM_items(&parser->global.enumerators).count;
            max_candidates += AM_items(&parser->global.structs).count;
            max_candidates += AM_items(&parser->global.unions).count;
            max_candidates += AM_items(&parser->global.enums).count;
        }

        struct CompletionPair* pairs = malloc(max_candidates * sizeof *pairs);
        if(!pairs) return 0;
        size_t n = 0;

        if(is_directive)
            COLLECT_FROM_TABLE(cc_directive_strs, N_DIRECTIVES);
        else {
            COLLECT_FROM_TABLE(cc_keyword_strs, N_KEYWORDS);
            COLLECT_FROM_ATOMMAP(AM_items(&parser->cpp.macros));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.typedefs));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.variables));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.functions));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.enumerators));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.structs));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.unions));
            COLLECT_FROM_ATOMMAP(AM_items(&parser->global.enums));
        }

        #undef COLLECT_FROM_TABLE
        #undef COLLECT_FROM_ATOMMAP

        qsort(pairs, n, sizeof *pairs, completion_pair_cmp);

        // Store sorted names into ordered array.
        for(size_t i = 0; i < n; i++){
            int e = ma_push(StringView)(&rctx->ordered, MALLOCATOR, pairs[i].name);
            if(e) break;
        }
        free(pairs);
    }

    if(n_tabs <= 0 || !rctx->ordered.count){
        // Restore original text.
        memcpy(ctx->buff, ctx->altbuff, orig_len);
        ctx->buff[orig_len] = 0;
        ctx->buff_count = orig_len;
        ctx->buff_cursor = orig_cursor;
        return 0;
    }

    size_t idx = (size_t)(n_tabs - 1) % rctx->ordered.count;
    StringView match = rctx->ordered.data[idx];
    size_t word_start = rctx->word_start;
    size_t tail_len = orig_len - orig_cursor;
    size_t new_len = word_start + match.length + tail_len;
    if(new_len >= GI_BUFF_SIZE - 1)
        return 0;

    // Build: prefix + match + suffix
    memcpy(ctx->buff, ctx->altbuff, word_start);
    memcpy(ctx->buff + word_start, match.text, match.length);
    memcpy(ctx->buff + word_start + match.length, ctx->altbuff + orig_cursor, tail_len);
    ctx->buff[new_len] = 0;
    ctx->buff_count = new_len;
    ctx->buff_cursor = word_start + match.length;
    return 0;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif
#endif
