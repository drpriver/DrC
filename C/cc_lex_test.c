//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#include "../Drp/Allocators/testing_allocator.h"
#include "../Drp/testing.h"
#include "../Drp/Allocators/mallocator.h"
#include "../Drp/Allocators/arena_allocator.h"
#include "../Drp/env.h"
#include "../Drp/atom_table.h"
#include "../Drp/stdlogger.h"
#include "../Drp/MStringBuilder.h"
#include "../Drp/file_cache.h"
#include "../Drp/msb_logger.h"
#include "cpp_tok.h"
#include "cc_tok.h"
#include "cc_lexer.h"

#include "../Drp/compiler_warnings.h"
#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static int cpp_next_pp_token(CPreprocessor* cpp, CppToken* ptok);

enum { MAX_TEST_TOKENS = 64 };

// Helper: lex a string into an array of CcTokens, return count or -1 on error.
static
int
cc_lex_string(StringView txt, CcToken (*out)[MAX_TEST_TOKENS], int* count, const char* file, const char* func, int line){
    int result = 0;
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {.allocator = a};
    Environment env = {.allocator = a, .at=&at};
    int err;
    CcLexer lexer = {
        .cpp = {
            .allocator = a,
            .fc = fc,
            .at = &at,
            .logger = logger,
            .env = &env,
            .target = cc_target_test(),
        },
    };
    fc_write_path(fc, "(test)", 6);
    err = fc_cache_file(fc, txt);
    if(err){ result = 1; goto finally; }
    err = cpp_define_builtin_macros(&lexer.cpp);
    if(err){ result = 1; goto finally; }
    err = cpp_include_file_via_file_cache(&lexer.cpp, SV("(test)"));
    if(err){ result = 1; goto finally; }
    *count = 0;
    for(;;){
        if(*count >= MAX_TEST_TOKENS){
            result = 1;
            goto finally;
        }
        CcToken tok;
        err = cc_lex_next_token(&lexer, &tok);
        if(err){
            result = 1;
            goto finally;
        }
        if(tok.type == CC_EOF) break;
        (*out)[(*count)++] = tok;
    }
    finally:
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        TestPrintf("%s%s:%d:%s%s\n    %.*s", _test_color_gray, file, line, func, _test_color_reset, sv_p(sv));
    }
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&lexer.cpp.synth_arena);
    return result;
}
#define CC_LEX_STRING(txt, out, count) cc_lex_string(txt, &out, count, __FILE__, __func__, __LINE__)

// Like cc_lex_string, but expects an error and captures the error message.
// Returns 0 if an error occurred (success), 1 if no error (failure).
static
int
cc_lex_string_expect_error(StringView txt, StringView* err_out){
    ArenaAllocator aa = {0};
    Allocator a = allocator_from_arena(&aa);
    FileCache *fc = fc_create(a);
    MStringBuilder log_sb = {.allocator=a};
    MsbLogger logger_ = {0};
    Logger* logger = msb_logger(&logger_, &log_sb);
    AtomTable at = {.allocator = a};
    Environment env = {.allocator = a, .at=&at};
    int err;
    CcLexer lexer = {
        .cpp = {
            .allocator = a,
            .fc = fc,
            .at = &at,
            .logger = logger,
            .env = &env,
            .target = cc_target_test(),
        },
    };
    fc_write_path(fc, "(test)", 6);
    err = fc_cache_file(fc, txt);
    if(err) goto finally;
    err = cpp_define_builtin_macros(&lexer.cpp);
    if(err) goto finally;
    err = cpp_include_file_via_file_cache(&lexer.cpp, SV("(test)"));
    if(err) goto finally;
    {
        CcToken tok;
        for(;;){
            err = cc_lex_next_token(&lexer, &tok);
            if(err) break;
            if(tok.type == CC_EOF) break;
        }
    }
    finally:;
    int result = err ? 0 : 1;
    if(log_sb.cursor){
        StringView sv = msb_borrow_sv(&log_sb);
        *err_out = (StringView){.length = sv.length, .text = (const char*)Allocator_dupe(MALLOCATOR, sv.text, sv.length)};
    }
    else
        *err_out = (StringView){0};
    ArenaAllocator_free_all(&aa);
    ArenaAllocator_free_all(&lexer.cpp.synth_arena);
    return result;
}

// Helpers for building expected tokens
static CcToken cc_int_tok(uint64_t v, CcConstantType ctype){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=ctype, .integer_value=v}}; }
static CcToken cc_float_tok(float v){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=CC_FLOAT, .float_value=v}}; }
static CcToken cc_double_tok(double v){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=CC_DOUBLE, .double_value=v}}; }
static CcToken cc_long_double_tok(double v){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=CC_LONG_DOUBLE, .double_value=v}}; }
static CcToken cc_kw_tok(CcKeyword kw){ return (CcToken){.kw={.type=CC_KEYWORD, .kw=kw}}; }
static CcToken cc_punct_tok(CcPunct p){ return (CcToken){.punct={.type=CC_PUNCTUATOR, .punct=p}}; }
// Abuse: stash a const char* in the Atom field. cc_tok_matches knows to
// compare the real atom's data/length against this C string.
static CcToken cc_ident_tok(const char* name){ return (CcToken){.ident={.type=CC_IDENTIFIER, .ident=(Atom)name}}; }
static CcToken cc_str_tok(CcStringType stype, StringView sv){ return (CcToken){.str={.type=CC_STRING_LITERAL, .stype=stype, .text=sv.text, .length=(uint32_t)sv.length}}; }

static
const char*
cc_type_name(CcTokenType t){
    switch(t){
        case CC_EOF: return "EOF";
        case CC_KEYWORD: return "KEYWORD";
        case CC_IDENTIFIER: return "IDENTIFIER";
        case CC_CONSTANT: return "CONSTANT";
        case CC_STRING_LITERAL: return "STRING_LITERAL";
        case CC_PUNCTUATOR: return "PUNCTUATOR";
    }
    return "?";
}

// Compare two tokens for the fields we care about.
// Returns 1 if they match, 0 otherwise.
static
_Bool
cc_tok_matches(CcToken got, CcToken exp){
    if(got.type != exp.type) return 0;
    switch(got.type){
        case CC_EOF: return 1;
        case CC_KEYWORD: return got.kw.kw == exp.kw.kw;
        case CC_IDENTIFIER: {
            // got.ident is a real Atom, exp.ident is a fake (const char* cast)
            const char* exp_name = (const char*)exp.ident.ident;
            size_t exp_len = strlen(exp_name);
            return got.ident.ident->length == exp_len
                && memcmp(got.ident.ident->data, exp_name, exp_len) == 0;
        }
        case CC_CONSTANT:
            if(got.constant.ctype != exp.constant.ctype) return 0;
            switch(got.constant.ctype){
                case CC_FLOAT:
                    return got.constant.float_value == exp.constant.float_value;
                case CC_DOUBLE:
                case CC_LONG_DOUBLE:
                    return got.constant.double_value == exp.constant.double_value;
                default:
                    return got.constant.integer_value == exp.constant.integer_value;
            }
        case CC_STRING_LITERAL:
            if(got.str.stype != exp.str.stype) return 0;
            if(got.str.length != exp.str.length) return 0;
            return memcmp(got.str.text, exp.str.text, got.str.length) == 0;
        case CC_PUNCTUATOR:
            return got.punct.punct == exp.punct.punct;
    }
    return 0;
}

TestFunction(test_cc_lex_integers){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; CcToken exp; int line;
    } test_cases[] = {
        {"zero",         SV("0"),          cc_int_tok(0, CC_INT), __LINE__},
        {"decimal",      SV("42"),         cc_int_tok(42, CC_INT), __LINE__},
        {"hex",          SV("0xFF"),       cc_int_tok(0xFF, CC_INT), __LINE__},
        {"hex_upper",    SV("0XAB"),       cc_int_tok(0xAB, CC_INT), __LINE__},
        {"binary",       SV("0b1010"),     cc_int_tok(10, CC_INT), __LINE__},
        {"octal",        SV("077"),        cc_int_tok(077, CC_INT), __LINE__},
        {"unsigned",     SV("42u"),        cc_int_tok(42, CC_UNSIGNED), __LINE__},
        {"unsigned_U",   SV("42U"),        cc_int_tok(42, CC_UNSIGNED), __LINE__},
        {"long",         SV("42l"),        cc_int_tok(42, CC_LONG), __LINE__},
        {"long_L",       SV("42L"),        cc_int_tok(42, CC_LONG), __LINE__},
        {"ulong",        SV("42ul"),       cc_int_tok(42, CC_UNSIGNED_LONG), __LINE__},
        {"ulong_LU",     SV("42LU"),       cc_int_tok(42, CC_UNSIGNED_LONG), __LINE__},
        {"llong",        SV("42ll"),       cc_int_tok(42, CC_LONG_LONG), __LINE__},
        {"ullong",       SV("42ull"),      cc_int_tok(42, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"ullong_ULL",   SV("42ULL"),      cc_int_tok(42, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"hex_ul",       SV("0xDEADul"),   cc_int_tok(0xDEAD, CC_UNSIGNED_LONG), __LINE__},
        {"digit_sep",    SV("1'000'000"),  cc_int_tok(1000000, CC_INT), __LINE__},
        {"hex_sep",      SV("0xFF'FF"),    cc_int_tok(0xFFFF, CC_INT), __LINE__},
        {"large",        SV("18446744073709551615ULL"), cc_int_tok(UINT64_MAX, CC_UNSIGNED_LONG_LONG), __LINE__},
        // Suffix order variations
        {"LLU",          SV("0LLU"),       cc_int_tok(0, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"Ull",          SV("0Ull"),       cc_int_tok(0, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"llu",          SV("0llu"),       cc_int_tok(0, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"Lu",           SV("42Lu"),       cc_int_tok(42, CC_UNSIGNED_LONG), __LINE__},
        {"uL",           SV("42uL"),       cc_int_tok(42, CC_UNSIGNED_LONG), __LINE__},
        // Digit separators in binary/octal
        {"bin_sep",      SV("0b1'0'1'0"), cc_int_tok(10, CC_INT), __LINE__},
        {"oct_sep",      SV("0'77"),       cc_int_tok(077, CC_INT), __LINE__},
        // 0 through decimal path (not octal)
        {"zero_u",       SV("0u"),         cc_int_tok(0, CC_UNSIGNED), __LINE__},
        {"zero_ll",      SV("0LL"),        cc_int_tok(0, CC_LONG_LONG), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): token mismatch", test_cases[i].name, test_cases[i].line);
            TestReport("  got type=%s ctype=%d value=%llu", cc_type_name(toks[0].type), toks[0].constant.ctype, (unsigned long long)toks[0].constant.integer_value);
            TestReport("  exp type=%s ctype=%d value=%llu", cc_type_name(test_cases[i].exp.type), test_cases[i].exp.constant.ctype, (unsigned long long)test_cases[i].exp.constant.integer_value);
        }
    }
    // Invalid suffix errors
    {
        struct {
            const char* name; StringView inp; StringView exp_err; int line;
        } error_cases[] = {
            {"fu", SV("0fu"), SV("(test):1:1: error: Invalid suffix: 'f' and 'u' are mutually exclusive\n"), __LINE__},
            {"fll", SV("0fll"), SV("(test):1:1: error: Invalid suffix: 'f' and 'll' are mutually exclusive\n"), __LINE__},
        };
        for(size_t i = 0; i < arrlen(error_cases); i++){
            StringView err_msg;
            int err = cc_lex_string_expect_error(error_cases[i].inp, &err_msg);
            TEST_stats.executed++;
            if(err){
                TEST_stats.failures++;
                TestReport("test '%s' (line %d): expected error for invalid suffix", error_cases[i].name, error_cases[i].line);
            }
            else if(!test_expect_equals_sv(error_cases[i].exp_err, err_msg, "exp_err", "err_msg", &TEST_stats, __FILE__, __func__, error_cases[i].line)){
                TestPrintf("%s:%d: %s failed\n", __FILE__, error_cases[i].line, error_cases[i].name);
            }
            if(err_msg.length)
                Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_floats){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; CcToken exp; int line;
    } test_cases[] = {
        {"float_f",      SV("3.14f"),      cc_float_tok(3.14f), __LINE__},
        {"float_F",      SV("3.14F"),      cc_float_tok(3.14f), __LINE__},
        {"double",       SV("3.14"),       cc_double_tok(3.14), __LINE__},
        {"double_exp",   SV("1e10"),       cc_double_tok(1e10), __LINE__},
        {"double_Exp",   SV("1E10"),       cc_double_tok(1E10), __LINE__},
        {"double_frac",  SV(".5"),         cc_double_tok(.5), __LINE__},
        {"double_trail", SV("1."),         cc_double_tok(1.), __LINE__},
        {"float_exp",    SV("1.5e2f"),     cc_float_tok(1.5e2f), __LINE__},
        {"zero_f",       SV("0.0f"),       cc_float_tok(0.0f), __LINE__},
        {"long_double",  SV("3.14L"),      cc_long_double_tok(3.14), __LINE__},
        {"digit_sep_f",  SV("1'000.5f"),   cc_float_tok(1000.5f), __LINE__},
        // Negative exponent
        {"neg_exp",      SV("1e-10"),      cc_double_tok(1e-10), __LINE__},
        {"neg_exp_f",    SV("1.5e-3f"),    cc_float_tok(1.5e-3f), __LINE__},
        // Positive exponent with +
        {"pos_exp",      SV("1e+10"),      cc_double_tok(1e+10), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): token mismatch", test_cases[i].name, test_cases[i].line);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_chars){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; uint64_t exp_value; CcConstantType exp_ctype; int line;
    } test_cases[] = {
        {"simple_a",    SV("'a'"),     'a',              CC_INT,    __LINE__},
        {"simple_z",    SV("'z'"),     'z',              CC_INT,    __LINE__},
        {"escape_n",    SV("'\\n'"),   '\n',             CC_INT,    __LINE__},
        {"escape_t",    SV("'\\t'"),   '\t',             CC_INT,    __LINE__},
        {"escape_0",    SV("'\\0'"),   '\0',             CC_INT,    __LINE__},
        {"escape_sq",   SV("'\\''"),   '\'',             CC_INT,    __LINE__},
        {"escape_bs",   SV("'\\\\'"), '\\',              CC_INT,    __LINE__},
        {"hex_escape",  SV("'\\x41'"), 0x41,             CC_INT,    __LINE__},
        {"octal_esc",   SV("'\\101'"), 0101,             CC_INT,    __LINE__},
        {"multichar",   SV("'ab'"),    (('a'<<8)|'b'),   CC_INT,    __LINE__},
        {"L_prefix",    SV("L'a'"),    'a',              CC_WCHAR,  __LINE__},
        {"u_prefix",    SV("u'a'"),    'a',              CC_CHAR16, __LINE__},
        {"U_prefix",    SV("U'a'"),    'a',              CC_CHAR32, __LINE__},
        {"u8_prefix",   SV("u8'a'"),   'a',              CC_UCHAR,  __LINE__},
        // Universal character names
        {"ucn_u",       SV("'\\u0041'"),  0x0041,          CC_INT,    __LINE__},
        {"ucn_U",       SV("'\\U00000041'"), 0x0041,       CC_INT,    __LINE__},
        {"ucn_u_hi",    SV("'\\u00E9'"),  0x00E9,          CC_INT,    __LINE__},
        // Prefixed with escape
        {"u8_escape_n", SV("u8'\\n'"),    '\n',             CC_UCHAR,  __LINE__},
        {"L_escape_0",  SV("L'\\0'"),     '\0',             CC_WCHAR,  __LINE__},
        {"U_hex_esc",   SV("U'\\x41'"),   0x41,             CC_CHAR32, __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(toks[0].type != CC_CONSTANT || toks[0].constant.ctype != test_cases[i].exp_ctype || toks[0].constant.integer_value != test_cases[i].exp_value){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): expected ctype=%d value=%llu, got type=%s ctype=%d value=%llu",
                test_cases[i].name, test_cases[i].line,
                test_cases[i].exp_ctype, (unsigned long long)test_cases[i].exp_value,
                cc_type_name(toks[0].type), toks[0].constant.ctype,
                (unsigned long long)toks[0].constant.integer_value);
        }
    }
    // Prefixed multi-character constants must be errors
    {
        struct {
            const char* name; StringView inp; StringView exp_err; int line;
        } error_cases[] = {
            {"u8_multi",  SV("u8'ab'"), SV("(test):1:1: error: Multi-character character constant with prefix is not allowed\n"), __LINE__},
            {"L_multi",   SV("L'ab'"),  SV("(test):1:1: error: Multi-character character constant with prefix is not allowed\n"), __LINE__},
            {"u_multi",   SV("u'ab'"),  SV("(test):1:1: error: Multi-character character constant with prefix is not allowed\n"), __LINE__},
            {"U_multi",   SV("U'ab'"),  SV("(test):1:1: error: Multi-character character constant with prefix is not allowed\n"), __LINE__},
            {"empty",     SV("''"),     SV("(test):1:1: error: Invalid character constant\n"), __LINE__},
        };
        for(size_t i = 0; i < arrlen(error_cases); i++){
            StringView err_msg;
            int err = cc_lex_string_expect_error(error_cases[i].inp, &err_msg);
            TEST_stats.executed++;
            if(err){
                TEST_stats.failures++;
                TestReport("test '%s' (line %d): expected error for prefixed multichar literal", error_cases[i].name, error_cases[i].line);
            }
            else if(!test_expect_equals_sv(error_cases[i].exp_err, err_msg, "exp_err", "err_msg", &TEST_stats, __FILE__, __func__, error_cases[i].line)){
                TestPrintf("%s:%d: %s failed\n", __FILE__, error_cases[i].line, error_cases[i].name);
            }
            if(err_msg.length)
                Allocator_free(MALLOCATOR, err_msg.text, err_msg.length);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_strings){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; CcToken exp; int line;
    } test_cases[] = {
        {"basic",    SV("\"hello\""),     cc_str_tok(CC_STRING,   SV("hello")), __LINE__},
        {"empty",    SV("\"\""),          cc_str_tok(CC_STRING,   SV("")), __LINE__},
        {"L_str",    SV("L\"wide\""),     cc_str_tok(CC_LSTRING,  SV("wide")), __LINE__},
        {"u_str",    SV("u\"utf16\""),    cc_str_tok(CC_uSTRING,  SV("utf16")), __LINE__},
        {"U_str",    SV("U\"utf32\""),    cc_str_tok(CC_USTRING,  SV("utf32")), __LINE__},
        {"u8_str",   SV("u8\"utf8\""),    cc_str_tok(CC_U8STRING, SV("utf8")), __LINE__},
        {"escapes",  SV("\"a\\nb\""),     cc_str_tok(CC_STRING,   SV("a\\nb")), __LINE__},
        // Empty prefixed strings
        {"L_empty",  SV("L\"\""),        cc_str_tok(CC_LSTRING,  SV("")), __LINE__},
        {"u_empty",  SV("u\"\""),        cc_str_tok(CC_uSTRING,  SV("")), __LINE__},
        {"U_empty",  SV("U\"\""),        cc_str_tok(CC_USTRING,  SV("")), __LINE__},
        {"u8_empty", SV("u8\"\""),       cc_str_tok(CC_U8STRING, SV("")), __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): string mismatch", test_cases[i].name, test_cases[i].line);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_punctuators){
    TESTBEGIN();
    #ifdef __GNUC__
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmultichar"
    #endif
    struct {
        const char* name; StringView inp; CcPunct exp; int line;
    } test_cases[] = {
        {"plus",     SV("+"),    CC_plus, __LINE__},
        {"minus",    SV("-"),    CC_minus, __LINE__},
        {"star",     SV("*"),    CC_star, __LINE__},
        {"slash",    SV("/"),    CC_slash, __LINE__},
        {"assign",   SV("="),    CC_assign, __LINE__},
        {"eq",       SV("=="),   CC_eq, __LINE__},
        {"ne",       SV("!="),   CC_ne, __LINE__},
        {"lt",       SV("<"),    CC_lt, __LINE__},
        {"gt",       SV(">"),    CC_gt, __LINE__},
        {"le",       SV("<="),   CC_le, __LINE__},
        {"ge",       SV(">="),   CC_ge, __LINE__},
        {"and",      SV("&&"),   CC_and, __LINE__},
        {"or",       SV("||"),   CC_or, __LINE__},
        {"amp",      SV("&"),    CC_amp, __LINE__},
        {"pipe",     SV("|"),    CC_pipe, __LINE__},
        {"xor",      SV("^"),    CC_xor, __LINE__},
        {"tilde",    SV("~"),    CC_tilde, __LINE__},
        {"bang",     SV("!"),    CC_bang, __LINE__},
        {"lparen",   SV("("),    CC_lparen, __LINE__},
        {"rparen",   SV(")"),    CC_rparen, __LINE__},
        {"lbracket", SV("["),    CC_lbracket, __LINE__},
        {"rbracket", SV("]"),    CC_rbracket, __LINE__},
        {"lbrace",   SV("{"),    CC_lbrace, __LINE__},
        {"rbrace",   SV("}"),    CC_rbrace, __LINE__},
        {"semi",     SV(";"),    CC_semi, __LINE__},
        {"comma",    SV(","),    CC_comma, __LINE__},
        {"dot",      SV("."),    CC_dot, __LINE__},
        {"arrow",    SV("->"),   CC_arrow, __LINE__},
        {"plusplus", SV("++"),   CC_plusplus, __LINE__},
        {"minmin",   SV("--"),   CC_minusminus, __LINE__},
        {"lshift",   SV("<<"),   CC_lshift, __LINE__},
        {"rshift",   SV(">>"),   CC_rshift, __LINE__},
        {"question", SV("?"),    CC_question, __LINE__},
        {"colon",    SV(":"),    CC_colon, __LINE__},
        {"star_eq",  SV("*="),   CC_star_assign, __LINE__},
        {"plus_eq",  SV("+="),   CC_plus_assign, __LINE__},
        {"minus_eq", SV("-="),   CC_minus_assign, __LINE__},
        {"slash_eq", SV("/="),   CC_slash_assign, __LINE__},
        {"pct_eq",   SV("%="),   CC_percent_assign, __LINE__},
        {"amp_eq",   SV("&="),   CC_amp_assign, __LINE__},
        {"pipe_eq",  SV("|="),   CC_pipe_assign, __LINE__},
        {"xor_eq",   SV("^="),   CC_xor_assign, __LINE__},
        {"lsh_eq",   SV("<<="),  CC_lshift_assign, __LINE__},
        {"rsh_eq",   SV(">>="),  CC_rshift_assign, __LINE__},
        {"ellipsis", SV("..."),  CC_ellipsis, __LINE__},
    };
    #ifdef __GNUC__
    #pragma GCC diagnostic pop
    #endif
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(toks[0].type != CC_PUNCTUATOR || toks[0].punct.punct != test_cases[i].exp){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): punct mismatch: got %u, expected %u", test_cases[i].name, test_cases[i].line, (unsigned)toks[0].punct.punct, (unsigned)test_cases[i].exp);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_keywords){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; CcKeyword exp; int line;
    } test_cases[] = {
        {"do", SV("do"), CC_do, __LINE__},
        {"if", SV("if"), CC_if, __LINE__},
        {"for", SV("for"), CC_for, __LINE__},
        {"int", SV("int"), CC_int, __LINE__},
        {"true", SV("true"), CC_true, __LINE__},
        {"long", SV("long"), CC_long, __LINE__},
        {"char", SV("char"), CC_char, __LINE__},
        {"auto", SV("auto"), CC_auto, __LINE__},
        {"bool", SV("bool"), CC_bool, __LINE__},
        {"_Bool", SV("_Bool"), CC_bool, __LINE__},
        {"else", SV("else"), CC_else, __LINE__},
        {"enum", SV("enum"), CC_enum, __LINE__},
        {"case", SV("case"), CC_case, __LINE__},
        {"goto", SV("goto"), CC_goto, __LINE__},
        {"void", SV("void"), CC_void, __LINE__},
        {"false", SV("false"), CC_false, __LINE__},
        {"break", SV("break"), CC_break, __LINE__},
        {"float", SV("float"), CC_float, __LINE__},
        {"const", SV("const"), CC_const, __LINE__},
        {"short", SV("short"), CC_short, __LINE__},
        {"union", SV("union"), CC_union, __LINE__},
        {"while", SV("while"), CC_while, __LINE__},
        {"double", SV("double"), CC_double, __LINE__},
        {"extern", SV("extern"), CC_extern, __LINE__},
        {"inline", SV("inline"), CC_inline, __LINE__},
        {"return", SV("return"), CC_return, __LINE__},
        {"signed", SV("signed"), CC_signed, __LINE__},
        {"sizeof", SV("sizeof"), CC_sizeof, __LINE__},
        {"static", SV("static"), CC_static, __LINE__},
        {"struct", SV("struct"), CC_struct, __LINE__},
        {"switch", SV("switch"), CC_switch, __LINE__},
        {"typeof", SV("typeof"), CC_typeof, __LINE__},
        {"alignas", SV("alignas"), CC_alignas, __LINE__},
        {"_Alignas", SV("_Alignas"), CC_alignas, __LINE__},
        {"alignof", SV("alignof"), CC_alignof, __LINE__},
        {"_Alignof", SV("_Alignof"), CC_alignof, __LINE__},
        {"default", SV("default"), CC_default, __LINE__},
        {"typedef", SV("typedef"), CC_typedef, __LINE__},
        {"nullptr", SV("nullptr"), CC_nullptr, __LINE__},
        {"_Atomic", SV("_Atomic"), CC__Atomic, __LINE__},
        {"_BitInt", SV("_BitInt"), CC__BitInt, __LINE__},
        {"_Complex", SV("_Complex"), CC__Complex, __LINE__},
        {"continue", SV("continue"), CC_continue, __LINE__},
        {"register", SV("register"), CC_register, __LINE__},
        {"restrict", SV("restrict"), CC_restrict, __LINE__},
        {"unsigned", SV("unsigned"), CC_unsigned, __LINE__},
        {"volatile", SV("volatile"), CC_volatile, __LINE__},
        {"_Generic", SV("_Generic"), CC__Generic, __LINE__},
        {"_Float16", SV("_Float16"), CC__Float16, __LINE__},
        {"_Float32", SV("_Float32"), CC__Float32, __LINE__},
        {"_Float64", SV("_Float64"), CC__Float64, __LINE__},
        {"constexpr", SV("constexpr"), CC_constexpr, __LINE__},
        {"_Float128", SV("_Float128"), CC__Float128, __LINE__},
        {"_Imaginary", SV("_Imaginary"), CC__Imaginary, __LINE__},
        {"_Noreturn", SV("_Noreturn"), CC__Noreturn, __LINE__},
        {"_Decimal32", SV("_Decimal32"), CC__Decimal32, __LINE__},
        {"_Decimal64", SV("_Decimal64"), CC__Decimal64, __LINE__},
        {"_Decimal128", SV("_Decimal128"), CC__Decimal128, __LINE__},
        {"thread_local", SV("thread_local"), CC_thread_local, __LINE__},
        {"_Thread_local", SV("_Thread_local"), CC_thread_local, __LINE__},
        {"static_assert", SV("static_assert"), CC_static_assert, __LINE__},
        {"_Static_assert", SV("_Static_assert"), CC_static_assert, __LINE__},
        {"typeof_unqual", SV("typeof_unqual"), CC_typeof_unqual, __LINE__},
        {"_Countof", SV("_Countof"), CC__Countof, __LINE__},
        {"countof", SV("countof"), CC__Countof, __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            continue;
        }
        TEST_stats.executed++;
        if(toks[0].type != CC_KEYWORD || toks[0].kw.kw != test_cases[i].exp){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): keyword mismatch", test_cases[i].name, test_cases[i].line);
        }
    }
    TESTEND();
}

TestFunction(test_cc_lex_multi_token){
    TESTBEGIN();
    struct {
        const char* name; StringView inp; int exp_count; CcToken exp[8]; int line;
    } test_cases[] = {
        {"simple_expr", SV("1 + 2"), 3, {
            cc_int_tok(1, CC_INT),
            cc_punct_tok(CC_plus),
            cc_int_tok(2, CC_INT),
        }, __LINE__},
        {"decl", SV("int x;"), 3, {
            cc_kw_tok(CC_int),
            cc_ident_tok("x"),
            cc_punct_tok(CC_semi),
        }, __LINE__},
        {"assign", SV("x = 42;"), 4, {
            cc_ident_tok("x"),
            cc_punct_tok(CC_assign),
            cc_int_tok(42, CC_INT),
            cc_punct_tok(CC_semi),
        }, __LINE__},
        {"func_call", SV("foo(1, 2)"), 6, {
            cc_ident_tok("foo"),
            cc_punct_tok(CC_lparen),
            cc_int_tok(1, CC_INT),
            cc_punct_tok(CC_comma),
            cc_int_tok(2, CC_INT),
            cc_punct_tok(CC_rparen),
        }, __LINE__},
        {"string_and_int", SV("\"hello\" 42"), 2, {
            cc_str_tok(CC_STRING, SV("hello")),
            cc_int_tok(42, CC_INT),
        }, __LINE__},
        // No-whitespace adjacency
        {"no_ws_add", SV("1+2"), 3, {
            cc_int_tok(1, CC_INT),
            cc_punct_tok(CC_plus),
            cc_int_tok(2, CC_INT),
        }, __LINE__},
        {"no_ws_assign", SV("a=b"), 3, {
            cc_ident_tok("a"),
            cc_punct_tok(CC_assign),
            cc_ident_tok("b"),
        }, __LINE__},
        // Near-miss identifiers (should NOT be keywords)
        {"near_miss_iff", SV("iff"), 1, {
            cc_ident_tok("iff"),
        }, __LINE__},
        {"near_miss_integer", SV("integer"), 1, {
            cc_ident_tok("integer"),
        }, __LINE__},
        {"near_miss__Boo", SV("_Boo"), 1, {
            cc_ident_tok("_Boo"),
        }, __LINE__},
    };
    for(size_t i = 0; i < arrlen(test_cases); i++){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count);
        TestAssertFalse(err);
        TEST_stats.executed++;
        if(count != test_cases[i].exp_count){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): expected %d tokens, got %d", test_cases[i].name, test_cases[i].line, test_cases[i].exp_count, count);
            continue;
        }
        for(int j = 0; j < count; j++){
            TEST_stats.executed++;
            if(!cc_tok_matches(toks[j], test_cases[i].exp[j])){
                TEST_stats.failures++;
                TestReport("test '%s' (line %d): token %d mismatch: got type=%s, expected type=%s", test_cases[i].name, test_cases[i].line, j, cc_type_name(toks[j].type), cc_type_name(test_cases[i].exp[j].type));
            }
        }
    }
    TESTEND();
}

int main(int argc, char** argv){
    testing_allocator_init();
    RegisterTest(test_cc_lex_integers);
    RegisterTest(test_cc_lex_floats);
    RegisterTest(test_cc_lex_chars);
    RegisterTest(test_cc_lex_strings);
    RegisterTest(test_cc_lex_punctuators);
    RegisterTest(test_cc_lex_keywords);
    RegisterTest(test_cc_lex_multi_token);
    int err = test_main(argc, argv, NULL);
    testing_assert_all_freed();
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
#include "cc_lexer.c"
