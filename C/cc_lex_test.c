//
// Copyright © 2026-2026, David Priver <david@davidpriver.com>
//
#define STB_SPRINTF_STATIC
#define STB_SPRINTF_IMPLEMENTATION
#if 1
#define USE_TESTING_ALLOCATOR
#define REPLACE_MALLOCATOR
#define HEAVY_RECORDING
#endif
#include "../Drp/compiler_warnings.h"
#include "../Drp/switch_macros.h"
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
#include "cpp_preprocessor.h"
#include "cc_tok.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

enum { MAX_TEST_TOKENS = 64 };

// Helper: lex a string into an array of CcTokens, return count or -1 on error.
static
int
cc_lex_string_mode(StringView txt, CcToken (*out)[MAX_TEST_TOKENS], int* count, ArenaAllocator* out_aa, ArenaAllocator* out_synth, const char* file, const char* func, int line, _Bool array, _Bool short_wchar, _Bool quiet){
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
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = cc_target_test(),
    };
    if(short_wchar) cpp.target.wchar_type = CCBT_unsigned_short;
    err = env_setenv4(&env, "literal_test", 12, "C:\\new\\test\"\n", 13);
    if(err){ result = 1; goto finally; }
    fc_write_path(fc, "(test)", 6);
    err = fc_cache_file(fc, txt);
    if(err){ result = 1; goto finally; }
    err = cpp_define_builtin_macros(&cpp);
    if(err){ result = 1; goto finally; }
    err = cpp_include_file_via_file_cache(&cpp, SV("(test)"));
    if(err){ result = 1; goto finally; }
    *count = 0;
    CppTokens pp = {0};
    const CppToken* cursor = NULL;
    const CppToken* end = NULL;
    if(array){
        for(;;){
            CppToken tok;
            err = cpp_next_pp_token(&cpp, &tok);
            if(err){ result = 1; goto finally; }
            err = ma_push(CppToken)(&pp, a, tok);
            if(err){ result = 1; goto finally; }
            if(tok.type == CPP_EOF) break;
        }
        cursor = pp.data;
        end = pp.data + pp.count;
    }
    for(;;){
        if(*count >= MAX_TEST_TOKENS){
            result = 1;
            goto finally;
        }
        CcToken tok;
        err = array ? cpp_next_c_token_array(&cpp, &cursor, end, &tok) : cpp_next_c_token(&cpp, &tok);
        if(err){
            result = 1;
            goto finally;
        }
        if(tok.type == CC_EOF) break;
        (*out)[(*count)++] = tok;
    }
    finally:
    if(log_sb.cursor && !quiet){
        StringView sv = msb_borrow_sv(&log_sb);
        TestPrintf("%s%s:%d:%s%s\n    %.*s", _test_color_gray, file, line, func, _test_color_reset, sv_p(sv));
    }
    *out_aa = aa;
    *out_synth = cpp.synth_arena;
    return result;
}
#define CC_LEX_STRING(txt, out, count, aa, synth) cc_lex_string_mode(txt, &out, count, aa, synth, __FILE__, __func__, __LINE__, 0, 0, 0)

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
    CppPreprocessor cpp = {
        .allocator = a,
        .fc = fc,
        .at = &at,
        .logger = logger,
        .env = &env,
        .target = cc_target_test(),
    };
    fc_write_path(fc, "(test)", 6);
    err = fc_cache_file(fc, txt);
    if(err) goto finally;
    err = cpp_define_builtin_macros(&cpp);
    if(err) goto finally;
    err = cpp_include_file_via_file_cache(&cpp, SV("(test)"));
    if(err) goto finally;
    {
        CcToken tok;
        for(;;){
            err = cpp_next_c_token(&cpp, &tok);
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
    ArenaAllocator_free_all(&cpp.synth_arena);
    return result;
}

// Helpers for building expected tokens
static CcToken cc_int_tok(uint64_t v, CcConstantType ctype){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=ctype, .integer_value=v}}; }
static CcToken cc_float_tok(float v){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=CC_FLOAT, .float_value=v}}; }
static CcToken cc_double_tok(double v){ return (CcToken){.constant={.type=CC_CONSTANT, .ctype=CC_DOUBLE, .double_value=v}}; }
// IEEE 754 binary128 words, least significant word first.
static CcToken cc_long_double_tok(uint64_t val[2]){ CcToken tok = {.constant={.type=CC_CONSTANT, .ctype=CC_LONG_DOUBLE}}; memcpy(&tok.constant.quad_value, val, 16); return tok;}
static CcToken cc_kw_tok(CcKeyword kw){ return (CcToken){.kw={.type=CC_KEYWORD, .kw=kw}}; }
static CcToken cc_punct_tok(CcPunct p){ return (CcToken){.punct={.type=CC_PUNCTUATOR, .punct=p}}; }
// Abuse: stash a const char* in the Atom field. cc_tok_matches knows to
// compare the real atom's data/length against this C string.
static CcToken cc_ident_tok(const char* name){ return (CcToken){.ident={.type=CC_IDENTIFIER, .ident=(Atom)name}}; }
static CcToken cc_str_tok(CcStringType stype, StringView sv){ return (CcToken){.str={.type=CC_STRING_LITERAL, .stype=stype, .utf8=sv.text, .length=(uint32_t)sv.length}}; }
static CcToken cc_str16_tok(CcStringType stype, const unsigned short* data, uint32_t len){ return (CcToken){.str={.type=CC_STRING_LITERAL, .stype=stype, .utf16=data, .length=len}}; }
static CcToken cc_str32_tok(CcStringType stype, const unsigned int* data, uint32_t len){ return (CcToken){.str={.type=CC_STRING_LITERAL, .stype=stype, .utf32=data, .length=len}}; }

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
                    return got.constant.double_value == exp.constant.double_value;
                case CC_LONG_DOUBLE:
                    return memcmp(&got.constant.quad_value, &exp.constant.quad_value, sizeof got.constant.quad_value) == 0;
                case CC_INT128:
                case CC_UNSIGNED_INT128:
                    return ci_uint128_eq(got.constant.integer128_value, exp.constant.integer128_value);
                case CC_INT:
                case CC_UNSIGNED:
                case CC_LONG:
                case CC_UNSIGNED_LONG:
                case CC_LONG_LONG:
                case CC_UNSIGNED_LONG_LONG:
                case CC_WCHAR:
                case CC_CHAR16:
                case CC_CHAR32:
                case CC_UCHAR:
                    return got.constant.integer_value == exp.constant.integer_value;
                DRP_CASES_EXHAUSTED;
            }
        case CC_STRING_LITERAL:
            if(got.str.stype != exp.str.stype) return 0;
            if(got.str.length != exp.str.length) return 0;
            switch(got.str.stype){
                case CC_uSTRING:
                    return memcmp(got.str.utf16, exp.str.utf16, got.str.length * 2) == 0;
                case CC_USTRING:
                    return memcmp(got.str.utf32, exp.str.utf32, got.str.length * 4) == 0;
                case CC_LSTRING:
                    return memcmp(got.str.utf32, exp.str.utf32, got.str.length * 4) == 0;
                case CC_STRING:
                case CC_U8STRING:
                    return memcmp(got.str.utf8, exp.str.utf8, got.str.length) == 0;
                DRP_CASES_EXHAUSTED;
            }
        case CC_PUNCTUATOR:
            return got.punct.punct == exp.punct.punct;
    }
    return 0;
}

static CcToken cc_int128_tok(uint64_t hi, uint64_t lo, CcConstantType ctype){
    return (CcToken){.constant={.type=CC_CONSTANT, .ctype=ctype,
        .integer128_value=ci_uint128_or(ci_uint128_shl(ci_uint128_from_uint64(hi), 64), ci_uint128_from_uint64(lo))}};
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
        {"binary_i128", SV("0b10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000001i128"), cc_int128_tok(UINT64_C(1) << 36, 1, CC_INT128), __LINE__},
        {"decimal_i128", SV("18446744073709551617i128"), cc_int128_tok(1, 1, CC_INT128), __LINE__},
        {"hex_ulll", SV("0xffffffffffffffffffffffffffffffffulll"), cc_int128_tok(UINT64_MAX, UINT64_MAX, CC_UNSIGNED_INT128), __LINE__},
        // MSVC integer suffixes
        {"msvc_i8",      SV("42i8"),       cc_int_tok(42, CC_INT), __LINE__},
        {"msvc_i16",     SV("42i16"),      cc_int_tok(42, CC_INT), __LINE__},
        {"msvc_i32",     SV("42i32"),      cc_int_tok(42, CC_INT), __LINE__},
        {"msvc_i64",     SV("42i64"),      cc_int_tok(42, CC_LONG_LONG), __LINE__},
        {"msvc_ui8",     SV("42ui8"),      cc_int_tok(42, CC_UNSIGNED), __LINE__},
        {"msvc_ui16",    SV("42ui16"),     cc_int_tok(42, CC_UNSIGNED), __LINE__},
        {"msvc_ui32",    SV("42ui32"),     cc_int_tok(42, CC_UNSIGNED), __LINE__},
        {"msvc_ui64",    SV("42ui64"),     cc_int_tok(42, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"msvc_hex_i32", SV("0xFFFFFFFFi32"), cc_int_tok(0xFFFFFFFF, CC_INT), __LINE__},
        {"msvc_hex_ui64",SV("0xFFFFFFFFFFFFFFFFui64"), cc_int_tok(UINT64_MAX, CC_UNSIGNED_LONG_LONG), __LINE__},
        {"msvc_I64_upper", SV("42I64"),    cc_int_tok(42, CC_LONG_LONG), __LINE__},
    };
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): token mismatch", test_cases[i].name, test_cases[i].line);
            TestReport("  got type=%s ctype=%d value=%llu", cc_type_name(toks[0].type), toks[0].constant.ctype, (unsigned long long)toks[0].constant.integer_value);
            TestReport("  exp type=%s ctype=%d value=%llu", cc_type_name(test_cases[i].exp.type), test_cases[i].exp.constant.ctype, (unsigned long long)test_cases[i].exp.constant.integer_value);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
    }
    // Invalid suffix errors
    {
        struct {
            const char* name; StringView inp; StringView exp_err; int line;
        } error_cases[] = {
            {"fu", SV("0fu"), SV("(test):1:1: error: Invalid suffix: 'f' and 'u' are mutually exclusive\n"), __LINE__},
            {"fll", SV("0fll"), SV("(test):1:1: error: Invalid suffix: 'f' and 'll' are mutually exclusive\n"), __LINE__},
        };
        static int error_idx = 0;
        for(size_t i = test_atomic_increment(&error_idx); i < arrlen(error_cases); i = test_atomic_increment(&error_idx)){
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
        {"long_double",  SV("3.14L"),      cc_long_double_tok((uint64_t[2]){0xeb851eb851eb851fULL, 0x400091eb851eb851ULL}), __LINE__},
        {"digit_sep_f",  SV("1'000.5f"),   cc_float_tok(1000.5f), __LINE__},
        // Negative exponent
        {"neg_exp",      SV("1e-10"),      cc_double_tok(1e-10), __LINE__},
        {"neg_exp_f",    SV("1.5e-3f"),    cc_float_tok(1.5e-3f), __LINE__},
        // Positive exponent with +
        {"pos_exp",      SV("1e+10"),      cc_double_tok(1e+10), __LINE__},
    };
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): token mismatch", test_cases[i].name, test_cases[i].line);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
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
        {"ucn_u_3byte", SV("'\\u250c'"),  0x250c,          CC_INT,    __LINE__},
        {"ucn_u_cjk",   SV("'\\u4e16'"),  0x4e16,          CC_INT,    __LINE__},
        {"ucn_U_4byte", SV("'\\U0001F600'"), 0x1F600,      CC_INT,    __LINE__},
        // Prefixed with escape
        {"u8_escape_n", SV("u8'\\n'"),    '\n',             CC_UCHAR,  __LINE__},
        {"L_escape_0",  SV("L'\\0'"),     '\0',             CC_WCHAR,  __LINE__},
        {"U_hex_esc",   SV("U'\\x41'"),   0x41,             CC_CHAR32, __LINE__},
    };
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
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
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
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
        static int error_idx = 0;
        for(size_t i = test_atomic_increment(&error_idx); i < arrlen(error_cases); i = test_atomic_increment(&error_idx)){
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
        {"basic",    SV("\"hello\""),     cc_str_tok(CC_STRING,   SV("hello\0")), __LINE__},
        {"empty",    SV("\"\""),          cc_str_tok(CC_STRING,   SV("\0")), __LINE__},
        {"L_str",    SV("L\"wide\""),     cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 'w','i','d','e', 0}, 5), __LINE__},
        {"u_str",    SV("u\"utf16\""),    cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 'u','t','f','1','6', 0}, 6), __LINE__},
        {"U_str",    SV("U\"utf32\""),    cc_str32_tok(CC_USTRING,  (const unsigned int[]){ 'u','t','f','3','2', 0}, 6), __LINE__},
        {"u8_str",   SV("u8\"utf8\""),    cc_str_tok(CC_U8STRING, SV("utf8\0")), __LINE__},
        {"escapes",  SV("\"a\\nb\""),     cc_str_tok(CC_STRING,   SV("a\nb\0")), __LINE__},
        // Universal character names
        {"ucn_u",    SV("\"\\u0041\""),   cc_str_tok(CC_STRING,   SV("A\0")), __LINE__},
        {"ucn_U",    SV("\"\\U00000041\""), cc_str_tok(CC_STRING, SV("A\0")), __LINE__},
        {"ucn_e_acute", SV("\"\\u00E9\""), cc_str_tok(CC_STRING,  SV("\xc3\xa9\0")), __LINE__},
        // 3-byte UTF-8 (U+250C = box drawing ┌, U+4E16 = CJK 世)
        {"ucn_u_3byte", SV("\"\\u250c\""), cc_str_tok(CC_STRING, SV("\xe2\x94\x8c\0")), __LINE__},
        {"ucn_u_cjk",   SV("\"\\u4e16\""), cc_str_tok(CC_STRING, SV("\xe4\xb8\x96\0")), __LINE__},
        // 4-byte UTF-8 (U+1F600 = 😀)
        {"ucn_U_4byte", SV("\"\\U0001F600\""), cc_str_tok(CC_STRING, SV("\xf0\x9f\x98\x80\0")), __LINE__},
        // Empty prefixed strings
        {"L_empty",  SV("L\"\""),        cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 0}, 1), __LINE__},
        {"u_empty",  SV("u\"\""),        cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 0}, 1), __LINE__},
        {"U_empty",  SV("U\"\""),        cc_str32_tok(CC_USTRING,  (const unsigned int[]){ 0}, 1), __LINE__},
        {"u8_empty", SV("u8\"\""),       cc_str_tok(CC_U8STRING, SV("\0")), __LINE__},
        // Escape sequences in prefixed strings
        {"L_escape_n",  SV("L\"a\\nb\""),   cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 'a','\n','b', 0}, 4), __LINE__},
        {"u_escape_t",  SV("u\"a\\tb\""),   cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 'a','\t','b', 0}, 4), __LINE__},
        {"U_escape_bs", SV("U\"a\\\\b\""),  cc_str32_tok(CC_USTRING,  (const unsigned int[]){ 'a','\\','b', 0}, 4), __LINE__},
        {"u8_escape_n", SV("u8\"a\\nb\""),  cc_str_tok(CC_U8STRING, SV("a\nb\0")), __LINE__},
        {"L_hex_esc",   SV("L\"\\x41\""),   cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 0x41, 0}, 2), __LINE__},
        {"u_hex_esc",   SV("u\"\\x41\""),   cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 0x41, 0}, 2), __LINE__},
        {"U_octal_esc", SV("U\"\\101\""),   cc_str32_tok(CC_USTRING,  (const unsigned int[]){ 0101, 0}, 2), __LINE__},
        {"u8_hex_esc",  SV("u8\"\\x41\""),  cc_str_tok(CC_U8STRING, SV("\x41\0")), __LINE__},
        // UCN in prefixed strings
        {"L_ucn_u",     SV("L\"\\u0041\""),      cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 0x0041, 0}, 2), __LINE__},
        {"u_ucn_u",     SV("u\"\\u00E9\""),      cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 0x00E9, 0}, 2), __LINE__},
        {"U_ucn_U",     SV("U\"\\U0001F600\""),  cc_str32_tok(CC_USTRING,  (const unsigned int[]){ 0x1F600, 0}, 2), __LINE__},
        {"u8_ucn_u",    SV("u8\"\\u00E9\""),     cc_str_tok(CC_U8STRING, SV("\xc3\xa9\0")), __LINE__},
        {"u8_ucn_cjk",  SV("u8\"\\u4e16\""),     cc_str_tok(CC_U8STRING, SV("\xe4\xb8\x96\0")), __LINE__},
        {"L_ucn_U_4b",  SV("L\"\\U0001F600\""),  cc_str32_tok(CC_LSTRING,  (const unsigned int[]){ 0x1F600, 0}, 2), __LINE__},
        // u string with UCN above BMP (surrogate pair)
        {"u_ucn_U_surr", SV("u\"\\U0001F600\""), cc_str16_tok(CC_uSTRING,  (const unsigned short[]){ 0xD83D, 0xDE00, 0}, 3), __LINE__},
    };
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestPrintf("%s:%d: test '%s': expected 1 token, got %d\n", __FILE__, test_cases[i].line, test_cases[i].name, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        TEST_stats.executed++;
        if(!cc_tok_matches(toks[0], test_cases[i].exp)){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): string mismatch", test_cases[i].name, test_cases[i].line);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
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
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        TEST_stats.executed++;
        if(toks[0].type != CC_PUNCTUATOR || toks[0].punct.punct != test_cases[i].exp){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): punct mismatch: got %u, expected %u", test_cases[i].name, test_cases[i].line, (unsigned)toks[0].punct.punct, (unsigned)test_cases[i].exp);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
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
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        if(count != 1){
            TestReport("test '%s' (line %d): expected 1 token, got %d", test_cases[i].name, test_cases[i].line, count);
            TEST_stats.executed++;
            TEST_stats.failures++;
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        TEST_stats.executed++;
        if(toks[0].type != CC_KEYWORD || toks[0].kw.kw != test_cases[i].exp){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): keyword mismatch", test_cases[i].name, test_cases[i].line);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
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
            cc_str_tok(CC_STRING, SV("hello\0")),
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
    static int case_idx = 0;
    for(size_t i = test_atomic_increment(&case_idx); i < arrlen(test_cases); i = test_atomic_increment(&case_idx)){
        CcToken toks[MAX_TEST_TOKENS];
        int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = CC_LEX_STRING(test_cases[i].inp, toks, &count, &aa, &synth);
        TestAssertFalse(err);
        TEST_stats.executed++;
        if(count != test_cases[i].exp_count){
            TEST_stats.failures++;
            TestReport("test '%s' (line %d): expected %d tokens, got %d", test_cases[i].name, test_cases[i].line, test_cases[i].exp_count, count);
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
            continue;
        }
        for(int j = 0; j < count; j++){
            TEST_stats.executed++;
            if(!cc_tok_matches(toks[j], test_cases[i].exp[j])){
                TEST_stats.failures++;
                TestReport("test '%s' (line %d): token %d mismatch: got type=%s, expected type=%s", test_cases[i].name, test_cases[i].line, j, cc_type_name(toks[j].type), cc_type_name(test_cases[i].exp[j].type));
            }
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
    }
    TESTEND();
}

// Exercise both public token consumption paths with the same regression cases.
TestFunction(test_literal_regressions){
    TESTBEGIN();
    uint64_t infinity_bits = UINT64_C(0x7ff0000000000000);
    double infinity;
    memcpy(&infinity, &infinity_bits, sizeof infinity);
    struct { StringView input; CcToken expected; } cases[] = {
        {SV("0x1p1024"), cc_double_tok(infinity)},
        {SV("0x1.fffffffffffff8p1023"), cc_double_tok(infinity)},
        {SV("0x1p99999999999999999999"), cc_double_tok(infinity)},
        {SV("0x1p128f"), cc_float_tok((float)infinity)},
        {SV("0x1.ffffffp127f"), cc_float_tok((float)infinity)},
        {SV("0x1.8p2"), cc_double_tok(0x1.8p2)},
        {SV("0X1.FP+10"), cc_double_tok(0X1.FP+10)},
        {SV("0x6p0"), cc_double_tok(0x6p0)},
        {SV("0x.8p0"), cc_double_tok(0x.8p0)},
        {SV("0x1.p0"), cc_double_tok(0x1.p0)},
        {SV("0x1ep0"), cc_double_tok(0x1ep0)},
        {SV("0x0p999999999999999999999"), cc_double_tok(0.0)},
        {SV("0x1p-999999999999999999999"), cc_double_tok(0.0)},
        {SV("0x1p-1022"), cc_double_tok(0x1p-1022)},
        {SV("0x1p-1074"), cc_double_tok(0x1p-1074)},
        {SV("0x1p-1075"), cc_double_tok(0.0)},
        {SV("0x1.0000000000000001p-1075"), cc_double_tok(0x1.0000000000000001p-1075)},
        {SV("0x1.fffffffffffffp1023"), cc_double_tok(0x1.fffffffffffffp1023)},
        {SV("0x1.fffffffffffff8p0"), cc_double_tok(0x1.fffffffffffff8p0)},
        {SV("0x1.00000000000008p0"), cc_double_tok(0x1.00000000000008p0)},
        {SV("0x1.00000000000018p0"), cc_double_tok(0x1.00000000000018p0)},
        {SV("0x1.00000000000008000001p0"), cc_double_tok(0x1.00000000000008000001p0)},
        {SV("0x0.fffffffffffff8p-1022"), cc_double_tok(0x0.fffffffffffff8p-1022)},
        {SV("0x1.8p2f"), cc_float_tok(0x1.8p2f)},
        {SV("0X1P0F"), cc_float_tok(0X1P0F)},
        {SV("0x1p-126f"), cc_float_tok(0x1p-126f)},
        {SV("0x1p-149f"), cc_float_tok(0x1p-149f)},
        {SV("0x1p-150f"), cc_float_tok(0.0f)},
        {SV("0x1.000001p-150f"), cc_float_tok(0x1.000001p-150f)},
        {SV("0x1.fffffep127f"), cc_float_tok(0x1.fffffep127f)},
        {SV("0x1.ffffffp0f"), cc_float_tok(0x1.ffffffp0f)},
        {SV("0x1.000001p0f"), cc_float_tok(0x1.000001p0f)},
        {SV("0x1.000003p0f"), cc_float_tok(0x1.000003p0f)},
        {SV("0x1.00000100000000001p0f"), cc_float_tok(0x1.00000100000000001p0f)},
        {SV("0x0.ffffffp-126f"), cc_float_tok(0x0.ffffffp-126f)},
        {SV("0x1.abp3L"), cc_long_double_tok((uint64_t[2]){0x0000000000000000ULL, 0x4002ab0000000000ULL})},
        {SV("0x1.a'bp1'0"), cc_double_tok(0x1.abp10)},
        {SV("#define HEX 0x1.8p2f\nHEX"), cc_float_tok(6.0f)},
        {SV("#define CAT(a,b) a##b\nCAT(0x1p, 2)"), cc_double_tok(4.0)},
        {SV("U'\\x1234'"), cc_int_tok(0x1234, CC_CHAR32)},
        {SV("U'\\777'"), cc_int_tok(0777, CC_CHAR32)},
        {SV("u'\\x1234'"), cc_int_tok(0x1234, CC_CHAR16)},
        {SV("L'\\x1234'"), cc_int_tok(0x1234, CC_WCHAR)},
        {SV("U'α'"), cc_int_tok(0x3B1, CC_CHAR32)},
        {SV("u'α'"), cc_int_tok(0x3B1, CC_CHAR16)},
        {SV("U'😀'"), cc_int_tok(0x1F600, CC_CHAR32)},
        {SV("U'\\U0001f600'"), cc_int_tok(0x1F600, CC_CHAR32)},
        {SV("u8'\\xff'"), cc_int_tok(255, CC_UCHAR)},
        {SV("__calc(U'\\x1234')"), cc_int_tok(0x1234, CC_INT)},
        {SV("__calc(U'α')"), cc_int_tok(0x3B1, CC_INT)},
        {SV("__mixin(\"\\x31\")"), cc_int_tok(1, CC_INT)},
        {SV("__mixin(\"\\u0031\")"), cc_int_tok(1, CC_INT)},
        {SV("__mixin(\"\\U00000031\")"), cc_int_tok(1, CC_INT)},
        {SV("__mixin(\"\\40\" \"1\")"), cc_int_tok(1, CC_INT)},
        {SV("__mixin(u8\"123\")"), cc_int_tok(123, CC_INT)},
        {SV("__mixin(L\"123\")"), cc_int_tok(123, CC_INT)},
        {SV("__mixin(\"\\\"\\?\\\"\")"), cc_str_tok(CC_STRING, SV("?\0"))},
        {SV("__mixin(\"\\\"a\\\\nb\\\"\")"), cc_str_tok(CC_STRING, SV("a\nb\0"))},
        {SV("__format(\"%s2\", \"\\1\")"), cc_str_tok(CC_STRING, SV("\0012\0"))},
        {SV("__format(\"%sA\", \"\\x1\")"), cc_str_tok(CC_STRING, SV("\001A\0"))},
        {SV("__format(\"%s\", \"\\1\" \"23\")"), cc_str_tok(CC_STRING, SV("\00123\0"))},
        {SV("__format(\"\\1\" \"23\")"), cc_str_tok(CC_STRING, SV("\00123\0"))},
        {SV("__format(\"%\" \"s\", \"ok\")"), cc_str_tok(CC_STRING, SV("ok\0"))},
        {SV("__format(u8\"abc\")"), cc_str_tok(CC_STRING, SV("abc\0"))},
        {SV("__format(L\"%s\", U\"α\")"), cc_str_tok(CC_STRING, SV("α\0"))},
        {SV("__format(\"%s\", \"\\0\\\"\\\\\\n\")"), cc_str_tok(CC_STRING, SV("\0\"\\\n\0"))},
        {SV("__env(u8\"literal_\\164est\")"), cc_str_tok(CC_STRING, SV("C:\\new\\test\"\n\0"))},
        {SV("#define decoded 42\n__ident(u8\"de\\143oded\")"), cc_int_tok(42, CC_INT)},
        {SV("_Pragma(\"message(\\\"a\\\\nb\\\")\") 1"), cc_int_tok(1, CC_INT)},
        {SV("_Pragma(L\"once\") 1"), cc_int_tok(1, CC_INT)},
        {SV("_Pragma(\"message(\\\"a\\\\\\\"b\\\")\") 1"), cc_int_tok(1, CC_INT)},
        {SV("\"\\xff\" \"f\""), cc_str_tok(CC_STRING, SV("\377f\0"))},
        {SV("u\"\\x1234\""), cc_str16_tok(CC_uSTRING, (const unsigned short[]){0x1234, 0}, 2)},
        {SV("u\"😀\""), cc_str16_tok(CC_uSTRING, (const unsigned short[]){0xD83D, 0xDE00, 0}, 3)},
        {SV("U\"α\\U0001f600\""), cc_str32_tok(CC_USTRING, (const unsigned int[]){0x3B1, 0x1F600, 0}, 3)},
        {SV("U\"\\xffffffff\""), cc_str32_tok(CC_USTRING, (const unsigned int[]){UINT32_MAX, 0}, 2)},
        {SV("u\"\\xd800\""), cc_str16_tok(CC_uSTRING, (const unsigned short[]){0xD800, 0}, 2)},
    };
    for(int array = 0; array < 2; array++){
        static int case_idx[2] = {0};
        for(size_t i = test_atomic_increment(case_idx+array); i < arrlen(cases); i = test_atomic_increment(case_idx+array)){
            CcToken out[MAX_TEST_TOKENS]; int count = 0;
            ArenaAllocator aa = {0}, synth = {0};
            int err = cc_lex_string_mode(cases[i].input, &out, &count, &aa, &synth, __FILE__, __func__, __LINE__, array, 0, 1);
            TestExpectFalse(err);
            TestExpectEquals(int, count, 1);
            if(!err && count == 1){
                if(!cc_tok_matches(out[0], cases[i].expected))
                    TestReport("literal regression (%s): %.*s", array ? "array" : "stream", sv_p(cases[i].input));
                TestExpectTrue(cc_tok_matches(out[0], cases[i].expected));
            }
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
        }
    }
    StringView invalid[] = {
        SV("0x1.2"),
        SV("0x.p0"),
        SV("0xp0"),
        SV("0x1p"),
        SV("0x1p+"),
        SV("0x1p0u"),
        SV("0x1p0ff"),
        SV("0x1p0LL"),
        SV("0x1p0fL"),
        SV("0x1p0i32"),
        SV("0x1p0ui64"),
        SV("0x1p0junk"),
        SV("0x1.2.3p0"),
        SV("0x'1p0"),
        SV("0x1.'8p0"),
        SV("0x1'p0"),
        SV("0x1p'0"),
        SV("0x1p0'f"),
        SV("0x1p0''1"),
        SV("'\\x'"), SV("'\\u12'"), SV("'\\U1234'"), SV("'\\q'"),
        SV("\"\\x\""), SV("\"\\u12\""), SV("\"\\U1234\""), SV("\"\\q\""),
        SV("u\"\\x\""), SV("U\"\\u12\""), SV("L\"\\U1234\""),
        SV("'\\ud800'"), SV("U'\\U00110000'"), SV("u\"\\U00110000\""),
        SV("\"\\udfff\""), SV("U\"\\Uffffffff\""),
        SV("'\\U00100000\\U00100000\\U00100000'"),
        SV("'\\Uffffffff\\Uffffffff'"),
        SV("u8'é'"), SV("u'😀'"), SV("u'\\x10000'"), SV("'\\777'"),
        SV("u\"\\x10000\""), SV("\"\\x100\""), SV("U\"\\x100000000\""),
        SV("U'\\x100000000'"), SV("__calc(U'ab')"), SV("__calc('\\u12')"),
        SV("__mixin(\"\\x\")"), SV("__format(\"%s\", \"\\u12\")"),
        SV("\"\300\257\""), SV("u\"\355\240\200\""), SV("U\"\360\237\""),
    };
    for(int array = 0; array < 2; array++){
        static int invalid_idx[2] = {0};
        for(size_t i = test_atomic_increment(invalid_idx+array); i < arrlen(invalid); i = test_atomic_increment(invalid_idx+array)){
            CcToken out[MAX_TEST_TOKENS]; int count = 0;
            ArenaAllocator aa = {0}, synth = {0};
            int err = cc_lex_string_mode(invalid[i], &out, &count, &aa, &synth, __FILE__, __func__, __LINE__, array, 0, 1);
            if(!err) TestReport("expected invalid literal (%s): %.*s", array ? "array" : "stream", sv_p(invalid[i]));
            TestExpectTrue(err);
            ArenaAllocator_free_all(&aa);
            ArenaAllocator_free_all(&synth);
        }
    }
    static int short_wchar_idx = 0;
    for(int array = test_atomic_increment(&short_wchar_idx); array < 2; array = test_atomic_increment(&short_wchar_idx)){
        CcToken out[MAX_TEST_TOKENS]; int count = 0;
        ArenaAllocator aa = {0}, synth = {0};
        int err = cc_lex_string_mode(SV("L'\\x1234' L\"😀\""), &out, &count, &aa, &synth, __FILE__, __func__, __LINE__, array, 1, 1);
        TestExpectFalse(err);
        TestExpectEquals(int, count, 2);
        if(!err && count == 2){
            TestExpectEquals(uint64_t, out[0].constant.integer_value, 0x1234);
            TestExpectEquals(uint32_t, out[1].str.length, 3);
            TestExpectEquals(unsigned short, out[1].str.utf16[0], 0xD83D);
            TestExpectEquals(unsigned short, out[1].str.utf16[1], 0xDE00);
        }
        ArenaAllocator_free_all(&aa);
        ArenaAllocator_free_all(&synth);
    }
    TESTEND();
}

static int cpp_number_to_cc_tok(CppPreprocessor*, CppToken*, CcToken*);

// Check target representations directly, without host long-double literals.
TestFunction(test_long_double_targets){
    TESTBEGIN();
    struct { StringView text; uint64_t quad[2], x87[2], binary64; } cases[] = {
        {SV("3.14L"), {0xeb851eb851eb851fULL, 0x400091eb851eb851ULL}, {0xc8f5c28f5c28f5c3ULL, 0x4000}, 0x40091eb851eb851fULL},
        {SV("0x1.abp3L"), {0, 0x4002ab0000000000ULL}, {0xd580000000000000ULL, 0x4002}, 0x402ab00000000000ULL},
        {SV("1.0000000000000000000000000000000002L"), {1, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}, 0x3ff0000000000000ULL},
        {SV("0x1.0000000000000000000000000001p0L"), {1, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}, 0x3ff0000000000000ULL},
        {SV("0x1.0000000000000001p0L"), {0x1000000000000ULL, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}, 0x3ff0000000000000ULL},
        {SV("0x1.00000000000000010001p0L"), {0x1000100000000ULL, 0x3fff000000000000ULL}, {0x8000000000000001ULL, 0x3fff}, 0x3ff0000000000000ULL},
        {SV("0x1p-16494L"), {1, 0}, {0, 0}, 0},
        {SV("0x1p-16495L"), {0, 0}, {0, 0}, 0},
        {SV("0x1.00000000000000000000000000001p-16495L"), {1, 0}, {0, 0}, 0},
        {SV("0x1p-16445L"), {0x2000000000000ULL, 0}, {1, 0}, 0},
        {SV("0x1p16384L"), {0, 0x7fff000000000000ULL}, {0x8000000000000000ULL, 0x7fff}, 0x7ff0000000000000ULL},
        {SV("1e4000L"), {0x18c21ab905450cc3ULL, 0x73e6a3750647fcabULL}, {0xd1ba8323fe558c61ULL, 0x73e6}, 0x7ff0000000000000ULL},
        {SV("1e-4000L"), {0x0b8049732d11a23dULL, 0x0c17387ae70c9e70ULL}, {0x9c3d73864f3805c0ULL, 0xc17}, 0x0000000000000000ULL},
        {SV("0x1.ffffffffffffffffffffffffffff8p0L"), {0x0000000000000000ULL, 0x4000000000000000ULL}, {0x8000000000000000ULL, 0x4000}, 0x4000000000000000ULL},
        {SV("0x0.ffffffffffffffffffffffffffff8p-16382L"), {0x0000000000000000ULL, 0x0001000000000000ULL}, {0x8000000000000000ULL, 0x1}, 0x0000000000000000ULL},
        {SV("0x1.fffffffffffffffep0L"), {0xfffe000000000000ULL, 0x3fffffffffffffffULL}, {0xffffffffffffffffULL, 0x3fff}, 0x4000000000000000ULL},
        {SV("0.0L"), {0, 0}, {0, 0}, 0},
        {SV("1e6000L"), {0, 0x7fff000000000000ULL}, {0x8000000000000000ULL, 0x7fff}, 0x7ff0000000000000ULL},
        {SV("1e-6000L"), {0, 0}, {0, 0}, 0},
    };
    CcLongDoubleFormat formats[] = {CC_LONG_DOUBLE_BINARY128, CC_LONG_DOUBLE_X87, CC_LONG_DOUBLE_BINARY64};
    for(size_t f = 0; f < arrlen(formats); f++){
        CppPreprocessor cpp = {.target = cc_target_test()};
        cpp.target.long_double_format = formats[f];
        static int case_idx[3] = {0};
        for(size_t i = test_atomic_increment(case_idx+f); i < arrlen(cases); i = test_atomic_increment(case_idx+f)){
            CppToken input = {.txt = cases[i].text};
            CcToken out;
            int err = cpp_number_to_cc_tok(&cpp, &input, &out);
            TestExpectFalse(err);
            TestExpectEquals(int, out.constant.ctype, CC_LONG_DOUBLE);
            uint64_t words[2] = {0};
            if(formats[f] == CC_LONG_DOUBLE_BINARY128){
                memcpy(words, &out.constant.quad_value, 16);
                TestExpectEquals(uint64_t, words[0], cases[i].quad[0]);
                TestExpectEquals(uint64_t, words[1], cases[i].quad[1]);
            }
            else if(formats[f] == CC_LONG_DOUBLE_X87){
                memcpy(words, &out.constant.x87_value, 10);
                TestExpectEquals(uint64_t, words[0], cases[i].x87[0]);
                TestExpectEquals(uint64_t, words[1], cases[i].x87[1]);
            }
            else {
                memcpy(words, &out.constant.double_value, 8);
                TestExpectEquals(uint64_t, words[0], cases[i].binary64);
            }
        }
    }
    TESTEND();
}

// Verify predefined limits by their target bits, including subnormal minima.
TestFunction(test_long_double_macros){
    TESTBEGIN();
    struct {
        uint64_t bits[4][2];
        StringView properties;
    } expected[] = {
        [CC_LONG_DOUBLE_BINARY64] = {
            {{0x7fefffffffffffffULL, 0}, {0x0010000000000000ULL, 0},
             {0x3cb0000000000000ULL, 0}, {1, 0}},
            SV("53 15 -1021 1024 -307 308 1 1 1 17"),
        },
        [CC_LONG_DOUBLE_X87] = {
            {{0xffffffffffffffffULL, 0x7ffe}, {0x8000000000000000ULL, 1},
             {0x8000000000000000ULL, 0x3fc0}, {1, 0}},
            SV("64 18 -16381 16384 -4931 4932 1 1 1 21"),
        },
        [CC_LONG_DOUBLE_BINARY128] = {
            {{0xffffffffffffffffULL, 0x7ffeffffffffffffULL}, {0, 0x0001000000000000ULL},
             {0, 0x3f8f000000000000ULL}, {1, 0}},
            SV("113 33 -16381 16384 -4931 4932 1 1 1 36"),
        },
    };
    StringView source = SV(
        "__LDBL_MAX__ __LDBL_MIN__ __LDBL_EPSILON__ __LDBL_DENORM_MIN__ "
        "__LDBL_MANT_DIG__ __LDBL_DIG__ __LDBL_MIN_EXP__ __LDBL_MAX_EXP__ "
        "__LDBL_MIN_10_EXP__ __LDBL_MAX_10_EXP__ __LDBL_HAS_DENORM__ "
        "__LDBL_HAS_INFINITY__ __LDBL_HAS_QUIET_NAN__ __DECIMAL_DIG__");
    static int idx = 0;
    for(size_t target = test_atomic_increment(&idx); target < CC_TARGET_COUNT; target=test_atomic_increment(&idx)){
        ArenaAllocator aa = {0};
        Allocator a = allocator_from_arena(&aa);
        AtomTable at = {.allocator = a};
        FileCache* fc = fc_create(a);
        CppPreprocessor cpp = {
            .allocator = a, .at = &at, .fc = fc,
            .target = cc_target_funcs[target](),
        };
        CcLongDoubleFormat format = cpp.target.long_double_format;
        fc_write_path(fc, "(test)", 6);
        int err = fc_cache_file(fc, source);
        TestExpectFalse(err);
        err = cpp_define_builtin_macros(&cpp);
        TestExpectFalse(err);
        err = cpp_include_file_via_file_cache(&cpp, SV("(test)"));
        TestExpectFalse(err);
        for(size_t i = 0; i < 4; i++){
            CcToken tok = {0};
            err = cpp_next_c_token(&cpp, &tok);
            TestExpectFalse(err);
            TestExpectEquals(int, tok.type, CC_CONSTANT);
            if(err || tok.type != CC_CONSTANT) continue;
            TestExpectEquals(int, tok.constant.ctype, CC_LONG_DOUBLE);
            uint64_t bits[2] = {0};
            switch(format){
                case CC_LONG_DOUBLE_BINARY64: memcpy(bits, &tok.constant.double_value, 8); break;
                case CC_LONG_DOUBLE_X87: memcpy(bits, &tok.constant.x87_value, 10); break;
                case CC_LONG_DOUBLE_BINARY128: memcpy(bits, &tok.constant.quad_value, 16); break;
            }
            TestExpectEquals(uint64_t, bits[0], expected[format].bits[i][0]);
            TestExpectEquals(uint64_t, bits[1], expected[format].bits[i][1]);
        }
        MStringBuilder properties = {.allocator = a};
        for(;;){
            CppToken tok;
            err = cpp_next_pp_token(&cpp, &tok);
            TestExpectFalse(err);
            if(err || tok.type == CPP_EOF) break;
            if(tok.type == CPP_WHITESPACE || tok.type == CPP_NEWLINE) continue;
            if(properties.cursor) msb_write_char(&properties, ' ');
            msb_write_str(&properties, tok.txt.text, tok.txt.length);
        }
        test_expect_equals_sv(expected[format].properties, msb_borrow_sv(&properties),
            "expected properties", "properties", &TEST_stats, __FILE__, __func__, __LINE__);
        ArenaAllocator_free_all(&cpp.synth_arena);
        ArenaAllocator_free_all(&aa);
    }
    TESTEND();
}

TestFunction(test_fast_float_wide){
    TESTBEGIN();
    struct { StringView text; uint64_t quad[2], x87[2]; } cases[] = {
        {SV("3.14"), {0xeb851eb851eb851fULL, 0x400091eb851eb851ULL}, {0xc8f5c28f5c28f5c3ULL, 0x4000}},
        {SV("-3.14"), {0xeb851eb851eb851fULL, 0xc00091eb851eb851ULL}, {0xc8f5c28f5c28f5c3ULL, 0xc000}},
        {SV("-0"), {0, 0x8000000000000000ULL}, {0, 0x8000}},
        {SV("+1.5"), {0, 0x3fff800000000000ULL}, {0xc000000000000000ULL, 0x3fff}},
        {SV("1e4000"), {0x18c21ab905450cc3ULL, 0x73e6a3750647fcabULL}, {0xd1ba8323fe558c61ULL, 0x73e6}},
        {SV("1e-4000"), {0x0b8049732d11a23dULL, 0x0c17387ae70c9e70ULL}, {0x9c3d73864f3805c0ULL, 0xc17}},
        {SV("1e999999"), {0, 0x7fff000000000000ULL}, {0x8000000000000000ULL, 0x7fff}},
        {SV("-1e-999999"), {0, 0x8000000000000000ULL}, {0, 0x8000}},
        {SV("inf"), {0, 0x7fff000000000000ULL}, {0x8000000000000000ULL, 0x7fff}},
        {SV("-infinity"), {0, 0xffff000000000000ULL}, {0x8000000000000000ULL, 0xffff}},
        {SV("nan(payload)"), {0, 0x7fff800000000000ULL}, {0xc000000000000000ULL, 0x7fff}},
        {SV("1.00000000000000000000000000000000009629649721936179265279889712924636592690508241076940976199693977832794189453125"), {0x0000000000000000ULL, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}},
        {SV("1.00000000000000000000000000000000028888949165808537795839669138773909778071524723230822928599081933498382568359375"), {0x0000000000000002ULL, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}},
        {SV("1.99999999999999999999999999999999990370350278063820734720110287075363407309491758923059023800306022167205810546875"), {0x0000000000000000ULL, 0x4000000000000000ULL}, {0x8000000000000000ULL, 0x4000}},
        // Exact halfway values: even significand wins, including carry to 2.
        {SV("1.0000000000000000000542101086242752217003726400434970855712890625"), {0x1000000000000ULL, 0x3fff000000000000ULL}, {0x8000000000000000ULL, 0x3fff}},
        {SV("1.9999999999999999999457898913757247782996273599565029144287109375"), {0xffff000000000000ULL, 0x3fffffffffffffffULL}, {0x8000000000000000ULL, 0x4000}},
    };
    struct { StringView text; float f; double d; } small[] = {
        {SV("0"), 0.0f, 0.0},
        {SV("0.1"), 0.1f, 0.1},
        {SV("1.000000059604644775390625"), 1.0f, 0x1.000001p0},
        {SV("1.000000059604644775390626"), 0x1.000002p0f, 0x1.000001p0},
        {SV("1.00000000000000011102230246251565404236316680908203125"), 1.0f, 1.0},
        {SV("1.99999999999999988897769753748434595763683319091796875"), 2.0f, 2.0},
        {SV("1e-45"), 0x1p-149f, 1e-45},
        {SV("5e-324"), 0.0f, 0x1p-1074},
        {SV("1e-400"), 0.0f, 0.0},
        {SV("1e400"), HUGE_VALF, HUGE_VAL},
    };
    {
        static int idx = 0;
        for(size_t i = test_atomic_increment(&idx); i < arrlen(small); i = test_atomic_increment(&idx)){
            StringView sv = small[i].text;
            float f;
            double d;
            fast_float_to_float_float(0, fast_float_parse_long_mantissa_float(sv.text, sv.text+sv.length), &f);
            fast_float_to_float_double(0, fast_float_parse_long_mantissa_double(sv.text, sv.text+sv.length), &d);
            TestExpectEquals(float, f, small[i].f);
            TestExpectEquals(double, d, small[i].d);
        }
    }
    for(int x87 = 0; x87 < 2; x87++){
        {
            static int idx[2] = {0};
            for(size_t i = test_atomic_increment(idx+x87); i < arrlen(cases); i = test_atomic_increment(idx+x87)){
                StringView sv = cases[i].text;
                uint64_t words[2] = {0};
                fast_float_from_chars_result r = x87
                    ? fast_float_from_chars_x87(sv.text, sv.text+sv.length, words, FASTFLOAT_FORMAT_GENERAL)
                    : fast_float_from_chars_binary128(sv.text, sv.text+sv.length, words, FASTFLOAT_FORMAT_GENERAL);
                TestExpectEquals(int, r.error, FASTFLOAT_NO_ERROR);
                TestExpectTrue(r.ptr == sv.text+sv.length);
                TestExpectEquals(uint64_t, words[0], x87 ? cases[i].x87[0] : cases[i].quad[0]);
                TestExpectEquals(uint64_t, words[1], x87 ? cases[i].x87[1] : cases[i].quad[1]);
            }
        }
        struct { StringView text; enum fast_float_chars_format fmt; int error, consumed; } syntax[] = {
            {SV(""), FASTFLOAT_FORMAT_GENERAL, FASTFLOAT_INVALID_VALUE, 0},
            {SV("."), FASTFLOAT_FORMAT_GENERAL, FASTFLOAT_INVALID_VALUE, 0},
            {SV("1.5"), (enum fast_float_chars_format)0, FASTFLOAT_BAD_FORMAT, 0},
            {SV("1.5tail"), FASTFLOAT_FORMAT_GENERAL, FASTFLOAT_NO_ERROR, 3},
            {SV("1.5e+"), FASTFLOAT_FORMAT_GENERAL, FASTFLOAT_NO_ERROR, 3},
            {SV("1.5e2"), FASTFLOAT_FORMAT_FIXED, FASTFLOAT_NO_ERROR, 3},
            {SV("1.5"), FASTFLOAT_FORMAT_SCIENTIFIC, FASTFLOAT_INVALID_VALUE, 0},
            {SV("1.5e0"), FASTFLOAT_FORMAT_SCIENTIFIC, FASTFLOAT_NO_ERROR, 5},
        };
        {
            static int idx[2] = {0};
            for(size_t i = test_atomic_increment(idx+x87); i < arrlen(syntax); i = test_atomic_increment(idx+x87)){
                StringView sv = syntax[i].text;
                uint64_t words[2] = {42, 43};
                fast_float_from_chars_result r = x87
                    ? fast_float_from_chars_x87(sv.text, sv.text+sv.length, words, syntax[i].fmt)
                    : fast_float_from_chars_binary128(sv.text, sv.text+sv.length, words, syntax[i].fmt);
                TestExpectEquals(int, r.error, syntax[i].error);
                TestExpectTrue(r.ptr == sv.text+syntax[i].consumed);
                if(r.error){
                    TestExpectEquals(uint64_t, words[0], 42);
                    TestExpectEquals(uint64_t, words[1], 43);
                }
                else {
                    TestExpectEquals(uint64_t, words[0], x87 ? 0xc000000000000000ULL : 0);
                    TestExpectEquals(uint64_t, words[1], x87 ? 0x3fff : 0x3fff800000000000ULL);
                }
            }
        }
    }
    {
        static int idx = 0;
        for(int job = test_atomic_increment(&idx); job < 8; job = test_atomic_increment(&idx)){
            int x87 = job/4;
            int variant = job%4;
            // Construct the exact midpoint between zero and the least subnormal:
            // 2^-n = 5^n * 10^-n. Test below, at, and above it without host floats.
            int power = x87 ? 16446 : 16495;
            // Base 10^9 and factors up to 5^13 avoid ~190 million single-digit
            // iterations under the interpreter. Products stay below 2^61.
            uint32_t chunks[1300] = {1}; // 5^16495 has 11530 digits: 1282 chunks.
            int nchunks = 1;
            for(int remaining = power; remaining;){
                int step = remaining < 13 ? remaining : 13;
                uint32_t factor = 1;
                for(int i = 0; i < step; i++) factor *= 5;
                uint64_t carry = 0;
                for(int j = 0; j < nchunks; j++){
                    uint64_t v = (uint64_t)chunks[j]*factor + carry;
                    chunks[j] = (uint32_t)(v%1000000000);
                    carry = v/1000000000;
                }
                while(carry){
                    chunks[nchunks++] = (uint32_t)(carry%1000000000);
                    carry /= 1000000000;
                }
                remaining -= step;
            }
            MStringBuilder sb = {.allocator=MALLOCATOR};
            msb_sprintf(&sb, "%u", chunks[nchunks-1]);
            for(size_t i = nchunks-1; i--;)
                msb_sprintf(&sb, "%09u", chunks[i]);
            if(variant < 3){
                msb_erase(&sb, 1);
                int offset = variant-1;
                msb_write_char(&sb, (char)('5'+offset));
                msb_sprintf(&sb, "e-%d", power);
            }
            else {
                // A nonzero digit beyond the retained buffer must break the tie too.
                msb_write_nchar(&sb, '0', 199);
                msb_write_char(&sb, '1');
                msb_sprintf(&sb, "e-%d", power+200);
            }
            if(sb.errored){
                msb_destroy(&sb);
                EndTest("OOM");
            }
            StringView sv = msb_borrow_sv(&sb);
            size_t length = sv.length;
            const char* text = sv.text;
            uint64_t words[2];
            fast_float_from_chars_result r = x87
                ? fast_float_from_chars_x87(text, text+length, words, FASTFLOAT_FORMAT_GENERAL)
                : fast_float_from_chars_binary128(text, text+length, words, FASTFLOAT_FORMAT_GENERAL);
            TestExpectEquals(int, r.error, FASTFLOAT_NO_ERROR);
            if(variant < 3) TestExpectTrue(r.ptr == text+length);
            TestExpectEquals(uint64_t, words[0], variant >= 2 ? 1 : 0);
            TestExpectEquals(uint64_t, words[1], 0);
            msb_destroy(&sb);
        }
    }
    TESTEND();
}

int main(int argc, char** argv){
    #ifdef USE_TESTING_ALLOCATOR
    testing_allocator_init();
    #endif
    RegisterTestFlags(test_cc_lex_integers,     TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_floats,       TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_long_double_macros, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_long_double_targets, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_fast_float_wide,     TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_chars,        TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_strings,      TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_literal_regressions, TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_punctuators,  TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_keywords,     TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    RegisterTestFlags(test_cc_lex_multi_token,  TEST_CASE_FLAGS_DUPLICATE_FOR_EACH_THREAD);
    int err = test_main(argc, argv, NULL);
    #ifdef USE_TESTING_ALLOCATOR
    testing_assert_all_freed();
    #endif
    return err;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#include "../Drp/Allocators/allocator.c"
#include "../Drp/file_cache.c"
#include "cpp_preprocessor.c"
